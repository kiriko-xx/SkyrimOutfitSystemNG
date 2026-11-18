# 版本与回退（VERSIONS）

本文件记录每一个交付版本对应的成品包、哈希与源码状态。**2026-11-17 之前本项目没有版本管理**：项目根只保留成品 zip，1.5.17–1.5.21 的源码没有快照，无法还原；从 2026-11-17 起用 git 管理（见文末）。

## 成品包

| 版本 | 日期 | 包大小（字节） | SHA-256 | 状态 / 备注 |
| --- | --- | --- | --- | --- |
| 1.5.17 | 2026-11-16 | 981,613 | `3e58e8442371020a863463d4cdde43f4cb72d168141ad9172212a86def9dd869` | **当前回退目标**；SLIF NG 兼容版；幻化显示正常 |
| 1.5.18 | 2026-11-16 | 986,943 | `2394cb3780aac9791b90c55ff7ebeddcf61fda3e9045fe8bdd2ba4e33ae4f008` | 几何签名探针；实机未解决 morph 跟随 |
| 1.5.19 | 2026-11-17 | 1,022,531 | `31e81dcd32f36bc3a1c03b739939d46ebbdc3995ebd2b2c59d0b7260c1af4ab9` | provider-first；skee 握手永不放弃 |
| 1.5.20 | 2026-11-17 | 1,020,827 | `a9ca108664f0df5714e7ba40f43f5910addb98f22194caadde4955cb8b115638` | .tri 目标 + MorphShapeCallback；**实机全裸**（CustomSkinPlayer 钩子校验失败） |
| 1.5.21 | 2026-11-17 | 1,037,676 | `5c0bbab12a2edac1f85ebddcfafa8d01a4d7327ea31d37afc061e44785a6097d` | 自定位 + fail-closed；幻化整条被关掉 |
| 1.5.22 | 2026-11-17 | 1,044,801 | `fee2d697056d1a012ee44efab6c776b2e095b241b89980942322906d44b4b2a2` | 函数体内扫 E8；实机仍失败，已回退 |
| 1.5.23 | 2026-11-17 | 1,053,290 | `e75a9274f8f0c5190ba9139311c1fe2da4871fa76d4e88ca7d920b2318c3a34b` | `CustomSkinPlayer` 扫描失败时回退 1.0.0 历史站点（SE `+0x81` / AE `+0x1EF`）+ 幻化门禁恢复成对；DLL 1,863,168 B / `be282e7a…`；**实机只显示头手、46 槽外露** |
| 1.5.24 | 2026-11-17 | 1,059,923 | `068b2396e6c0c72c4c26087a3aeb0f9f6da3a323a292acb8cda43254392c92cf` | 贴件回到引擎传进来的 `ActorWeightModel`（`rbx`/`rdi` 寄存器契约）、只贴未真实穿戴的件；DLL 1,866,752 B / `2530c1e5…`；**实机幻化件恢复显示，但身体（32 槽）变空模型** |
| 1.5.25 | 2026-11-17 | 1,065,358 | `afb826b016997ec57d17c01fddc7cf64617837a3deafde153227dabc8ceca191` | 删除 `ClearSlots`/`SlotClone`：离开幻化集的槽不再清空、原样交回引擎（`left to the engine`）；DLL 1,866,240 B / `ae797d9d…`；**实机确认幻化服装与空幻化都正常**（m02536） |
| 1.5.26 | 2026-11-17 | 1,074,870 | `705f3517db9553df9e0bda4c5cadda59c49275d337ceea2b21dc4d3b0f288df6` | `.tri` 跟随不再依赖 skee 接口：每个幻化克隆件无条件贴 `BODYTRI`（RaceMenu/OBody 自己的 `ApplyBodyMorphs` 会遍历到并 morph 它），接口降级为加分项，握手改到主线程任务，几何探针补蒙皮网格 `rawVertexData`；DLL 1,868,288 B / `7ca36b0d…`；**实机 0 个 `.tri` target 贴上（m02992）** |

| 1.5.27 | 2026-11-17 | 1,085,341 | `5979290d01fb375c1d2a46040f4d799d21fa9db3cde2e528a365853d3569cfe9` | **实机仍不跟随**；`.tri` 路径改问引擎已解析的 `biped->objects[slot].addon`（不再用 `GetArmorAddon(race)`），四个候选名全试，`.nif` 自带目标也计入，克隆件武装后不再 `CycleIllusion`；DLL 1,875,456 B / `22eb7794…` |

| 1.5.28 | 2026-11-17 | 1,126,775 | `4894dcbc71c6a1cf445c87c301d13804aeb47791ed32d0ffc3ee0a1a41613b35` | **已被 1.5.29 取代**；实机反馈 m05502（场景结束残留 + 一次渲染期崩溃）后不再作为候选；修双 meshes 前缀、校验 BODYTRI 可读资源、ARMA/sex/1P/3P 缓存、回调过滤与 provider 基线；自动刷新不摘幻化；DLL 1,878,016 B / `fe5c79b5b1f1e1fc3153870dd6b61cf293081805cab407ad3ea41ed1621bb553` / 1.5.28.0；31 条目解包哈希31/31、清单30/30 |
| 1.5.29 | 2026-11-18 | 1,161,851 | `e581230e2fc8b75e9d1f161d59cf0dfed8852b4ea37db2b1676b934f121f6521` | **已被 1.5.30 取代**；修场景结束后的幻化残留（窄清我们自己的克隆）与主线程幻化几何对账（`IllusionMissingSlots` → `refreshArmorFor`）；基础幻化钩子未动；DLL 1,936,384 B / `4f0bcae1e5926763c3d9592ab01812c8f7b968d6e8c7ef37d0ac002c3cb84c92` / 1.5.29.0 |

| 1.5.30 | 2026-11-18 | 1,170,589 | `8d0db7dc23a6f1609b7206d0ab9ba6194f2a4d2c9d2a7e71f89dede1cda9989b` | **已被 1.5.31 取代**（实机 m07050：补跑 pass 全部执行但模型仍不出，缺的是引擎重建 3D）；场景切换补跑有限次装备 pass（120/400/900 ms，`Features::RefreshActor` 统一入口）修「切 `_sex` 必须弹一次窗」；模块改名为 `OutfitSystemNG.dll`（内部插件名不变，旧名必须删除防双加载）；DLL 1,939,456 B / `2d0d968953abe0c3715cf1e2e8b882b2d410d937fc30e0a8659dda451d79cfda` / 1.5.30.0 |
| 1.5.31 | 2026-11-18 | 1,178,871 | `3ebd6861a51597076fee666748261c359e7b7f2de763b2674f61c90d42a9920f` | **已被 1.5.32 取代**（实机 m07382：`Model Rebuild for` 照跑、模型仍不出现，用户要求把补跑与重建整套删掉）；场景开始/结束不必再弹窗：新增 `OutfitSystem::rebuildActorModel()`（`Actor::Update3DModel()`，挂在随访链第 2 步），`SceneOutfits` 改调 `Features::AssignOutfit()`（与「点套装列表」同一条函数）；DLL `OutfitSystemNG.dll` 1,941,504 B / `1.5.31.0` / `c8c8ae5d…` |
| 1.5.32 | 2026-11-18 | 1,185,452 | `065f0ad79fdd31c17116901d3bee74a7cfe15c385d4660d61e3238d29378a8f1` | **已被 1.5.33 取代**（实机 m07666：场景切换后仍要按键；该版把场景入口改成 `Features::AssignOutfit()`，与用户「参照按地点/状态切换」的指示不符）；场景套装回到「找对名字 → 切套装」（m07382 / m07424）：删除 1.5.30 的三次补跑装备 pass 与 1.5.31 的 `rebuildActorModel()`，场景开始/结束只调 `Features::AssignOutfit()`，事件改游戏线程同步执行，名字查找支持忽略大小写；DLL 1,937,920 B / `da9ec899…` / 1.5.32.0 |
| 1.5.33 | 2026-11-18 | 1,191,778 | `3166bb3db3f7afb958300edc50e0e52d6183d28ca1e032c0353b3e7d2102c405` | **已被 1.5.34 取代**（实机 m07842：场景事件被 `OnGameThread()` 门禁整条丢弃，OStim 场景开始后压根没切换）；按 m07666 回退源码到 `v1.5.29` 后重做场景套装：删掉场景专用的 SKSE 任务队列 / `Tick()` 心跳 / 补跑 / 重建，新增 `OutfitSystem::setOutfitByName()`，场景开始/结束走「锁内指派 → 锁外 `refreshArmorFor()`」，与 `AutoSwitch::Evaluate()`（按地点/状态切换，本版一行未改）同构；基础幻化钩子 / 注入壳 / 贴件语义 
| 1.5.34 | 2026-11-18 | 1,200,086 | `98fd0cff7b7a10bc2b9cae32bb7c3eacbfa6545c5374e099867bad66c8ba6c8b` | **当前候选，待实机**；修 m07842 —— 1.5.33 的 `OnGameThread()` 门禁把工作线程送来的场景事件整条丢弃，本版改为「离线程事件用一次 `SKSE::GetTaskInterface()->AddTask` 交接到游戏线程 → 统一 `Dispatch()` 入口」；不建队列、不加 `Tick()`，换装序列仍 `setOutfitByName()` + `refreshArmorFor()`；DLL `OutfitSystemNG.dll` 1,937,408 B / `e1cdb951…` / `1.5.34.0` |/ 槽清理 / `.tri` 推导未动；DLL 1,934,848 B / `8c23e5ee…` / 1.5.33.0 |

各包旁的 `.zip.sha256` 内容为「哈希 + 两个空格 + 包名」+ CRLF，可直接 `certutil -hashfile` 复核。

## 源码状态

| 版本 | 源码 |
| --- | --- |
| 1.5.17 – 1.5.21 | **没有源码快照**，只剩成品包里的 DLL/脚本/文档 |
| 1.5.22 | git tag `v1.5.22-archive`（提交 `3c0b118`，2996 文件） |
| 1.5.23 | git tag `v1.5.23`（提交 `573b888`） |
| 1.5.24 | git tag `v1.5.24`（提交 `0a78b76`） |
| 1.5.25 | git tag `v1.5.25`（提交 `2db9580`） |
| 1.5.26 | git tag `v1.5.26` |
| 1.5.27 | git tag `v1.5.27` |
| 1.5.28 | git tag `v1.5.28` |
| 1.5.29 | git tag `v1.5.29` |
| 1.5.30 | git tag `v1.5.30` |
| 1.5.31 | git tag `v1.5.31` |
| 1.5.32 | git tag `v1.5.32` |
| 1.5.33 | git tag `v1.5.33` |
| 1.5.34 | git tag `v1.5.34` |
| 当前 dist | 1.5.34 的候选交付（31 文件 / 2,768,310 B；`SKSE/Plugins/OutfitSystemNG.dll` 1,937,408 B / 1.5.34.0 / `e1cdb951…`；旧名 `SkyrimOutfitSystemNG.dll` 不在包内），SHA256SUMS.txt 30 行复核 30/30 |

1.5.22 的交付目录存档在 `build/dist_1_5_22/`（build/ 不入库）。

## 版本管理规则

- **分支布局（2026-11-18 起）**：`main` = 最新发布线（当前 `1.5.34`，提交与 tag `v1.5.33` 相同）；发布一律在 `main` 上按 tag 推进。历史上的实验性提交（2026-11-17 晚 `fix: gate scene and morph tasks…` 那一串）不在发布线上，已封存为分支 `archive/main-experiments-20261117` 与同名 tag，需要时 `git diff main archive/main-experiments-20261117 -- src/` 对照取舍。
- 仓库根的 `.gitignore` 忽略 `dist/`、`build/`、`*.dll`、`*.pex`、`*.log`：git 只跟踪源码与文档，交付目录每次由构建生成。
- 每次定稿一个交付版本：先把 `dist/` 收尾，再 `git tag vX.Y.Z`（tag 打在当时那份源码上），随后打包 zip 并更新本文件。
- 回退 = 直接装对应版本的 zip；**不要**在没有源码快照的版本之间做“源码回退”，只能以二进制为准。
- 回退 1.5.17 之后，如果要从源码继续 1.5.17 的行为基线，需要以 1.5.22 的源码为底做行为级裁剪（1.5.18 起的改动集中在 MorphRefresh / HookSupport / Hooks / Patches，以及 09:46–10:41 那批菜单与 i18n 改动），并且必须逐项对照本文件里的版本说明。

## 回退与合并（用户 m06712 要求：随时可回退、可合并有用版本）

**回退（推荐、唯一安全途径）**：装对应版本的成品包即可 —— 仓库根有 1.5.17–1.5.33 的 `SkyrimOutfitSystemNG-<版本>.zip`，每个包旁有 `.zip.sha256`（内容为 `hash + 两空格 + 文件名`）。想完全干净地回退：删掉 `Data/SKSE/Plugins/` 下的 `OutfitSystemNG.dll` **和** `SkyrimOutfitSystemNG.dll`，再解压目标包覆盖。

**源码级回退**：`git tag` 逐版对应（见上表）。`git checkout v1.5.29` 可回到那一版源码，`git diff v1.5.29 v1.5.30 -- src/` 可看单版差异。注意 1.5.17–1.5.21 **没有源码快照**，只能以二进制回退。

**合并有用版本**（例如把 1.5.29 的残留克隆窄清搬到更新的分支上）：

```
git switch -c merge/from-1.5.29 v1.5.30
git cherry-pick -n v1.5.29          # -n 不自动提交，便于取舍
git diff --cached -- src/            # 逐个 hunk 看，只要想要的部分
git commit -m "merge: bring 1.5.29 stale-clone narrow clear forward"
```

跨多个版本时用 `git diff v1.5.27 v1.5.29 -- src/cpp/` 先看区间差异，再按文件挑（`git checkout v1.5.29 -- src/cpp/hooking/Patches.cpp` 会把整份文件换成那一版，慎用）。

**合并时绝对不能动的底线**：①`Hooks.cpp` 的注入壳与站点选择（`FindCallInFunctionOrOffset`）不要跨版本混用，1.5.17 与 1.5.22+ 的契约不同；②不得恢复任何 `ClearBipedPart` / `ClearSlots` 对 `BipedAnim::objects[bit]` 的清空（1.5.24 的 bug）；③`.tri` 目标必须无条件贴，不得再以拿到 skee 接口为前提（1.5.20–1.5.25 的 bug）；④模块名字段只能出现一个 DLL（`OutfitSystemNG.dll`）。

**每次发布固定顺序**（避免台账自引用死循环）：改 `version.h`/`CMakeLists` → 构建 → 收尾 `dist/` → 重算 `SHA256SUMS.txt` → 打包并记录 zip 大小/哈希到本文件 → `git add -A` + `git commit` + `git tag vX.Y.Z`。不要在 commit 消息或本文件里写自身提交哈希。