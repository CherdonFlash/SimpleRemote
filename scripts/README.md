# 项目脚本

这里集中保存 SimpleRemote 设计和校核过程中由 AI 辅助生成的自动化脚本源码，避免脚本、编译产物与正式硬件文件混放。

## mechanical

- `AnalyzeNewPcbStep.py`：读取 STEP 实体关系并提取 PCB 与器件的机械包络。
- `BuildRemoteEnclosure.cs`：生成或修改遥控器上壳结构。
- `BuildShellAssembly.cs`、`BuildCorrectObjValidation.cs`：构建外壳及 PCB 对位校核装配体。
- `CheckAssemblyInterference.cs`、`CheckShellBodies.cs`、`SwInspector.cs`：检查装配干涉、实体状态和模型信息。
- `BuildNewPcbValidation.cs`、`BuildRemoteAssembly.cs`：早期基准模型的重建与校核源码，保留用于设计追溯。

Python 脚本可直接通过命令行运行。C# 脚本依赖本机 SolidWorks 2024 COM 接口及对应 Interop 程序集；生成的 EXE、DLL、PDB 和临时结果不纳入 Git。
