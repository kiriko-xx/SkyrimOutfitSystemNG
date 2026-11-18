# Skyrim Outfit System — NG

把任意已穿护甲的外观替换成任意套装（“幻化”），战斗数值、护甲评级、perk 与套装加成仍然读取真实护甲。

本仓库是 Skyrim Outfit System SE 0.7.0 的 **CommonLibSSE-NG 移植版**：原 C++ 幻化实现（xbyak 钩子、Papyrus 层、策略系统、ESP/脚本/翻译）全部保留，只把随包的 Rust 数据层等价改写为 C++，并在此基础上增加/替换若干功能。没有任何 fitting-room 代码。

- 当前版本：**1.5.30**（修 m06712：OStim 场景里切 `_sex` 套装不显示、必须按一次 Tab/Z 才更新 —— 原因是「套装已切换」不等于「渲染模型已更新」，一次装备 pass 只填 biped 条目，几何要由后续 pass 物化；现在 `MorphRefresh::RequestOutfitPasses()` 在每次切换后补跑 120/400/900 ms 三次 pass，并由 `Features::RefreshActor()` 统一调用，`SceneOutfits` 也改走它。同时按 m06781 把模块文件名改为 **`OutfitSystemNG.dll`**（内部插件名、日志文件名、翻译前缀不变；**旧名 `SkyrimOutfitSystemNG.dll` 必须删除**，否则双加载）。未实机验证。版本、哈希与回退见 [VERSIONS.md](VERSIONS.md)）
- 支持：Skyrim SE 1.5.97 / AE 1.6.1130+（含 1.6.1170、1.6.1179、1.6.640）；VR 不支持
- 前置：Address Library for SKSE Plugins；SKSE Menu Framework 3.x 可选（没有它插件照常工作，只是没有菜单）

## 用户文档

交付包里的 `dist/README.md` 是完整的用户手册（安装、功能、ini 全部键、常见问题、升级须知），`dist/docs/` 下另有：

| 文档 | 内容 |
| --- | --- |
| ARCHITECTURE.md | 模块划分、数据模型、槽位策略、幻化数据流、生命周期 |
| HOOKS.md | 五条 xbyak 钩子的 ID/偏移、语义、诚实还原、预检与排查 |
| BUILD.md | 构建链、vcpkg triplet、计划任务无人值守构建、打包 |
| PAPYRUS_API.md | 全部 Papyrus 函数（含原版 63 个注册名对照） |
| MCM_PARITY.md | 原版 SkyUI MCM 与 SMF 菜单的逐项对照 |
| I18N.md | 翻译文件位置/格式、新增语言、覆盖范围 |
| TESTING.md | 静态验证、首测顺序、功能矩阵、回归清单 |
| DELIVERY.md | 版本与哈希、需求对照、静态验证结果、日志排查 |
| CHANGELOG.md / THIRD_PARTY.md | 版本变更、第三方许可 |

## 源码结构

`src/cpp/` 原版 C++ 层（Papyrus、菜单、钩子、兼容）；`src/core/` 取代 Rust crate 的 C++ 数据层；`src/papyrus/` 脚本源码；`mod_files/` ESP+ini+翻译；`dist/` 交付目录；`docs/` 文档源。

## 构建

VS2022 + CMake >= 3.21 + vcpkg（triplet `x64-windows-static`，静态 CRT），产物自动拷到 `dist/SKSE/Plugins/`。详见 `docs/BUILD.md`。

## 已知限制

- 本机没有安装 Skyrim，所有游戏内行为**未实机验证**（静态验证项见 `docs/DELIVERY.md`）。
- 旧 co-save（protobuf，版本 ≤ 6）读档时自动迁移为 JSON（版本 7），版本 3 及更早不支持。
- `RemoveActor` 拒绝移除玩家；VR 未构建。

## 版本管理

2026-11-17 之前本项目没有 git 仓库，项目根只有成品包 `SkyrimOutfitSystemNG-1.5.17.zip` … `-1.5.22.zip`（各带 `.zip.sha256`），**1.5.17–1.5.21 的源码没有快照**。现在仓库已 `git init`，`v1.5.22-archive` 是第一个记录；每个交付版本定稿后在 `dist/` 收尾并打 `vX.Y.Z` tag。完整列表、哈希与回退规则见 [VERSIONS.md](VERSIONS.md)。

## 许可

沿用原项目 CC BY-NC-SA 4.0，见 LICENSE.md。
