# Skyrim Outfit System — NG

把任意已穿护甲的外观替换成任意套装（“幻化”），战斗数值、护甲评级、perk 与套装加成仍然读取真实护甲。

本仓库是 Skyrim Outfit System SE 0.7.0 的 **CommonLibSSE-NG 移植版**：原 C++ 幻化实现（xbyak 钩子、Papyrus 层、策略系统、ESP/脚本/翻译）全部保留，只把随包的 Rust 数据层等价改写为 C++，并在此基础上增加/替换若干功能。没有任何 fitting-room 代码。

- 当前版本：**1.5.46**（未实机验证；工作分支 `feature/slot-policy-esp-outfits-golden`，`main` 仍是已验收的 1.5.34）。本版只重写「从 ESP 生成套装」的识别算法（m11058）：不再假定名称里有 `_`、`-` 或空格，也不再只看「前 4 个词」—— ①名称标准化只做比较副本（英文小写、丢弃一切分隔符、忽略 `[]` 与 `()` 标签，原文拼写只用于套装命名）；②款式识别用字符级前缀树：一个前缀下的每支分支若各自都已构成「两件以上、跨两个以上槽位」的服装组，就拆成不同款式（`星夜礼服` 与 `星夜晚礼服` 分开），否则整支算同一套；③同一款式内按槽位分桶，槽内多件剥掉共同部分取差分标识（A/B/红/黑），单件槽对所有组合共享；④按同一差分标识跨槽位组合（上衣 A/B 配裙子红/黑这种标识对不上的，整套不生成并在日志打 `cannot be matched by name across slots`，宁缺勿错），同槽两件冲突丢该组合、同名同槽取第一件；⑤命名与上限沿用 1.5.45（`<ESP名称> <款式>` 或 `<ESP名称>` / `_1`，96 套 / 32 变体 / `XXXO`），日志 `ESP outfits: … in K costume(s)` 的 K 就是套数。金件页与引擎侧（注入壳 / 贴件语义 / 槽位清理 / `.tri` 推导 / 场景套装 / 钩子）一行未改 —— 1.5.25 的基础幻化、1.5.34 的场景切换、1.5.42 的删除崩溃、1.5.44 的组行点击都不受影响。
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
