# 第三方组件（THIRD_PARTY）

## 运行期依赖（不随包分发）

| 组件 | 作者/来源 | 许可 | 说明 |
| --- | --- | --- | --- |
| SKSE64 2.0.20+ / 2.2.6+ | Ian Patterson & Stephen Abel | 自定义（允许免费使用） | 插件加载与 Papyrus/存档接口 |
| Address Library for SKSE Plugins | meh321 | 免费使用 | 提供 ID → 偏移映射 |
| SKSE Menu Framework 3.x（可选） | Skyrim Menu Framework 项目 | LGPL-2.1 | 设置菜单宿主；本插件运行期 `GetProcAddress` 绑定，未安装也能正常工作 |

本插件**不导入** `SKSEMenuFramework.dll` 与 `imgui.dll`，也没有延迟导入；缺失时只损失设置菜单。

## 构建期依赖（已编译进 DLL）

| 组件 | 版本 | 许可 |
| --- | --- | --- |
| CommonLibSSE-NG（alandtse fork, ng 分支 16.4.0） | commit 9955ae3 | GPL-3.0-or-later，附 Modding Exception（见其 EXCEPTIONS.md，允许用于 Skyrim 插件） |
| xbyak | vcpkg baseline | BSD-3-Clause |
| spdlog | 同上 | MIT |
| fmt | 同上 | MIT（{fmt} 许可） |
| nlohmann/json | 同上 | MIT |
| SimpleIni | 同上 | MIT |
| rapidcsv | 同上 | BSD-2-Clause |
| toml11 | 同上 | MIT |
| DirectXMath / DirectXToolkit | 同上 | MIT |
| imgui 头文件（仅头，用于类型与函数指针声明） | 1.90.x | MIT |


## 兼容的目标 mod（既不随包分发，也不链接、不 hook）

| mod | 版本 | 交互方式 |
| --- | --- | --- |
| Procedural Armor Damage | 1.0.4 | 无代码交互。它的可撕件门禁（背包里 `IsWorn` + 场景里 `BIPOBJECT.item`）与本插件的“诚实还原”天然一致；本插件只在 ExtraLogging 下记录“幻化件下无真实护甲”的槽位。逆向仅用于确认兼容性，产物中不含其任何代码。 |
| Advanced Nudity Detection | 4.2.0 | 只写它对外发布的阵营（`AND_NudeActorFaction` / `AND_ToplessFaction` / `AND_BottomlessFaction`）等级，走引擎 `Actor::AddToFaction`；不 hook、不改内存、不读它的内部结构。阵营按 EditorID 解析，找不到即退回原版行为。 |
| OBody | 4.4.0+（可选） | 通过 OBody 公开的 SKSE 插件 API（`RequestPluginInterface` / `IPluginInterface`）读取 `GetPresetAssignedToActor`，并在无 RaceMenu 时调用 `ApplyOBodyMorphsToActor`；不 hook、不改内存、不随包分发。接口声明为按公开头文件 ABI 手写的最小声明，产物不含 OBody 代码。 |
## 上游项目

本仓库是 **Skyrim Outfit System SE**（metricexpansion，master 34a702a，2023-04-15）的移植与改进，遵循其许可 **CC BY-NC-SA 4.0**（见 LICENSE.md）。原项目的 C++ 幻化实现、Papyrus 脚本、ESP 与翻译均保留。

原项目的 Rust 数据层（`src/rust`，cxx + tokio + protobuf）与 Svelte Web UI（`src/web`）在本移植中被移除，功能以 C++（`src/core`）等价重写；移除的组件不在本仓库的构建产物中。

## 未使用的参考

开发过程中查看过 `maartenharms/fitting-room`（GPL-3.0）的公开代码以核对 CommonLibSSE-NG 的接口形态，但**没有复制任何代码或结构**：本项目的钩子表、偏移、数据模型与 UI 均来自原 Skyrim Outfit System 工程与 Address Library。
