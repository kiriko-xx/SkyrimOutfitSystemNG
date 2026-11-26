# 版本与回退（VERSIONS）

本文件记录每一个交付版本对应的成品包、哈希与源码状态。**2026-11-17 之前本项目没有版本管理**：项目根只保留成品 zip，1.5.17–1.5.21 的源码没有快照，无法还原；从 2026-11-17 起用 git 管理（见文末）。

## 成品包

| 版本 | 日期 | 包大小（字节） | SHA-256 | 状态 / 备注 |
| --- | --- | --- | --- | --- |
| 1.5.81 | 2026-11-26 | 1,454,541 | `35C5F0DE58E69FAB4A56927BDFFC4DA75D5795D73DAAB3C6513B0B59DC47D601` | **当前版本，已并入 `main`，tag `v1.5.81`**：1.5.65 之后的第一个正式发布，一次收尾 1.5.66–1.5.81 测试包线 —— 热键录制窗口照 Eelta/FreeClimb 的集合语义重做（全部松开才确认、录制期间键盘与手柄输入由插件独占、LT/RT 能录、手柄组合按按下顺序显示、热键状态直接问设备），能力键与快速选择键只在「Change Outfit」被选中时归本插件（键盘/手柄同一判定），快速选择 Esc/Tab 都能关、底部提示按设备分行并自动折行，框架默认 logger 关闭（不再多出一行 `SkyrimOutfitSystemNG.log`），钩子与别的插件共存（跟跳转链 → 远跳桩 → 自写 5 字节分支 → 写完读回 + 加载后复查两次 + 点名接管者 → 幻化不可用弹提示框）。SE 1.5.97 / AE 1.6.1170 实机通过；DLL 2,301,440 B / `4D443994…` / 1.5.81.0。1.5.46 与 1.5.66–1.5.80 只出过测试包，不单独发成品包 |
| 1.5.56 | 2026-11-22 | 1,413,907 | `82203e35ebbb35c14ca9f23e09618ed58c189fd2323bc21dedfc82486f24194c` | **（历史，被 1.5.81 取代）已并入 `main`，tag `v1.5.56`**：Z 键即时打开快速选择（`[RandomOutfit] QuickPickerKey = Z`，0.5 s 限流，绕过 lesser power 的施法动画）、未开 `ExtraLogging` 时日志只剩摘要行（预览行附待判断原因直方图）、i18n 341 条全量落到 `interface/translations`（129 处裸 ImGui 文案改走 `I18n::Tr/TrId`）、翻译文件 `$英文<TAB>译文` 逐字取用、新增 `tests/i18n_translation_files.ps1` 守卫；DLL 2,207,232 B / `42dcf428…` / 1.5.56.0；31 条目解包逐文件哈希与 dist 全等；本版一次性收尾 1.5.47–1.5.55（ESP 生成算法定稿：标签组整装、单槽不截断、待判断并入、同名副本不崩、撞槽不毁族、一槽一件） |
| 1.5.17 | 2026-11-16 | 981,613 | `3e58e8442371020a863463d4cdde43f4cb72d168141ad9172212a86def9dd869` | 早期回退目标（1.5.56 之前的成熟基线）；SLIF NG 兼容版；幻化显示正常 |
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
| 1.5.33 | 2026-11-18 | 1,191,778 | `3166bb3db3f7afb958300edc50e0e52d6183d28ca1e032c0353b3e7d2102c405` | **已被 1.5.34 取代**（实机 m07842：场景事件被 `OnGameThread()` 门禁整条丢弃，OStim 场景开始后压根没切换）；按 m07666 回退源码到 `v1.5.29` 后重做场景套装：删掉场景专用的 SKSE 任务队列 / `Tick()` 心跳 / 补跑 / 重建，新增 `OutfitSystem::setOutfitByName()`，场景开始/结束走「锁内指派 → 锁外 `refreshArmorFor()`」，与 `AutoSwitch::Evaluate()`（按地点/状态切换，本版一行未改）同构；基础幻化钩子 / 注入壳 / 贴件语义  / 槽清理 / `.tri` 推导未动；DLL 1,934,848 B / `8c23e5ee…` / 1.5.33.0 |
| 1.5.34 | 2026-11-18 | 1,200,086 | `98fd0cff7b7a10bc2b9cae32bb7c3eacbfa6545c5374e099867bad66c8ba6c8b` | **当前 main 发布版，已验收**（m08008：OStim 场景套装自动切换实机正常）；修 m07842 —— 1.5.33 的 `OnGameThread()` 门禁把工作线程送来的场景事件整条丢弃，本版改为「离线程事件用一次 `SKSE::GetTaskInterface()->AddTask` 交接到游戏线程 → 统一 `Dispatch()` 入口」；不建队列、不加 `Tick()`，换装序列仍 `setOutfitByName()` + `refreshArmorFor()`；DLL `OutfitSystemNG.dll` 1,937,408 B / `e1cdb951…` / `1.5.34.0` 
| 1.5.45 | 2026-11-21 | 1,341,328 | `5c699ce05b6cbb4d3e5c99a83da5453b02ded651b38e248e78a41bbf85d782de` | **当前工作基线（分支 `feature/slot-policy-esp-outfits-golden` HEAD = `591e437`，2026-11-21 由 1.5.46 回退至此）**：金件两级列表可用；**「从 ESP 生成套装」的按名分组仍是启发式，用户反馈乱组（见 `HANDOFF.md` 第 5 节），下一版要重做判据**；金件「按 FormID 添加单件」改成「按 ESP 添加单件」（选 ESP → 列出该 ESP 的全部槽位护甲 → 点一件即加入；左侧金件组选中时右侧同一列表），`ParseLocalId` / `##goldenlocalid` 已删；从 ESP 生成套装按服装名「至少两件共享的最长词前缀（≤4 词）」分组，组内仍按槽位冲突拆变体，单服装 ESP 命名不变，多套为 `<ESP> <服装名>`，上限 96 套（m10869）。DLL 2,074,112 B / `075e211e…` / 1.5.45.0；引擎侧（注入壳/贴件/槽清理/`.tri`/场景套装/钩子）零改动 |
| 1.5.46 | 2026-11-21 | 1,364,056 | `aa2b7230171264f442bbefd821d830eddab112803601ae76baa5fa1d93941361` | **已作废并回退到 1.5.45**（用户 m11335：「生成的预设简直是不按名称乱组」）；`ESP 生成套装`改为 Trie + 字符级公共前缀 + 槽位约束组合，DLL 2,097,664 B / `220994b5…` / 1.5.46.0；源码留在 tag `v1.5.46`（`c7202b4`）与语义 tag `abandoned/esp-outfits-trie-1.5.46`，仅作反面参考 |
| 1.5.44 | 2026-11-21 | 1,316,037 | `ab9fc2dc99c29c4d218c40b64b2c402fc3d26a2039c028e3ec8c0edc78f0f2c7` | **已被 1.5.45 取代（金件仍要求手输 FormID、套装不按服装名分组 m10869）**；只修「金件列表依旧点不中」（m10689）—— 真因不是命中矩形而是 **ImGui 控件 id 跨帧不稳定**：组行推 `IG::PushID(group.plugin.c_str())` 命中 `PushID(const void*)` 重载，而 `Golden::Groups()` 每帧返回新副本 ⇒ 指针变 ⇒ 按下帧与松开帧 id 不同 ⇒ 点击被丢弃；同页插件选择器用整数 `PushID(it->order)` 所以能点，「添加某个 ESP 的全部装备」是代码直接赋 `g_goldenSelected` 所以会选中。修法：组行改 `PushID(static_cast<int>(rowIndex))`、选中日志升 info `Menu: golden group … selected (N of M).`、新增守卫 `tests/golden_row_id.ps1`。DLL 2,042,368 B / `859d81d5…` / 1.5.44.0；测试包 917,373 B / `c76d6e42…` |
| 1.5.43 | 2026-11-21 | 1,307,589 | `b8fe9d196ebae620e3675adc4d4d44cd1bdad33dd5e26067b2481509101d3559` | **已被 1.5.44 取代（其组行仍把字符串指针当 ImGui id ⇒ 金件列表点不中 m10689）**；只修金件页三处（m10387）—— ①左侧组行点不中（`Selectable` 尺寸 `(0,0)` + 右栏负宽覆盖左栏 ⇒ `GoldenRow()` 显式宽高 + 双栏夹紧 `[240, max(240, avail-320)]`）；②取消每次编辑自动写盘（`g_goldenDirty`，`RenderGolden()` 内 `SaveToDisk()` 只剩导出分支，常显 `[not saved]` 提示）；③light(ESL) 插件金件显示 `<当前加载顺序里没有>`（id 存成 `formId & 0xFFFFFF` 把运行时**光索引**一起存了，`LookupForm` 二次叠加 ⇒ 61 条全 missing；新增 `PluginScan::IsLightPlugin()` / `LocalIdFor()` / `ResolveForm()`，旧档加载即归一化）。DLL 2,041,856 B / `349a3e0f…` / 1.5.43.0；测试包 916,980 B / `483e8406…` |
| 1.5.42 | 2026-11-21 | 1,291,155 | `454492e13d3e93d730594c14955f19e6670fde81a11f19c8866b0df6e4908a31` | **已被 1.5.43 取代（其实机启动闪退修复已由用户确认 m10387 首句「上个轮问题已修复」；同一版金件页有三处缺陷）；仍为未发布候选（分支 `feature/slot-policy-esp-outfits-golden`）**；只修 1.5.41 报出的新崩溃 —— **删除幻化套装预设时闪退**（m10183）：菜单分页切片在 `g_filtered.clear()` 之后仍按删除前的页码算，`end = 0` 而 `begin > 0`，`end - begin` 以 `size_t` 下溢 ⇒ `std::vector::assign` 抛 `std::length_error("vector too long")`，异常逃出菜单渲染回调把游戏带走（第 1 页删除不触发）。修法：先取 `total`、`begin` / `end` 都夹进 `total`，并加 `tests/menu_page_slice.ps1` 守卫。引擎侧与金件存库零改动；测试包 `SkyrimOutfitSystemNG-1.5.42-test.zip` 914,720 B |
| 1.5.41 | 2026-11-21 | 1,283,083 | `c640d60299b98c3d33b786a581bc95026a507322849d89cbaec739ca499cc7d3` | **已被 1.5.42 取代（其实机启动闪退已修好，但同一版在删除幻化套装预设时闪退 m10183）；仍未发布候选（分支 `feature/slot-policy-esp-outfits-golden`）**；只修 1.5.40 的**启动即闪退**（m10014）—— 金件存库锁 `g_mutex` 是普通 `std::mutex`，而 `Golden::ImportText()` 持锁后又调公共 `TotalCount()`（再 lock 一次）⇒ 同线程重入；MSVC 对此抛 `std::system_error("resource deadlock would occur")`，异常在 SKSE 消息回调里没人接 ⇒ 启动即退。触发点是 `kDataLoaded` 无条件的 `Golden::LoadFromDisk()`，而 1.5.39 崩溃前已写出 `OutfitSystemGoldenItems.json`。修法：①锁改 `std::recursive_mutex`；②新增 `TotalCountLocked()`，三条持锁路径改用它；③`kDataLoaded` 金件块包 try/catch（失败只打 `the golden list stays empty for this session`）；④`MessageHandler` 拆 `HandleMessage` + try/catch 壳。引擎侧与菜单功能零改动。另出只含 DLL 的测试包 `SkyrimOutfitSystemNG-1.5.41-test.zip`（914,574 B / `798989d7ed22270a46c732a6bfb89643488fe0de176853d35c9ca6fcb1147e57`） |
| 1.5.40 | 2026-11-21 | 1,272,548 | `1fcca7f372aaaab38a285431bca366da57d6ab0096c0ef3ef62c163d51296c50` | **已被 1.5.41 取代（实机 m10014：启动即闪退，金件存库锁自死锁）；仍为未发布候选（分支 `feature/slot-policy-esp-outfits-golden`）**；只修 1.5.39 实机报出的两处回归（m09740）—— ①「幻化」页 ESP 列表为空（服装判据只查男性模型 + 列表按 `outfitArmors` 过滤 ⇒ 女性服装包全被滤掉；改为男女都看、名字放宽、列表按 `entry.armors` 过滤）；②「金件」页「Add every piece of a plugin」必闪退（中英占位符顺序不一致，`I18n::T()` 结果被当 printf 格式串 ⇒ `%s` 吃到 `size_t` ⇒ AV at `OutfitSystemNG.dll+0125E41`；改中文串同序 + `I18n::Apply()` 加占位符校验护栏）。另出只含 DLL 的测试包 `SkyrimOutfitSystemNG-1.5.40-test.zip`（913,780 B / `26544231d6015489675564e4f320b97b1ecda84ef5a4a823fa3281b18c4e2dd5`） |
| 1.5.39 | 2026-11-20 | 1,258,672 | `4765a0568207829e5247033296587cec4cfb21d8d0eb4d1b6b96293c1ec934c1` | **已被 1.5.40 取代（实机 m09740 报两处回归：ESP 列表为空、金件页闪退）；仍为未发布候选（分支 `feature/slot-policy-esp-outfits-golden`）**；菜单三件 m09191 / m09426 —— ①逐槽策略编辑器展开状态改按套装名保存（`g_slotPolicyOpen`），只展开被选中的那套，JSON 仍 `slotPolicy` / `slotPolicies`；②「从 ESP 生成套装」（`PluginScan` + `EspOutfits`，按 32 槽分桶生成 `<名称>` / `<名称>_1`…，槽位策略「需要已装备」，可选自动收藏）；③「金件」页（`GoldenItems`，独立 `OutfitSystemGoldenItems.json`，可增删、可从 ESP 全量添加、可按 FormID 添加单件）；**服装过滤只用于套装生成**（`IsOutfitArmor()` = 有 EditorID + 非 FX 槽位 + addon 有模型，分桶剔除 `kFxBit`），**金件不过滤**；引擎侧零改动；DLL `OutfitSystemNG.dll` 2,030,592 B / `c72a60a6…` / `1.5.39.0`；**未实机验证** |

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
| 1.5.45 | git tag `v1.5.45`（同一分支 `feature/slot-policy-esp-outfits-golden`；`main` 仍是 1.5.34） |
| 1.5.44 | git tag `v1.5.44`（同一分支 `feature/slot-policy-esp-outfits-golden`；`main` 仍是 1.5.34） |
| 1.5.43 | git tag `v1.5.43`（同一分支 `feature/slot-policy-esp-outfits-golden`；`main` 仍是 1.5.34） |
| 1.5.42 | git tag `v1.5.42`（同一分支 `feature/slot-policy-esp-outfits-golden`；`main` 仍是 1.5.34） |
| 1.5.41 | git tag `v1.5.41`（同一分支 `feature/slot-policy-esp-outfits-golden`；`main` 仍是 1.5.34） |
| 1.5.40 | git tag `v1.5.40`（同一分支 `feature/slot-policy-esp-outfits-golden`；`main` 仍是 1.5.34） |
| 1.5.39 | git tag `v1.5.39`（打在分支 `feature/slot-policy-esp-outfits-golden` 上；`main` 仍是 1.5.34，实机通过后快进合并） |
| 1.5.47 – 1.5.56 | 开发线在 `test/esp-outfit-*` 分支链上（1.5.47 `1a9ff46` → 1.5.50 `6467e43` → 1.5.51 `bf89961` → 1.5.52 `3cda8ec` → 1.5.53 `5f0c47b` → 1.5.54 `8976f40` → 1.5.55/1.5.56 的提交在 `test/slot-unique-bulk-delete-1.5.55`）；**已并入 `main`，tag `v1.5.56`** |
| 1.5.65 | git tag `v1.5.65`（提交 `cde2cab`；含热键录制弹窗、项目/DLL/ini 改名、ESP 加 ESL flag 并瘦到一条能力） |
| 1.5.66 – 1.5.81 | 开发线全在分支 `fix/hotkey-input-core-1.5.71` 上逐版提交、逐版只出 `-test` 包，1.5.81（tag `v1.5.81`）实机通过后并入 `main` |
| 当前 dist | 1.5.81 交付（`SKSE/Plugins/OutfitSystemNG.dll` 2,301,440 B / 1.5.81.0 / `4D443994…`；`interface/translations/SkyrimOutfitSystemNG_{english,chinese}.txt` 各 362 行；zip 大小/哈希见上表） |
| 1.5.56 的 dist（历史） | 1.5.56 交付（分支 `test/slot-unique-bulk-delete-1.5.55`，31 文件 / 3,264,062 B；`SKSE/Plugins/OutfitSystemNG.dll` 2,207,232 B / 1.5.56.0 / `42dcf428…`；`interface/translations/SkyrimOutfitSystemNG_{english,chinese}.txt` 各 341 行；zip 1,413,907 B / `2fdfa421…`） |

1.5.22 的交付目录存档在 `build/dist_1_5_22/`（build/ 不入库）。

## 版本管理规则

- **分支布局（2026-11-26 快照）**：`main` = 1.5.81（tag `v1.5.81`），由 `fix/hotkey-input-core-1.5.71`（1.5.66–1.5.81 的逐版提交，父链从 1.5.65 的 `cde2cab` 起）快进合并；1.5.46 仍封存为 tag `v1.5.46` / `abandoned/esp-outfits-trie-1.5.46`；`archive/main-experiments-20261117` 保留 1.5.34 之前的实验线。
- **分支布局（2026-11-22 快照，历史）**：`main` = 最新发布线，已从 `1.5.34`（`fad3e64`）快进到 **`1.5.56`（tag `v1.5.56`）**；`1.5.35`–`1.5.38` 的实时形变候选废弃为 tag `abandoned/menu-morph-live-1.5.38`；`1.5.39`–`1.5.45` 在 `feature/slot-policy-esp-outfits-golden` 上；`1.5.46` 因乱组被否，代码留 tag `v1.5.46` / `abandoned/esp-outfits-trie-1.5.46`；`1.5.47`–`1.5.56` 在 `test/*` 分支链上按 m08008 测试包协议逐版实机验证，1.5.56 一次性合并收尾。
- 仓库根的 `.gitignore` 忽略 `dist/`、`build/`、`*.dll`、`*.pex`、`*.log`：git 只跟踪源码与文档，交付目录每次由构建生成。
- 每次定稿一个交付版本：先把 `dist/` 收尾，再 `git tag vX.Y.Z`（tag 打在当时那份源码上），随后打包 zip 并更新本文件。
- 回退 = 直接装对应版本的 zip；**不要**在没有源码快照的版本之间做“源码回退”，只能以二进制为准。
- 回退 1.5.17 之后，如果要从源码继续 1.5.17 的行为基线，需要以 1.5.22 的源码为底做行为级裁剪（1.5.18 起的改动集中在 MorphRefresh / HookSupport / Hooks / Patches，以及 09:46–10:41 那批菜单与 i18n 改动），并且必须逐项对照本文件里的版本说明。

## 回退与合并（用户 m06712 要求：随时可回退、可合并有用版本）

**回退（推荐、唯一安全途径）**：装对应版本的成品包即可 —— 成品包与测试包**不入库**（`SkyrimOutfitSystemNG-*.zip` 已写进 `.gitignore`，只留在本地构建机的仓库根，每个包旁有 `.zip.sha256`，内容为 `hash + 两空格 + 文件名`）；`VERSIONS.md` 的上表记录了每个已发布成品包的大小与哈希，可用于核对手上的包是不是原版。想完全干净地回退：删掉 `Data/SKSE/Plugins/` 下的 `OutfitSystemNG.dll` **和** `SkyrimOutfitSystemNG.dll`，再解压目标包覆盖。

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
## v1.5.65（2026-11-25）
- 热键改为点击后弹窗录制（1~4 键和弦，支持手柄），写入 ini 并即时生效。
- 项目/源码目录、DLL 与 ini 统一改名为 SkyrimOutfitSystemNG（日志 OutfitSystemNG.log 与 DLL 同名）；旧 ini 首次保存自动改名。
- dist/SkyrimOutfitSystem.esp 加 ESL flag 并精简为一条 Change Outfit 能力（MGEF/SPEL/QUST）；删掉 MCM/自动切换/战斗检测脚本与记录。
- 成品包 SkyrimOutfitSystemNG-1.5.65.zip。

## v1.5.81（2026-11-26）
- 热键录制窗口照 Eelta/FreeClimb 的集合语义重做：组合键全部松开才确认；录制期间键盘与手柄输入由插件独占（设备 Poll 里从 `BSInputEventQueue` 摘事件），Esc 不再关 SMF 主菜单、手柄 B/LB 不再漏给宿主，LT/RT 能录；手柄组合按按下顺序显示。
- 热键状态改为直接问设备（`GetAsyncKeyState` / `XInputGetState`），修掉「只按 Alt 就触发 Alt+O」的残留 bug；`PadState` 与 `XINPUT_GAMEPAD` 对齐并加 `static_assert(sizeof == 16)`。
- 能力键与快速选择键只在「Change Outfit」被选中时归本插件，键盘与手柄同一判定（修键盘半边无条件开窗的回归）。
- 快速选择窗口 Esc / Tab 都能关闭；底部键位提示按键盘 / 手柄分行，窗口窄时自动折行。
- 关闭框架默认 logger：不再多出只有一行版本号的 `SkyrimOutfitSystemNG.log`。
- 钩子与别的插件共存：别的插件抢先钩住幻化显示函数时顺着跳转链跟上去钩（真实案例 InventoryLagFix.dll）、够不着就在站点旁分配远跳桩、5 字节分支自写不再经 `Trampoline::write_branch`（修 AE 启动即崩的 `displacement is out of range`）、写完读回并在 data loaded / post load game 各复查一次、被覆盖时点名接管的插件、幻化确实装不上时弹提示框。
- 成品包 SkyrimOutfitSystemNG-1.5.81.zip（1,454,541 B / `35C5F0DE58E69FAB4A56927BDFFC4DA75D5795D73DAAB3C6513B0B59DC47D601`）。
