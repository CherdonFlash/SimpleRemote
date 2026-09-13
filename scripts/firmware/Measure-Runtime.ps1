param(
    [ValidateRange(5, 60)][int]$Seconds = 30,
    [ValidateRange(1, 5)][int]$Windows = 2,
    [string]$Serial = '0F381212A218303030303032',
    [string]$Programmer = 'D:\SoftWare\stm32IDE\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304\tools\bin\STM32_Programmer_CLI.exe',
    [string]$FromElf = 'D:\SoftWare\stm32Keil\ARM\ARMCC\Bin\fromelf.exe'
)

# 仅HotPlug读取；不复位、不暂停CPU、不写寄存器。需先烧录与当前AXF匹配的固件。
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$axf = Join-Path $repo 'mcu\Objects\stm32f103c8t6.axf'
$symbols = (& $FromElf --text -s $axf) -join "`n"
if ($LASTEXITCODE -ne 0) { throw '读取AXF符号失败' }
$addresses = @{}
foreach ($name in @('systick_ms', 'g_nrf_diag', 'g_iic_diag', 'g_cw2015_diag')) {
    $match = [regex]::Match($symbols, '(?m)^\s*\d+\s+' + $name + '\s+0x([0-9a-fA-F]+)')
    if (-not $match.Success) { throw "未找到符号 $name" }
    $addresses[$name] = [Convert]::ToUInt32($match.Groups[1].Value, 16)
}

function Read-Snapshot {
    $argsList = @('-c', 'port=SWD', "sn=$Serial", 'mode=HotPlug')
    foreach ($block in @(@('systick_ms', 4), @('g_nrf_diag', 40), @('g_iic_diag', 20), @('g_cw2015_diag', 28), @('systick_ms', 4))) {
        $argsList += @('-r32', ('0x{0:X8}' -f $addresses[$block[0]]), [string]$block[1])
    }
    $output = (& $Programmer @argsList 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw $output }
    if ($output -notmatch 'Device ID\s*:\s*0x412') { throw '目标不是预期F103低容量器件，停止读取统计。' }
    $words = @{}
    $ticks = @()
    foreach ($line in [regex]::Matches($output, '(?m)^0x([0-9A-Fa-f]+)\s*:\s*((?:[0-9A-Fa-f]{8}(?:[ \t]+|$))+)')) {
        $address = [Convert]::ToUInt32($line.Groups[1].Value, 16)
        $offset = 0
        foreach ($word in [regex]::Matches($line.Groups[2].Value, '[0-9A-Fa-f]{8}')) {
            $value = [Convert]::ToUInt32($word.Value, 16)
            $words[[uint32]($address + $offset)] = $value
            if ($address + $offset -eq $addresses['systick_ms']) { $ticks += $value }
            $offset += 4
        }
    }
    if ($ticks.Count -ne 2 -or $ticks[1] -lt $ticks[0]) { throw '节拍数据不完整或读取期间复位/回绕' }
    $result = [ordered]@{ tick_ms = ([double]$ticks[0] + $ticks[1]) / 2; read_span_ms = $ticks[1] - $ticks[0] }
    foreach ($entry in @(@('nrf', 'g_nrf_diag', 10), @('oled', 'g_iic_diag', 5), @('battery', 'g_cw2015_diag', 7))) {
        $values = @()
        for ($i = 0; $i -lt $entry[2]; $i++) {
            $address = [uint32]($addresses[$entry[1]] + 4 * $i)
            if (-not $words.ContainsKey($address)) { throw ('缺少内存数据0x{0:X8}' -f $address) }
            $values += $words[$address]
        }
        $result[$entry[0]] = $values
    }
    return [pscustomobject]$result
}

$before = Read-Snapshot
for ($window = 1; $window -le $Windows; $window++) {
    Start-Sleep -Seconds $Seconds
    $after = Read-Snapshot
    $elapsed = ($after.tick_ms - $before.tick_ms) / 1000.0
    if ($elapsed -le 0 -or $after.nrf[0] -lt $before.nrf[0] -or $after.oled[1] -lt $before.oled[1]) {
        throw '观察窗口中发生复位或计数回绕，结果无效'
    }
    $attempts = $after.nrf[0] - $before.nrf[0]
    $frames = $after.oled[1] - $before.oled[1]
    [pscustomobject]@{
        window = $window
        elapsed_s = $elapsed
        tx_attempts = $attempts
        tx_hz = [math]::Round($attempts / $elapsed, 3)
        tx_acked = $after.nrf[1] - $before.nrf[1]
        tx_max_retries = $after.nrf[2] - $before.nrf[2]
        tx_other_errors = $after.nrf[3] - $before.nrf[3]
        oled_frames = $frames
        oled_fps = [math]::Round($frames / $elapsed, 3)
        oled_errors = $after.oled[2] - $before.oled[2]
        oled_recoveries = $after.oled[3] - $before.oled[3]
        battery_reads = $after.battery[1] - $before.battery[1]
        battery_errors = $after.battery[2] - $before.battery[2]
        lifetime_tx_interval_min_ms = $after.nrf[5]
        lifetime_tx_interval_max_ms = $after.nrf[6]
        rf_channel = $after.nrf[8]
        rf_setup = ('0x{0:X2}' -f $after.nrf[9])
        snapshot_span_ms = @($before.read_span_ms, $after.read_span_ms)
    } | ConvertTo-Json -Compress
    $before = $after
}
