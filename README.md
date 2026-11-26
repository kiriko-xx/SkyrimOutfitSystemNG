# Skyrim Outfit System — NG

把任意已穿护甲的外观替换成任意套装（“幻化”），战斗数值、护甲评级、perk 与套装加成仍然读取真实护甲。

本仓库是 Skyrim Outfit System SE 0.7.0 的 **CommonLibSSE-NG 移植版**：原 C++ 幻化实现（xbyak 钩子、Papyrus 层、策略系统、ESP/脚本/翻译）全部保留，只把随包的 Rust 数据层等价改写为 C++，并在此基础上增加/替换若干功能。没有任何 fitting-room 代码。

- 当前版本：**1.5.81**（2026-11-26，`main` / tag `v1.5.81`，实机通过）。1.5.65 之后的第一个正式发布，一次收尾 1.5.66–1.5.81 测试包线：①热键录制窗口照 Eelta/FreeClimb 的集合语义重做 —— 组合键全部松开才确认、录制期间键盘与手柄输入由本插件独占（Esc 不再关 SMF 主菜单、手柄 B/LB 不再漏给宿主）、LT/RT 能录、手柄组合按按下顺序显示；②热键「是否按住」直接问设备，修掉残留键导致的「只按 Alt 就触发 Alt+O」；③能力键与快速选择键只在「Change Outfit」被选中时归本插件（键盘/手柄同一判定）；④快速选择窗口 Esc/Tab 都能关、底部提示按设备分行并自动折行；⑤钩子与别的插件共存 —— 别的插件抢先钩住幻化显示函数时顺着跳转链跟上去钩、够不着就架远跳桩、5 字节分支自写（修 AE 启动即崩）、写完读回并在加载后复查两次、被覆盖时点名接管者。历史版本与回退见 `VERSIONS.md`。
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

- SE 1.5.97 与 AE 1.6.1170 均已实机验证；其余 AE 版本（1.6.317–1.6.1179）只做静态审计（Address Library 覆盖 + 双 exe 反汇编对表），未逐版本实机。
- 旧 co-save（protobuf，版本 ≤ 6）读档时自动迁移为 JSON（版本 7），版本 3 及更早不支持。
- `RemoveActor` 拒绝移除玩家；VR 未构建。

## 版本管理

2026-11-17 之前本项目没有 git 仓库，项目根只有成品包 `SkyrimOutfitSystemNG-1.5.17.zip` … `-1.5.22.zip`（各带 `.zip.sha256`），**1.5.17–1.5.21 的源码没有快照**。现在仓库已 `git init`，`v1.5.22-archive` 是第一个记录；每个交付版本定稿后在 `dist/` 收尾并打 `vX.Y.Z` tag。完整列表、哈希与回退规则见 [VERSIONS.md](VERSIONS.md)。

## 许可

沿用原项目 CC BY-NC-SA 4.0，见 LICENSE.md。
