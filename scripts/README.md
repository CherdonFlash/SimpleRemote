# 项目脚本

这里集中保存 SimpleRemote 设计和校核过程中由 AI 辅助生成的自动化脚本源码，避免脚本、编译产物与正式硬件文件混放。

## firmware

- `Measure-Runtime.ps1`：从当前AXF解析运行统计变量地址，使用ST-Link HotPlug读取两个时间窗口内的无线发送尝试/ACK/失败计数、OLED完整帧数及电量计成功读取数。不暂停CPU、不复位、不烧录。
- 运行示例：`./scripts/firmware/Measure-Runtime.ps1 -Seconds 30 -Windows 2 -Serial 0F381212A218303030303032`。
- 运行前必须烧录与当前AXF对应的固件；若本机工具路径不同，用`-Programmer`、`-FromElf`参数覆盖。输出为每个窗口一行JSON。
- 时间依据MCU的SysTick；分块读内存不是原子快照，边界处进行中的发送可能使尝试数和完成数差1，结果用于近似运行速率，不替代逻辑分析仪或端到端延迟测试。

## mechanical

- `BuildLatestEnclosure.cs`：按 2026-09-13 PCB 模型和 35.4×33.5mm OLED 构建外壳，通过 SolidWorks COM API 调用，无桌面操作。
- `BuildProductAssembly.cs`：生成产品装配体及 2400×1600 装配图，PCB 顶面位于 Z=17.6mm。
- `AnalyzeObjMesh.py`、`CheckMeshClearance.py`：分析 ZIP 中的 OBJ 网格，并以顶点和面重心采样检查穿壳情况；后者依赖 NumPy，不能替代实体干涉及实物试装。
- `AnalyzeNewPcbStep.py`：读取 STEP 实体关系并提取 PCB 与器件的机械包络。
- `BuildRemoteEnclosure.cs`：生成或修改遥控器上壳结构。
- `BuildShellAssembly.cs`、`BuildCorrectObjValidation.cs`：构建外壳及 PCB 对位校核装配体。
- `CheckAssemblyInterference.cs`、`CheckShellBodies.cs`、`SwInspector.cs`：检查装配干涉、实体状态和模型信息。
- `BuildNewPcbValidation.cs`、`BuildRemoteAssembly.cs`：早期基准模型的重建与校核源码，保留用于设计追溯。

Python 脚本可直接通过命令行运行。C# 脚本依赖本机 SolidWorks 2024 COM 接口及对应 Interop 程序集；生成的 EXE、DLL、PDB 和临时结果不纳入 Git。
