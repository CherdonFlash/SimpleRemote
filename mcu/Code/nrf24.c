#include "nrf24.h"
#include "key.h"
#include "turn.h"
#include "bat.h"
#define CS_Set(x) (x ? GPIO_SetBits(GPIOB, GPIO_Pin_9) : GPIO_ResetBits(GPIOB, GPIO_Pin_9)) //拉低开始通信
#define CE_Set(x) (x ? GPIO_SetBits(GPIOB, GPIO_Pin_8) : GPIO_ResetBits(GPIOB, GPIO_Pin_8)) //收发引脚  
#define IRQ_Read() GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_15)

#define TX_ADR_WIDTH    5     //5字节地址宽度
#define RX_ADR_WIDTH    5     //5字节地址宽度
#define TX_PLOAD_WIDTH  32    //32字节有效数据宽度
#define RX_PLOAD_WIDTH  32    //32字节有效数据宽度

uint8_t Buf[32]={0};//数据包
uint8_t head = 0x51;//帧头
uint8_t end = 0x15;//帧尾
//摇杆数据
extern int8_t mobile_1;
extern int8_t mobile_2;
extern int8_t mobile_3;
extern int8_t mobile_4;//油门0-255
extern float V_Bat;//电池电压 实际值
void NRF_SendAll(){
    Buf[0] = head;

    Buf[1] = mobile_1;//摇杆
    Buf[2] = mobile_2;
    Buf[3] = mobile_3;
    Buf[4] = mobile_4;
    
    uint8_t io1=(Key_GetState(1) !=0);
    uint8_t io2=(Key_GetState(2) !=0);
    uint8_t io3=(Key_GetState(3) !=0);
    uint8_t io4=(Key_GetState(4) !=0);
    
    Buf[5] = (io1 << 0) |(io2 << 1) |(io3 << 2) |(io4 << 3);//低四位依次是四个按键的电平状态
    
    Buf[6] = (uint8_t)turn_get();//单刀双掷开关状态
    
    Buf[7] = (uint8_t)((uint16_t)(V_Bat*1000)>>8); //电池电压高八位
    
    Buf[8] = (uint8_t)((uint16_t)(V_Bat*1000)& 0xFF); //电池电压低八位
    
    Buf[31] = end;


    
    nrf24_send(Buf);//发送数据包
    
    // 打印整个数据包
    UART1_Printf("Send Buf: ");
    for(int i = 0; i < 32; i++)
    {
        UART1_Printf("%d,", Buf[i]);
    }
    UART1_Printf("\r\n");
}
//设置发送的地址和接受的地址
const uint8_t TX_ADDRESS[TX_ADR_WIDTH]={0x1F, 0xFF, 0xFF, 0xFF, 0x1F}; 
const uint8_t RX_ADDRESS[RX_ADR_WIDTH]={0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; 
void nrf24_init(void){
    
    nrf24_gpio();
    while(!nrf24_check());//自检死循环
    nrf24_TX_init();
    
}

uint8_t nrf24_send(uint8_t *Buf){
uint8_t State;
    uint16_t timeout = 0xFFFF; // 防止死机

    /* 1. 确保 CE 为 0，进入待命状态 */
    CE_Set(0);												

    /* 2. 将数据写入 TX FIFO */
    NRF_Write_Buf(WR_TX_PLOAD, Buf, TX_PLOAD_WIDTH);	

    /* 3. 脉冲拉高 CE，触发无线电发射 (手册要求 >10us) */
    CE_Set(1);												
    // 虽然单片机指令执行需要时间，但为了绝对规范，加一点点延时
    // 如果没有 Delay_us 函数，写个简单的 for 循环空跑几十次也行
    
    /* 4. 等待发送完成 (TX_DS) 或 达到最大重发次数 (MAX_RT) 中断 */
    while(IRQ_Read() == 1)
    {
        timeout--;
        if(timeout == 0) break; // 超时强行退出
    }									
    
    /* 5. 发送完毕，立刻拉低 CE 回到待机模式 */
    CE_Set(0);

    /* 6. 读取并强制清除所有的中断标志位 */
    State = NRF_Read_Reg(STATUS);  					
    NRF_Write_Reg(nRF_WRITE_REG + STATUS, State | 0x70); 		

    /* 7. 判断发送结果 */
    if(State & MAX_TX)				 						
    {
        // ? 发送失败（没人应答），必须清空发送缓冲区
        // 【关键修复】：使用单字节指令！杜绝错位！
        NRF_Write_Cmd(0xE1); 
        return MAX_TX;
    }
    
    if(State & TX_OK)	
    {
        // ? 发送成功（收到了接收端的应答）
        return TX_OK;
    }

    return 0; // 其他异常
}

uint8_t nrf24_get(uint8_t *Buf)
{
    uint8_t State;

    /* --- 1. 进入接收模式 --- */
    CE_Set(0);
    
    NRF_Write_Reg(nRF_WRITE_REG + CONFIG, 0x0F); // PWR_UP=1, PRIM_RX=1, CRC=2字节

    CE_Set(1);
    //
    /* --- 2. 等待接收中断 --- */
    while (IRQ_Read() == 1);

    
    /* --- 3. 读取状态寄存器 --- */
	State = NRF_Read_Reg(STATUS);  		
    NRF_Write_Reg(nRF_WRITE_REG + STATUS, State); 	

    /* --- 4. 判断是否接收完成 --- */
    if (State & RX_OK)   // RX_DR
    {
        /* --- 5. 读取接收数据 --- */
        NRF_Read_Buf(RD_RX_PLOAD, Buf, RX_PLOAD_WIDTH);

        /* --- 6. 清除 RX 中断标志 --- */
        NRF_Write_Reg(nRF_WRITE_REG + STATUS, RX_OK);

        /* --- 7. 清 RX FIFO，防止残留 --- */
        NRF_Write_Reg(FLUSH_RX, NOP);

        return RX_OK;    // 接收成功
    }

    /* --- 8. 异常情况，清所有中断 --- */
    NRF_Write_Reg(nRF_WRITE_REG + STATUS, State);

    return 0;
}
//NRF24收发配置初始化
void nrf24_TX_init(){
    CE_Set(0);
// 设置发送地址
    NRF_Write_Buf(0x20 | TX_ADDR, (uint8_t*)TX_ADDRESS, TX_ADR_WIDTH);
    // 【关键】：发送端为了接收 ACK，通道 0 的接收地址必须和 TX 地址一模一样！
    NRF_Write_Buf(0x20 | RX_ADDR_P0, (uint8_t*)TX_ADDRESS, TX_ADR_WIDTH);
    
    // 启用自动应答和通道 0
    NRF_Write_Reg(0x20 | EN_AA, 0x01); 
    NRF_Write_Reg(0x20 | EN_RXADDR, 0x01); 
    
    // 设置自动重传：重发间隔 500us (0x01<<4), 最多重发 10 次 (0x0A)
    NRF_Write_Reg(0x20 | SETUP_RETR, 0x1A);
    
    // 射频参数设置 (收发必须一致)
    NRF_Write_Reg(0x20 | RF_CH, 0); 
    NRF_Write_Reg(0x20 | RF_SETUP, 0x0F); 
    
    // 配置寄存器: PWR_UP=1, CRCO=1, EN_CRC=1, PRIM_RX=0 (发送模式!) -> 0x0E
    NRF_Write_Reg(0x20 | CONFIG, 0x0E); 
    
    // 清空内部残留
    NRF_Write_Cmd(0xE1); // FLUSH_TX 清空发送缓冲区
    NRF_Write_Reg(0x20 | STATUS, 0x70); // 清除所有中断标志
    CE_Set(0);
}
//检查nrf24l01是否存在
uint8_t nrf24_check(){
Delay_ms(100);
    uint8_t check_in_buf[5] = {0x11 ,0x22, 0x33, 0x44, 0x55};
	uint8_t check_out_buf[5] = {0x01};
    
    NRF_Write_Buf(0x20 | 0x10, check_in_buf, 5);//写寄存器
    NRF_Read_Buf(0x00 | 0x10, check_out_buf, 5);//读寄存器
    //UART1_Printf("data nrf %d \n",check_out_buf[2]);
    if(check_out_buf[0]==0x11 && check_out_buf[1]==0x22 &&check_out_buf[2]==0x33 &&check_out_buf[3]==0x44 &&check_out_buf[4]==0x55){
        //nrf24存在
        UART1_Printf("success data nrf %d \n",check_out_buf[2]);
        return 1;
    }else {
        //不存在
        UART1_Printf("erroe data nrf %d \n",check_out_buf[2]);
        return 0;
    }
}
void nrf24_gpio(){
    
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    /* --- 1. 开启时钟 --- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_AFIO  |
                           RCC_APB2Periph_SPI1, ENABLE);

    /* --- 2. 关闭 JTAG，释放 PA15 --- */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    /* --- 3. 配置 PA15 为 NRF24 IRQ --- */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;        // 建议使用上拉
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* --- 4. 配置 NRF24 的 CE(PB8) 和 CSN(PB9) --- */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOB, GPIO_Pin_8);  // CE = 0
    GPIO_SetBits(GPIOB, GPIO_Pin_9);    // CSN = 1

    /* --- 5. 配置硬件 SPI1 使用 PA5,6,7 --- */
    GPIO_PinRemapConfig(GPIO_Remap_SPI1, DISABLE);

    // PA5 = SCK，PA7 = MOSI
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA6 = MISO
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* --- 6. SPI1 初始化参数 --- */
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;

    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;  // 建议 4.5MHz
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;

    SPI_Init(SPI1, &SPI_InitStructure);

    /* --- 7. 使能 SPI1 --- */
    SPI_Cmd(SPI1, ENABLE);
}

// 往寄存器写多个字节
void NRF_Write_Buf(uint8_t reg, uint8_t *pBuf, uint8_t len)
{
    uint8_t i;
    // 片选拉低
    CS_Set(0);
    // 发送寄存器地址（最高位=0 表示写）
    SPI1_ReadWriteByte(reg);
    // 连续发送数据
    for (i = 0; i < len; i++)
    {
        SPI1_ReadWriteByte(pBuf[i]);
    }
    // 片选拉高
    CS_Set(1);
}

// 从寄存器读多个字节
void NRF_Read_Buf(uint8_t reg, uint8_t *pBuf, uint8_t len)
{
    uint8_t i;
    // 片选拉低
    CS_Set(0);
    // 发送寄存器地址（最高位=1 表示读）
    SPI1_ReadWriteByte(reg);
    // 连续读取数据（每次发送虚拟字节）
    for (i = 0; i < len; i++)
    {
        pBuf[i] = SPI1_ReadWriteByte(0xFF);
    }
    // 片选拉高
    CS_Set(1);
}

// 写一个字节到寄存器
void NRF_Write_Reg(uint8_t reg, uint8_t data)
{
    // 片选拉低
    CS_Set(0);
    // 发送寄存器地址（最高位=0 表示写）
    SPI1_ReadWriteByte(reg);
    // 发送数据
    SPI1_ReadWriteByte(data);

    // 片选拉高
    CS_Set(1);;
}

// 从寄存器读一个字节
uint8_t NRF_Read_Reg(uint8_t reg)
{
    uint8_t val;

    // 片选拉低
    CS_Set(0);

    // 发送寄存器地址（最高位=1 表示读）
    SPI1_ReadWriteByte( reg);
    // 读取数据（发送一个虚拟字节）
    val = SPI1_ReadWriteByte(0xFF);

    // 片选拉高
    CS_Set(1);

    return val;
}
// SPI 发送并接收一个字节
uint8_t SPI1_ReadWriteByte(uint8_t TxData)
{
    // 等待发送缓冲区空
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, TxData);

    // 等待接收缓冲区非空
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI1);
}

// 发送单字节指令（如 FLUSH_RX, FLUSH_TX, NOP 等）
// 返回值：执行指令时同步读回的 STATUS 状态寄存器值
uint8_t NRF_Write_Cmd(uint8_t cmd)
{
    uint8_t status;
    
    // 片选拉低
    CS_Set(0);
    
    // 发送 1 个字节指令，同时 NRF24L01 会自动返回 STATUS 的值
    status = SPI1_ReadWriteByte(cmd);
    
    // 片选立刻拉高
    CS_Set(1);
    
    return status;
}








