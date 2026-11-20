# 交付说明（DELIVERY）

## 1.5.39 最新交付准备（2026-11-20）

**验证状态（1.5.39）**：**尚未实机验证**，不能声称菜单新功能可用。静态侧：Release 全量重编 `BUILD_EXIT=0`（0 error / 0 warning，日志 `build/build_task.log`，留档 `build/build_1_5_39.log`）；新增守卫测试 `tests/esp_outfits_filter.ps1` 通过，`scene_outfit_switch` / `skinning_stale_slots` / `morph_paths` / `morph_read_only` / `morph_slider_queue` / `morph_thread_gate` 六项既有守卫仍全部 PASS。

| 最终产物复核项 | 状态 |
| --- | --- |
| DLL 大小 / SHA256 / FileVersion | 2,030,592 字节；`c72a60a6f4a400bbb4cf06a9b2e1c81a86baca487fbf901119e264421aee83da`；`1.5.39.0`（build/Release 与 dist 逐字节一致） |
| 安装包与复核记录 | SkyrimOutfitSystemNG-1.5.39.zip：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小/哈希只写包外 `VERSIONS.md` 与同名 `.zip.sha256`，包内不自引用 |

- **本版只动菜单与扫描层，不动引擎侧**：贴件语义、槽位清理、`.tri` 推导、钩子安装、`SceneOutfits` 一行未改（`git diff v1.5.34 HEAD -- src/cpp/hooking src/cpp/MorphRefresh.cpp src/cpp/MorphRefresh.h src/cpp/SceneOutfits.cpp` 为空），1.5.25 实测正常的基础幻化与 1.5.34 已验收的场景套装切换都不受影响。
- **逐槽策略编辑器只对被选中的套装展开**（m09191②）：旧实现是一个全局布尔，勾一次就让所有套装的策略编辑器一起展开；现在展开状态按套装名保存在 `std::unordered_set<std::string> g_slotPolicyOpen` 里（`src/cpp/MenuOutfitEditor.cpp`），导出/导入 JSON 的键仍是每套自己的 `slotPolicy` / `slotPolicies`，文件格式未变。
- **新增「从 ESP 生成套装」**（m09191③）：幻化页底部选择 ESP → 扫描该 ESP 的护甲 → 同名槽位拆成 `<名称>_1`、`<名称>_2` …，槽位策略统一「需要已装备」，生成后列表焦点跳到第一套生成的套装；变体超过 32 种时只取前 32 并在提示里说明。
- **新增「金件」页**（m09191④）：独立数据文件 `Data\SKSE\Plugins\OutfitSystemGoldenItems.json`（与幻化 JSON 分开），左侧按 ESP 分组、右侧增删单件，支持「添加某个 ESP 的全部装备」与「按 FormID 添加单件」（输入的件必须真实存在）；旧 `SkyrimOutfitSystemSE.ini` 里的金件列表在首次加载时迁移一次。
- **扫描只收正常服装**（m09426）：`PluginScan::IsOutfitArmor()` 要求记录有 EditorID、覆盖至少一个非 FX 槽位（FX = 显示掩码第 31 位 / CK 槽 61）、且至少一个 armor addon 有模型；同时把 FX 位从分桶里剔除，避免 FX 特效被当成服装槽。**金件不受这三条限制**：FX 件、没有 EditorID 的件都能加进金件列表。
- **设置页新增「套装生成」段**：`[Customization] EspOutfitAutoFavorite`（默认打开）＝从 ESP 生成的套装自动收藏。

## 1.5.34 交付准备（历史，已被 1.5.39 取代）

**验证状态（1.5.34）**：**尚未实机验证**，不能声称已修好。静态侧：Release 全量重编 `BUILD_EXIT=0`（0 error / 0 warning，日志 `build/build_task.log`，留档 `build/build_1_5_34.log`）；`OutfitSystemNG.dll` 1,937,408 字节 / `e1cdb951…` / `1.5.34.0`（build/Release 与 dist 逐字节一致）。

| 最终产物复核项 | 状态 |
| --- | --- |
| DLL 大小 / SHA256 / FileVersion | 1,937,408 字节；`e1cdb9518149d0e1f7a23d1835317620b8258c1e030d6d987b658e4718283782`；`1.5.34.0`（build/Release 与 dist 一致） |
| 安装包与复核记录 | SkyrimOutfitSystemNG-1.5.34.zip：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小/哈希只写包外 VERSIONS.md 与同名 .zip.sha256，包内不自引用 |

- **真因（1.5.33 的线程门禁丢事件）**：用户日志 `v1-5-33-0` 里 `SceneOutfits` 只有 4 行，其中三条是 `'ostim_start' arrived off the game thread; the scene switch is skipped.`（20:37:33 线程 39516、20:38:11 线程 10604、20:38:29 线程 27040）。OStim / SexLab 的 mod 事件是从工作线程投过来的，所以「切套装」这一步压根没执行 —— 名字没查、`setOutfitByName()` 没调、`refreshArmorFor()` 没跑。
- **修法（一个 task 交接，不建队列）**：`ModEventSink` 里离线程事件改走 `SKSE::GetTaskInterface()->AddTask([eventName]() { Dispatch(eventName); });`，把换装送到游戏线程上执行；仍在游戏线程时保留内联快路径直接 `Dispatch(eventName)`。没有队列、没有 `Tick()`、没有 generation 计数 —— 与 `AutoSwitch::Tick` 回主线程的做法一致（m07666 要求的那条路）。
- **取证手留了线程号**：离线程时打 debug `SceneOutfits: '{}' came from thread {} (game thread {}); the switch is handed to the game thread.`，下一轮日志可直接看出事件来自哪个线程、主线程是哪个。
- **换装调用序列一个字没改**：仍是 `setOutfitByName()`（锁内指派）→ 放锁 → `OutfitSystem::refreshArmorFor()`，与「按地点/状态切换」同构；`AutoSwitch.cpp` / `AutoSwitch.h` 本版仍为空 diff。
- **未动的东西**：基础幻化钩子（`Hooks.cpp`）、注入壳、贴件语义、槽位清理策略、`.tri` 目标推导、DLL 文件名（`OutfitSystemNG.dll`）一律未改；1.5.25 实测正常的基础幻化与空幻化不受影响。
## 1.5.33 交付准备（历史，已被 1.5.34 取代：其 `OnGameThread()` 门禁把工作线程送来的场景事件整条丢弃）

**验证状态（1.5.33）**：**尚未实机验证**，不能声称已修好。静态侧：Release 全量重编 `BUILD_EXIT=0`（0 error / 0 warning，日志 `build/build_task.log` 149 行，留档 `build/build_1_5_33.log`）；`OutfitSystemNG.dll` 1,934,848 字节 / `8c23e5ee…` / `1.5.33.0`（build/Release 与 dist 逐字节一致）。

| 最终产物复核项 | 状态 |
| --- | --- |
| DLL 大小 / SHA256 / FileVersion | 1,934,848 字节；`8c23e5eec3ae0f4561125ab2cabb71e63ef7c21d01e6a8b85755687454716aaf`；`1.5.33.0`（build/Release 与 dist 一致） |
| 安装包与复核记录 | SkyrimOutfitSystemNG-1.5.33.zip：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小/哈希只写包外 VERSIONS.md 与同名 .zip.sha256，包内不自引用 |

- **按用户要求回退到 1.5.29 再改**（m07666）：源码基线 `git checkout v1.5.29 -- src`，只留「场景套装」这一件事的改动。1.5.30 的补跑 pass、1.5.31 的 `rebuildActorModel()`、1.5.32 的 `Features::AssignOutfit()` 场景入口全都不在 1.5.33 里。
- **场景专用机件删干净**：`SceneOutfits.cpp` 里的 SKSE 任务队列（`QueueSceneEvent` / `DrainSceneEvents` / `ScheduleDrain` / `g_generation` / `g_pendingEvents` / `g_drainScheduled`）、`Tick()` 心跳，以及 `main.cpp` / `NGHotkeys.cpp` 两处调用全部删除。
- **换装方法照抄「按地点/状态自动切换」**：新增 `OutfitSystem::setOutfitByName()`，与 `setOutfitUsingState()` 并排；场景开始/结束都走同一个两步序列 —— 取套装库锁 → 指派名字 → 放锁 → `OutfitSystem::refreshArmorFor(player)`，与 `AutoSwitch::Evaluate()` 的两步一一对应（见 §4.61）。
- **没改「按地点/状态切换」**：`git diff v1.5.29 HEAD -- src/cpp/AutoSwitch.cpp src/cpp/AutoSwitch.h` 输出为空，`AutoSwitch` 从 1.5.29 到本版一行未动。
- **未动的东西**：基础幻化钩子（`Hooks.cpp`）、注入壳、贴件语义、槽位清理策略、`.tri` 目标推导、DLL 文件名（`OutfitSystemNG.dll`）一律未改；1.5.25 实测正常的基础幻化与空幻化不受影响。

## 1.5.32 交付准备（历史，已被 1.5.33 取代；其 `Features::AssignOutfit()` 场景入口被用户否决）

**验证状态（1.5.32）**：**尚未实机验证**，不能声称已修好。静态侧：Release `BUILD_EXIT=0`（0 error / 0 warning，日志 `build/build_task.log`，留档 `build/build_1_5_32.log`）；`OutfitSystemNG.dll` 1,937,920 字节 / `da9ec899…` / `1.5.32.0`（build/Release 与 dist 一致）。

| 最终产物复核项 | 状态 |
| --- | --- |
| DLL 大小 / SHA256 / FileVersion | 1,937,920 字节；`da9ec8995c599f00cb249b611d66a13c2f3f030f845951b3d6b5eacdca05f875`；`1.5.32.0`（build/Release 与 dist 一致） |
| 安装包与复核记录 | SkyrimOutfitSystemNG-1.5.32.zip：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小/哈希只写包外 VERSIONS.md 与同名 .zip.sha256，包内不自引用 |

- **用户指令照做**：删掉 1.5.30 的「补跑装备 pass」与 1.5.31 的 `rebuildActorModel()`（`Actor::Update3DModel()`）—— 两者都已被用户实机否掉。场景套装现在只做「找对名字 → 切套装」，切换入口仍是 `Features::AssignOutfit()`（= 点套装列表那条函数，`MenuUI::ApplyTo` 用的同一个）。
- **对齐按地点/战斗切换**：场景事件在游戏线程上**同步**执行切换（与 `AutoSwitch` 的 `TESActorLocationChangeEvent` / `TESFastTravelEndEvent` sink 同构），不再绕 `AddTask` 队列；名字查找先试 `<当前套装>_sex`，再在全部套装名里做忽略大小写匹配。
- **未动的东西**：基础幻化钩子（`Hooks.cpp`）、注入壳、贴件语义、槽位清理策略、`.tri` 目标推导、DLL 文件名一律未改；1.5.25 实测正常的基础幻化不受影响。

## 1.5.31 交付准备（历史，已被 1.5.32 取代，且其「重建 3D」修法已被用户否决）

**验证状态（1.5.31）**：**尚未实机验证**，不能声称已修好。静态侧：Release `BUILD_EXIT=0`（0 error / 0 warning，日志 `build/build_task.log`，留档 `build/build_1_5_31.log`）；`OutfitSystemNG.dll` 1,941,504 字节 / `c8c8ae5d…` / `1.5.31.0`（build/Release 与 dist 一致）。

| 最终产物复核项 | 状态 |
| --- | --- |
| DLL 大小 / SHA256 / FileVersion | 1,941,504 字节；`c8c8ae5db004c0017b029110b23dd707e1e519789c2acf81ba637a5ed63ad6b2`；`1.5.31.0`（build/Release 与 dist 一致） |
| 安装包与复核记录 | SkyrimOutfitSystemNG-1.5.31.zip：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小/哈希只写包外 VERSIONS.md 与同名 .zip.sha256，包内不自引用 |

- **用户指令照做**：删掉 `SceneOutfits` 里自己拼的 `set_outfit_c` + `refreshArmorFor`，场景开始/结束一律改调 `Features::AssignOutfit()` —— 正是「点套装列表」走的那条函数（`MenuUI::ApplyTo` 用的同一个）。
- **补上真正缺的一步**：1.5.30 证明「多跑几次装备 pass」不能让模型出现；让模型出现的是引擎重建 3D。新增 `OutfitSystem::rebuildActorModel()`（`Actor::Update3DModel()`），在 3 次随访中的第 2 次之前调用一次。
- **未动的东西**：基础幻化钩子（`Hooks.cpp`）、注入壳、贴件语义、槽位清理策略、`.tri` 目标推导、DLL 文件名一律未改；1.5.25 实测正常的基础幻化不受影响。
## 1.5.29 交付准备（历史，已被 1.5.31 取代）

**验证状态（1.5.29）**：**尚未实机验证**，不能声称已修好。静态侧：Release `BUILD_EXIT=0`（0 error / 0 warning）；`tests/*.ps1` 共 14 条源码守卫 12 条通过，2 条失效断言是既有问题（`hair_tint_read_only.ps1` 的锚点 `tasks->AddTask([retryProviders]()` 在 HEAD 已被重构掉、`skse_listener_replay.ps1` 硬绑旧插件索引 139/132），与本次改动无关；新增 `tests/skinning_stale_slots.ps1` 通过。

| 最终产物复核项 | 状态 |
| --- | --- |
| DLL 大小 / SHA256 / FileVersion | 1,936,384 字节；`4f0bcae1e5926763c3d9592ab01812c8f7b968d6e8c7ef37d0ac002c3cb84c92`；`1.5.29.0`（build/Release 与 dist 一致） |
| 安装包与复核记录 | SkyrimOutfitSystemNG-1.5.29.zip：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小/哈希只写包外 VERSIONS.md 与同名 .zip.sha256，包内不自引用 |

- **只改两处**：①离开幻化集的残留克隆改成「只拆我们自己的」窄清（1.5.25 的交回引擎在引擎没有真甲可重建时会留下上一套的幻化件）；②主线程新增幻化几何对账，发现幻化槽没有几何就用已验收的 `OutfitSystem::refreshArmorFor` 重建。基础幻化钩子（`Hooks.cpp`）、贴件语义、槽位清理策略一律未动，1.5.25 已实测正常的行为不受影响。
- **框架无关**：对账与重建里没有任何 OStim / SexLab / SAM / OBody / RaceMenu / SLIF 名字，只依赖「幻化集里有这个槽、但 biped 槽里没有 `partClone`」这一自身状态；守卫测试会拦住后续改动引入框架分支。
- **崩溃未实证**：同批日志里的 `EXCEPTION_ACCESS_VIOLATION`（SkyrimSE.exe+12BACD7，读 `[rax+0xF4]`，RAX=0，发生在 `hdtSMP64` → 渲染/剔除阶段）与本插件的关系**没有证据**，本版不加任何「防崩」逻辑，只记录分析与判据，见 `docs/CRASH_221544_AUDIT.md`。

## 1.5.28 交付准备（历史，已被 1.5.29 取代）

**验证状态（1.5.28）**：尚未实机验证，不能声称已实机修好。生产函数路径回归 6/6 通过；最终 Release BUILD_EXIT=0，DLL 版本/哈希与交付副本一致。包内逐文件清单见 SHA256SUMS.txt；最终 zip 大小、哈希及解包复核记录见包外 VERSIONS.md 与同名 .zip.sha256，避免自引用。

### 1.5.28 新静态验证记录

- Release 构建：`BUILD_EXIT=0`；最终 DLL 的 `build/Release` 与 `dist` 副本逐文件一致，大小、SHA256、FileVersion 见下表。
- 生产函数路径回归：6/6 通过。上述静态结果不证明 TRI shape 匹配、顶点移动或游戏内实时跟随。
- FileVersion/ProductVersion 均为 1.5.28.0；dumpbin 确认 SKSEPlugin_Load/Query/Version 三个导出，依赖仅 VERSION/KERNEL32/USER32/ole32/SHELL32 系统 DLL。未在 Skyrim 中加载验证。

| 最终产物复核项 | 状态 |
| --- | --- |
| 最终 DLL 大小 / SHA256 / FileVersion | 1,878,016 字节；`fe5c79b5b1f1e1fc3153870dd6b61cf293081805cab407ad3ea41ed1621bb553`；`1.5.28.0`（主 agent 复核；build/Release 与 dist 一致） |
| 安装包与复核记录 | SkyrimOutfitSystemNG-1.5.28.zip；包外 VERSIONS.md 记录最终大小/条目数/逐文件复核；zip SHA256 只写包外，包内不自引用 |

- **修正资源路径**：1.5.27 日志里的模型路径已经带 `meshes` 前缀，`TriFileExists()` 又加一次，实际探测成 `meshes\meshes\...`；1.5.28 规范化模型/TRI 路径，资源打开路径只保留一次 `meshes` 前缀。
- **收紧 BODYTRI 诊断**：不再只按 extra data 名称匹配；必须是 `NiStringExtraData`、路径非空且对应资源可打开才计数。日志改为 `has readable BODYTRI resources on N of M...`，并明确 `paths only, not proof of TRI shape matching or morph movement`。N/M 只是路径资源可读性，不是 TRI shape 名匹配、顶点移动或实时跟随的成功证据；即使 N=M 也不能宣告修好。
- **分离模型缓存**：以实际 addon（ARMA）+ sex + 1P/3P 区分缓存；1P 选择 `bipedModel1stPersons`，3P 使用第三人称模型，避免多 ARMA、性别与视角之间串用路径。
- **保留有效回调**：去掉会误吞所有 biped 回调的 `IsIllusionClone` 过滤，只用 `selfWrite` 防止自家写入重入。provider 调用前采集几何签名，再作调用后比较，避免把已更新的状态当基线。
- **自动刷新不摘幻化**：自动 morph 刷新不再 `CycleIllusion` 摘掉再穿回，而排队 `RequestBake`；不改基础幻化钩子、不清身体槽，也不以重新穿脱作为自动兜底。
- **共享 extra data 安全性**：规范化已有 BODYTRI 时，不原位改写共享字符串；为该节点创建新的 `NiStringExtraData` 替换，持有 `NiPointer` 保留原对象，替换失败时恢复。`GetBodyMorphs` 等未使用虚槽的声明签名纠正，虚表槽顺序不变。
- **能力边界**：主机制仍依赖 RaceMenu 自己的 morph 遍历能访问克隆件、TRI shape 能匹配。拿到且通过版本门禁的接口时才可额外重放；无接口重放不能保证实时，轮询/路径可读日志都不能替代实机顶点与外观验证。

### 握手协议与安装侧排障

SKSE64 2.0.20 的 `RegisterListener(nullptr)` 只订阅注册当时已有的 sender 列表（含当时预留槽），不是对未来 sender 的持续订阅；skee 的 `[Features] bEnableEarlyRegistration=1` 若使其过早注册，可能漏掉后加载的 SOS。`QueueProviderRetry()` 只重发消息，不能补建 skee 对 SOS 的订阅关系。公开配置修法：检查**实际游戏生效的** `Data/SKSE/Plugins/skee64.ini`，若 `[Features] bEnableEarlyRegistration=1`，由用户改为 `0` 并完全退出、重启游戏；不自动修改，也不打包用户 ini。当前没有本次 `skse64.log`，不能断言本次配置必为 `1`。协议依据：[SKSE v2.0.20 PluginManager.cpp L488–616](https://github.com/ianpatt/skse64/blob/v2.0.20/skse64/PluginManager.cpp#L488-L616)。

实机验收需同时覆盖基本幻化/身体槽、1P/3P、多 ARMA、RaceMenu/OBody/SLIF、无可见中间态、界面关闭重开和 skee API 版本门禁；见 TESTING 的 1.5.28 章节。

> **1.5.28 纠正（历史说明，不作当前验收依据）**：旧文档将握手归因于“仅消息嵌套”/receiver 名字错位的解释已纠正，实际是注册时机导致 sender 订阅遗漏；主线程 Queue 重试并非完整修法。旧 `carries a .tri morph target on N of M`、`N > 0` 或 `N = M` 判定均不充分，不能证明 TRI shape 匹配或 morph 顶点移动；旧实时成功措辞是当时目标/假设而非实机结论。当前以 1.5.28 最新章节为准。

## 1. 交付信息（1.5.27 历史记录，非 1.5.28 产物）

| 项 | 值 |
| --- | --- |
| 版本 | 1.5.27 |
| 构建时间 | 2026-11-17 |
| 主 DLL | SKSE/Plugins/SkyrimOutfitSystemNG.dll（1,875,456 字节，sha256 22eb7794af94dd34d98f195638e48d48699e4381046a8790c787f09a91af178f） |
| 支持运行时 | Skyrim SE 1.5.97（SKSE64 2.0.20）与 AE 1.6.1130+（含 1.6.1170 / 1.6.1179 / 1.6.640），VR 不支持 |
| 构建链 | VS2022 14.44 + CMake 4.2 + vcpkg（x64-windows-static，baseline ee12231b）+ CommonLibSSE-NG（ng 分支） |
| 前置 | SKSE64 + Address Library；SKSE Menu Framework 3.x 可选（只影响菜单与快速选择窗口） |
| 安装包 | SkyrimOutfitSystemNG-1.5.27.zip（31 个条目 = 30 个文件 + SHA256SUMS.txt） |
| 安装包哈希 | 见包外同名 `.sha256` 文件（包内文档无法写入包自身的哈希）；包内逐文件哈希见 SHA256SUMS.txt，30 条全部复核通过 |

## 2. 包内容

| 路径 | 说明 |
| --- | --- |
| SKSE/Plugins/OutfitSystemNG.dll | 插件本体（1.5.30 起文件名；插件身份仍是 SkyrimOutfitSystemNG，旧名不得留在 Data 里） |
| SKSE/Plugins/SkyrimOutfitSystemSE.ini | 默认配置（缺失时自动使用随包默认值） |
| interface/translations/SkyrimOutfitSystemNG_english.txt | **可编辑翻译文件（英文，248 条，UTF-16LE，29,016 字节）**，见 `docs/I18N.md` |
| interface/translations/SkyrimOutfitSystemNG_chinese.txt | **可编辑翻译文件（中文，248 条，UTF-16LE，21,530 字节）** |
| Scripts/*.pex（**6 个**） | 快捷槽、战斗侦测、自动切换、快捷槽管理、MCM 空壳、**SkyrimOutfitSystemNativeFuncs** |
| Scripts/Source/*.psc（6 个） | 对应源码，其中 SkyrimOutfitSystemNativeFuncs.psc 含 **66** 个原生函数声明 |
| SkyrimOutfitSystem.esp | 原版 ESP（法术 / 战斗侦测 / quest 绑定） |
| interface/translations/skyrimoutfitsystem_english.txt | 翻译 |
| docs/ | 10 篇维护文档（1.4.0 新增 I18N.md） |
| README.md / LICENSE.md | 使用说明与许可 |
| SHA256SUMS.txt | 逐文件哈希 |

包内**不含** SKSE Menu Framework 与任何第三方 mod（前置由用户自行安装；插件在框架缺失时照常加载）。

## 3. 需求对照

| 需求 | 实现 |
| --- | --- |
| 迁移到 CommonLibSSE-NG，兼容 SE + AE 1.6.1170+ | 单套 REL::RelocationID(seID, aeID) 地址表（src/cpp/hooking/Hooks.cpp），运行时由 Address Library 选表 |
| 在原 SkyrimOutfitSystemSE 基础上改造 | 原 C++ 层 / Papyrus 层 / ESP / psc / 翻译全部保留，仅 Rust 数据层等价改写为 src/core |
| 舍弃 MCM，改用 SKSE Menu Framework | MenuUI.cpp（Outfits / Actors / Settings / Quick picker），MCM 脚本改空壳 |
| 1000+ 套装浏览 / 搜索 / 快速定位 / 翻页 | 名字搜索 + 仅收藏 + 分页（5–500/页）+ 双击应用 + 结果签名缓存 |
| 随机换装（可仅收藏） | DIK 组合键（默认 Ctrl+Shift+O），边沿触发；手柄组合键尚未实现（见 §7） |
| 快捷槽脚本支持 >100 页 | 改为原生快速选择窗口（搜索 + 分页），不再用 Papyrus 列表菜单 |
| 针对特定 mod 优化（1.1.0 新方向） | Procedural Armor Damage 与 Advanced Nudity Detection 的**零 hook** 兼容层，见 §4.2 / §4.3 |
| 菜单中英双语（1.2.0） | Settings → Language（auto / en / zh），`[UI] Language`，见 §4.6 |
| 原版 MCM 细节功能全部补齐（1.3.0） | 套装编辑器（四种添加方式 / 冲突确认 / 32 槽策略）、Apply to 目标选择、附近角色添加、快捷槽法术开关；逐控件对照见 `docs/MCM_PARITY.md`、§4.7 |
| 外置可编辑翻译文件（1.4.1 改版） | `Data/interface/translations/SkyrimOutfitSystemNG_<语言>.txt`，**每种语言一个文件**（UTF-16LE，`$英文<TAB>译文`），目录里多一个文件就多一个语言选项；缺失时自动写全量模板；菜单内 Reload，见 `docs/I18N.md` |
| 左侧菜单项也走翻译（1.4.0） | Quick picker / Outfits / Actors / Settings 由 `I18n::Tr` 生成，重启游戏后生效（框架缓存标题） |
| 快速选择窗口可操作化（1.4.0） | 640×800 居中自绘窗口；W/S/↑/↓/手柄十字上下选择、A/D/←/→/十字左右翻页（长按加速）；E/Enter/A 穿上、R/Y 随机、Esc/B 关闭；焦点记忆当前穿着与随机结果；底部 Remove outfit |
| Outfits 列表显示不全（1.4.0） | 行宽随文本撑开 + 横向滚动条 + Settings → Interface 的 List width 持久化 |
| 每页数量持久化（1.4.0） | `[UI] OutfitPageSize` / `QuickPickerPageSize`，两个面板的滑条直接读写 ini |
| PAD 在幻化外观上不生效（1.4.0） | `Custom()` 贴完把 `BIPOBJECT.item` 还原为真实护甲（诚实还原），PAD 的 CollectWorn/ScanScene 才认这些槽，见 §4.10 |
| 快选窗位置固化（1.4.1） | `[UI] QuickPickerX/Y/Width/Height`，拖动结束写回 ini，重启后回到原位，见 §4.12 |
| v4 老存档槽位策略错认（1.4.1） | `LegacySave` 解析 `OBSOLETE_slot_policy`(7) / `OBSOLETE_slot_policies`(6) 数字策略并先重置为原版初始态，见 §4.13 |
| 原版 JSON 导入导出（1.4.1） | `OutfitSystemData.json` 按原版 protobuf JSON 结构（lowerCamelCase）读写；Papyrus 两个函数 + 菜单 Settings → Outfit data 两个按钮，见 §4.14 |
| 补齐界面文案翻译（1.4.1） | 词典 153 → 224 条：12 条策略文案（含“需要已装备”/“始终使用套装”）、32 条槽位名、导入导出与错误提示、拼接前缀，见 §4.15 |
| 维护文档 + 编译产物 | 本文 + 9 篇文档 + 可安装包 |
| 快速选择窗口不居中/不记忆/ini 无效、“Debug”标题（1.4.2） | 不再用 `GetCurrentWindow` 判断，独立窗口永远自绘；`ImGuiCond_Always` 套用 ini 矩形；可选 ImGui 符号逐项绑定 —— §4.16 |
| JSON 策略被认成“从不显示任何东西”（1.4.2） | `NormalizePolicyCode()` 认四字符码 / 翻译键 / 中英标签（含新旧“套装/幻化”措辞）—— §4.17 |
| 槽位文案带编号、中文“套装”→“幻化”（1.4.2） | `头部（槽位 0）` …；7 条策略文案改“幻化” —— §4.18 |
| 快选长按太迟钝（1.4.2） | 首次延迟 0.22 s、最小间隔 0.030 s —— §4.16 |
| 槽位号应是 30–61 而非 0–31（1.4.3） | `kDisplaySlotOffset = 30`，并按用户表重写 32 个槽位名 —— §4.19 |
| JSON 导入导出仍错、要按原版结构（1.4.3） | `slotPolicy/slotPolicies` 读写都用原版数字枚举，导入兼容数字/四字符码/翻译键/标签 —— §4.20 |
| Outfits 删除按钮点了没反应（1.4.4） | 原先只把名字存进 `g_pendingDelete` 没人消费；现在有关闭确认条、`delete_outfit()` 真正执行、日志与外观刷新 —— §4.21 |
| 槽位列表/逐槽策略无横向滚动，长名挡住（1.4.4） | 三处子窗口加 `ImGuiWindowFlags_HorizontalScrollbar`，行宽按 `CalcTextSize` 撑开，策略下拉固定宽度 —— §4.22 |
| 默认日志太吵（1.4.4） | 65 条诊断日志降为 `debug`，只在 Settings 勾选 Verbose logging（`[Debug] ExtraLogging=true`）后输出；info 只保留生命周期与用户主动操作 —— §4.23 |
| 地点/状态自动切换改原生（1.4.4） | 新增 `AutoSwitch`：地点变化 / 快速旅行事件 + 输入心跳 2 秒兜底；Papyrus 触发器改为空实现 —— §4.24 |
| 状态套装新增 `<随机>`（1.4.4） | 13 个状态的下拉在 `<继承>` 之后多一项 `<随机>`；进入该状态随机抽一套，同状态不重抽 —— §4.25 |
| Outfits 列表按钮点不中、光标先选中条目（1.5.0） | Selectable 只占名字区，按钮从独立 x 坐标开始，不再重叠；Wear / 星标 / Delete 都能直接点 —— §4.26 |
| 快选窗口手柄出现两个光标，按 A 同时换装和取消幻化（1.5.0） | 独立窗口加 `NoNavInputs` + `NoNavFocus`，框架不再移动第二焦点；窗口自己的键位处理不变 —— §4.27 |
| 切换幻化时自动重置 SMP，且只重置对应角色（1.5.0） | `[Compatibility] AutoResetSMP`；`DynamicHDT.ResetPhysics(actor, false)` 逐角色 soft reload，不是全局 reset —— §4.28 |
| SexLab / OStim 场景自动换 `_sex` 套装（1.5.0） | `[Compatibility] SceneOutfitSwitching`；场景开始查“当前套装名 + _sex”，已带后缀则不变、找不到则保持；场景结束恢复 —— §4.29 |
| 9 小时无人值守自查 bug / 优化性能（1.5.1） | 菜单 revision 缓存、Outfit 槽位/策略缓存、护甲搜索缓存、短锁、设置保存节流；修复列表不刷新、页码越界、热键 latch、快选跨线程刷新等问题 —— §4.30 |
| 继续无人值守自查 / 性能优化（1.5.2） | 输入线程只排队、AutoSwitch/热键回主线程；菜单空指针防护与 revision；场景重叠；守卫栈化、搜索零分配、列表复用、编辑器 500ms 缓存 —— §4.31 |
| 快速选择窗口加 X 键取消幻化（1.5.7） | 键盘 X / 手柄 X 与 Remove outfit 按钮调用同一个 `QuickRemoveOutfit()`，文本输入时不响应 —— §4.35 |
| 带 .tri 的幻化装备不实时刷新（1.5.7） | `MorphRefresh` 轮询 RaceMenu BodyMorph 签名，变化时清空幻化槽并重建 partClone，再延迟 `ApplyBodyMorphs` —— §4.36 |
| 套装编辑器身体槽位“移除”按钮无反应（1.5.8） | 确认条从编辑器顶部改到被点击行内，长名称按钮按可视宽度 clamp，删除成功写 info 日志 —— §4.37 |
| OBody 换身形后只有上衣刷新、裤子等幻化件穿模（1.5.9） | 强制重建范围从 applySet 扩大到整个 displaySet 的并集；真实穿戴但属于显示集的件也清槽重贴 —— §4.38 |
| OBody 换预设后幻化仍不刷新、且环境没有 RaceMenu（1.5.10） | `MorphRefresh` 增加 OBody 插件 API 后端：轮询 `GetPresetAssignedToActor` 预设名签名，变化时 OBody-only 先 `ApplyOBodyMorphsToActor` 再强制重建 displaySet —— §4.39 |
| OBody 换预设后其余幻化件仍不刷新、穿模（1.5.12） | 500 ms 延迟二次重建（等 OBody 稳定）+ OBody 后端重放整套 morph + RaceMenu 接口每 2 秒重试（最多 30 次） —— §4.41 |
| 手动重穿当前套装（Z / 快选窗 E）不刷新幻化（1.5.14） | revision 变化后 2 秒内全量重建（ClearSlots + 重贴 displaySet） —— §4.42 |
| OBody 落定晚于重建、仍穿模（1.5.14） | 400 ms / 1400 ms 两段延迟重建，强制窗口 4 秒 —— §4.42 |
| 幻化循环闪出原版装备且卡顿（1.5.15） | 两阶段合并进同一主线程任务，中间不渲染任何一帧；Z 键直接走循环 —— §4.44 |
| 上述三条身形症状 1.5.18 一条都没解决（m21255），且 RaceMenu 接口从未接上（1.5.19） | skee 握手不再放弃（心跳 5 秒×8 → 每 30 秒直到结束 + `kPostLoadGame` + 用时惰性重试）、失败诊断区分「没装 / 装了不答」、几何探针改为递归子树采样、变化先交 provider 直接刷新（`ApplyBodyMorphs` / `ApplyOBodyMorphsToActor`）并在 4 × 350 ms 内校验，未动才回退幻化循环、删掉 700 ms 二次重建、新增 `[Compatibility] MorphProviderFirst`（详见 §4.48） |
| RaceMenu 拖 Body Morph 滑块、OBody 换预设、SLIF 膨胀三种身形变化对幻化不跟随，且 OBody 换预设变卡（1.5.18） | 新增与供应商无关的几何签名（对 partClone 顶点缓冲做 FNV）+ SLIF 10 个滑块进签名 + 删掉 400/1400 ms 两段冗余重建（改单次 700 ms）+ 循环后 2.5 秒 quiet 窗口防自触发 + 无 provider 也轮询 —— §4.47 |
| SLIF NG 膨胀后幻化不跟随、穿模（1.5.17） | 修 skee 取接口（命名失败后无名广播重试）+ 新增 SLIF NG 直连查询后端（读 4 个规范膨胀键的 applied 值并入签名）+ 签名静置 250 ms 才重建 —— §4.46 |
| 幻化栏要能复制/粘贴整套预设（1.5.16） | Copy 把选中套装的名称 / 部件 / 基础策略 / 32 槽覆盖写成原版 JSON 进剪贴板；Paste 读剪贴板，重名自动 `_copy`，创建完左侧列表自动选中并滚动到该条 —— §4.45 |

| RaceMenu 拖 Body Morph 滑块 / OBody 换预设 / SLIF 膨胀时幻化件身形卡住不动（m00002 / m00270，1.5.20） | 幻化 clone 不经过 skee 的 attach 钩子、没有 `BODYTRI` 额外数据，RaceMenu 的 morph 走不到它；1.5.20 给每个幻化槽补 `.tri` 目标（按 ARMA 模型路径推导）+ 订阅 skee v5 `MorphShapeCallback` 与 OBody `Obody_ApplyMorph` 事件，在主线程用 `ApplyVertexDiff(…, false)` 幂等重放 morph，不摘幻化、不重建（详见 §4.49） |
| 1.5.20 装上后幻化完全不显示、角色只剩裸体（m00746） | `CustomSkinPlayer` 钩子站点验证失败被禁用（该 exe 那处不是那条 call），而 `DontVanillaSkinPlayer` 仍在藏真护甲 ⇒ 真护甲被藏、幻化件无人贴。1.5.21：①站点对不上时在 ±0x800 内自定位唯一那条到 `InventoryChanges::ExecuteVisitorOnWorn` 的调用（含全 `.text` 唯一调用点兜底，多候选拒绝）；②两个皮肤钩子没都装上就 `SetIllusionDisplayEnabled(false)`，`ShouldOverrideSkinning` 恒 false，原版流程照跑、**显示真实护甲而不是裸体**；③`ShouldOverrideSkinning` 先用 `VirtualQuery` 验指针再 `skyrim_cast`；④失败时打印 exe 身份、站点字节窗口与全 `.text` 调用点统计（详见 §4.50） |
| 1.5.21 装上后幻化依旧失败（m01170），用户指出「让你在 CommonLibSSE-NG 基础上改，不是让你专门去钩 1597」 | 1.5.21 的站点仍是写死偏移（`24231 + 0x81`，抄自上游 1.5.73 的注释），只是失败后在 ±0x800 内找替身，本质仍是给某个具体运行时量身定做。1.5.22 把站点定位整个换掉：`REL::RelocationID(seID, aeID)` 只用来定位**宿主函数**，站点靠在该函数体内扫描「`E8` 且目标等于目标函数地址」的调用点确定，**代码里不再有任何字节偏移常量**；壳代码不再假定 actor 在哪个寄存器（用 `rcx` = `InventoryChanges*` 取 `owner`）；`CustomSkinPlayer` 扫不到调用点时改由主线程延迟任务贴件（100 ms 节流），幻化不再因此关闭（详见 §4.51） |
| 1.5.20 起底层幻化乱套：1.5.20 全裸、1.5.21 幻化整体关闭、1.5.22 依旧失败（m01836 / m01837） | 1.5.20 给 `CustomSkinPlayer` 新增「站点必须是 `E8` 且目标等于 `InventoryChanges::ExecuteVisitorOnWorn`」的校验，把 1.5.17 一直可用、目标却不同的调用点判死；钩子拒装后 `DontVanillaSkinPlayer` 仍在藏真甲 ⇒ 全裸，1.5.21/1.5.22 的 fail-closed 与纯扫描继续把幻化关掉。1.5.23 新增 `FindCallInFunctionOrOffset`：先按 1.5.22 扫描，扫不到才回退到 1.0.0 起一直使用的历史站点（SE `+0x81` / AE `+0x1EF`），仅当该处仍是 5 字节 `E8` 才安装；总开关恢复成对判定（详见 §4.52） |

| 1.5.23 装上后仍「只显示头手、其它槽全空模型」，切套装时原装备 46 槽外露（m02035） | 贴件用的是 `&actor->GetBiped(fp)` 强转出来的假 `ActorWeightModel`（该结构在 CommonLibSSE-NG 里只有前向声明），`ApplyArmorAddon` 全部拒绝（日志 `clone 0x0`）；`fullRebuild` 又把已贴好的槽清空重贴，离开 displaySet 的槽还被「还原真甲」贴回真装备。见 §4.53 | 1.5.24 壳代码回到 1.5.17 的寄存器契约（`rbx` = 被蒙皮对象、`rdi` = 引擎的 `ActorWeightModel*`），只贴未真实穿戴的件、从不 `ClearSlots(displayMask)`，离开 displaySet 的槽只清空不还原真甲 |
| 幻化件不跟随 RaceMenu 的 Body Morphs 实时形变（m00002 / m00270 / m02536） | 1.5.20–1.5.25 把「拿到 skee 的 `BodyMorph` 接口」当成前提（`BakeIllusion` 无接口就直接 return，连 `BODYTRI` 都不贴），而用户机器的 skse64.log 证明该接口整局都拿不到：我们的 `0x9E3779B9` 广播只投给 8 个「加载过程中才注册监听者」的插件，skee 是加载期注册的，永远收不到。1.5.26 改为**无条件**给每个幻化克隆件贴 `.tri` 目标（skee 每次 `ApplyBodyMorphs` 都会遍历 biped 槽找 `BODYTRI`，RaceMenu 拖滑块、OBody 换预设都会走那条路），接口只作加分项；握手改到主线程任务里发；几何探测补上蒙皮网格的 `rawVertexData`。详见 §4.55 |
| 1.5.26 装上后照旧不跟随：日志里 `0 .tri target(s) of 5 slot(s)` 出现 5 次，一整局没贴上一个 `BODYTRI`（m02992） | 1.5.26 用 `armor->GetArmorAddon(race)` 取 ARMA 再推导 `.tri`；自定义种族 / 套装没列该种族 / 引擎走默认种族回退时该查询返回空 ⇒ 路径为空 ⇒ 连 `AttachBodyTri()` 都不调（同一份日志的槽位转储里 `addon` 全是非 0，说明引擎自己解析得好好的）。1.5.27 改成优先用 `biped->objects[i].addon`（引擎已解析的那份 ARMA）、候选 `.tri` 名全试、`.nif` 自带的 `BODYTRI` 也计入目标；目标贴上后不再跑幻化循环（1.5.26 那 5 次摘幻化即来自此）。见 §4.56 |
| 1.5.24 装上后幻化件能显示了，但身体（32 槽）永远是空模型；在物品栏取消 32 槽护甲、切到空幻化套装都只剩头手（m02242） | 1.5.24 对「离开幻化集的槽」调用 `ClearSlots` → `RE::BipedAnimAugments::ClearBipedPart`，把 `BipedAnim::objects[bit]` 的 `item` / `addon` / `part` 三个字段清空，连引擎自己的皮肤条目一起拆掉；此后装备栏没有变化，引擎不会再重建，身体槽就永久空着。见 §4.54 | 1.5.25 删除 `ClearSlots` / `SlotClone`：离开幻化集的槽只打日志 `left to the engine`，槽完全交回引擎；贴件后额外打 `item / addon / part / clone` 的 debug 行便于取证 |
| 场景套装切换（OStim/SexLab）必须弹一次快速选择窗口才显示 `_sex` 套装，场景结束回普通幻化也要按键（m07382） | 交接机制本身没坏：1.5.31 日志证明场景开始/结束时 biped 条目与克隆几何都在（`attaching 5 outfit-only piece(s)`、`slot NN part 1`），真正缺的是「被绘制的 3D」。1.5.30 的补跑 pass 与 1.5.31 的 `Update3DModel()` 都试过、都无效，用户要求把这套机件删掉。见 §4.60 | 1.5.32 删除 `RequestOutfitPasses` / `DrainOutfitPasses` / `kOutfitPassCount` / `rebuildActorModel`，场景套装只保留 `Features::AssignOutfit()`；场景事件改为在游戏线程同步切换（与按地点/战斗切换同构），并把名字查找放宽到「全部套装名 + 忽略大小写」 |
| 「你只用找对名字然后切换，参考按地点/战斗切换幻化套装的逻辑」（m07424） | 按地点/战斗切换走的是 `AutoSwitch::Evaluate()` → `setOutfitUsingState()` → `refreshArmorFor()`，场景套装此前用的是自己拼的 `set_outfit_c()` + 补跑随访，两套路径行为不同。见 §4.60 | 1.5.32 让场景套装与菜单点击共用同一条 `Features::AssignOutfit()`，并去掉全部 `_sex` 专属的槽位/时机特判（`_sex` 套装里的规则全部由用户自己配置） |

| 「依旧和之前一样的问题……我说的是 Sexlab 和 OStim 场景的专用换装 `_sex`，你不会改的是按地点/状态自动切换吧？不行直接回退到 1.5.29，从这开始改……参照按地点/状态自动切换这里的换装方法去换装」（m07666） | 先核对了改动面：`AutoSwitch.cpp` / `AutoSwitch.h` 自 1.5.29 起**一行未改**（`git diff v1.5.29 HEAD` 为空），改的确实是 SexLab/OStim 那个模块 `SceneOutfits.cpp`。但场景入口在 1.5.32 走的是菜单那条 `Features::AssignOutfit()`，且仍带着自有的 SKSE 任务队列 + `Tick()` 心跳，与状态切换的两步序列不同。见 §4.61 | 1.5.33 回退源码到 v1.5.29，删掉场景专用的队列与 `Tick()`，新增 `OutfitSystem::setOutfitByName()`，让场景入口与状态切换共用「锁内指派 + 锁外 `refreshArmorFor`」这两步 |
| 「这次 OStim 场景开始后幻化压根没切换」（m07842，1.5.33 实机） | 日志 `v1-5-33-0` 里 `SceneOutfits` 只有 4 行，三条是 `'ostim_start' / 'ostim_end' arrived off the game thread; the scene switch is skipped.`：1.5.33 在 `ModEventSink` 里用 `OnGameThread()` 门禁把工作线程送来的事件整条丢弃，装换装逻辑根本没执行（线程 39516 / 10604 / 27040，主线程是 34628）。见 §4.62 | 1.5.34 离线程事件改用一个 `SKSE::GetTaskInterface()->AddTask(...)` 交接到游戏线程执行 `Dispatch()`，游戏线程上仍走内联快路径；不建队列、不加 `Tick()`，换装调用序列保持 `setOutfitByName()` + `refreshArmorFor()` |

| 「幻化栏选中一套装时勾选逐槽策略编辑器，会把所有套装的该选项一起打开」（m09191②） | 编辑器展开状态原先是一个全局布尔，任何一次勾选都会让全部套装的策略面板一起展开。见 §4.63 | 1.5.39 把展开状态改成按套装名保存的 `g_slotPolicyOpen`（`std::unordered_set<std::string>`，key = 套装名），只展开被选中的那套；JSON 仍是每套自己的 `slotPolicy` / `slotPolicies`，导入导出格式不变 |
| 「按 ESP 自动生成套装……扫描服装时必须是正常服装，FX 特效，没有 EditorID，各种不算在内。不过金件就不一样了，可以放开按金件的规则来」（m09191③④、m09426） | 生成套装与金件都要按加载顺序列出 ESP 并扫描护甲，但两者对「能不能收」的要求相反：套装只该收能穿的东西，金件是用户自愿指定的任何件。见 §4.63 | 1.5.39 新增 `PluginScan::IsOutfitArmor()`（有 EditorID + 至少一个非 FX 槽位 + 至少一个 armor addon 有模型）用于套装生成，并把 FX 位从分桶剔除；金件路径不调用它，保持全量扫描 |
## 4. 本版变更

### 4.1 移除感知层（Perception）

1.0.2 曾重绑 Papyrus 的 `Actor.GetWornForm` / `Actor.WornHasKeyword`。实机日志（1.0.2 与 1.0.3 两次）里**一次都没有出现** `Perception: GetWornForm(...)` 行，说明触发判定的 mod 走的是引擎条件而非 Papyrus 入口；而引擎条件读真实装备的 `ExtraWorn` 标记（TESCondition 函数表不在 Address Library），无法安全改写。既然覆盖不到真实的判定路径，本版按用户要求**整体删除**感知功能：

- 删除 `src/cpp/Perception.cpp`、`src/cpp/include/Perception.h`；
- Papyrus 端删除 `IsPerceptionEnabled` / `SetPerceptionEnabled` / `GetPerceivedArmor`，注册名 69 → **66**；
- ini 删除 `[Perception]` 段与 `[Debug] InstallPerception`。

替代做法是对用户实际在用的两个 mod 做**数据级**兼容（§4.2、§4.3）。

### 4.2 Procedural Armor Damage 兼容（零补丁）

PAD 的可撕件判定链（逆向 PAD 1.0.4 得到，见 `docs/HOOKS.md` §7）：

1. `CollectWorn`：从**玩家背包**取 `InventoryEntryData::IsWorn` 的护甲（排除 skin），加上 `ExtraDataList` 的破损副本；
2. `ScanScene`：遍历 `BipedAnim::objects[32]`，只保留 `BIPOBJECT.item` 落在第 1 步集合里的槽，再用 `partClone` 的模型名 + addon formID 组成 MeshKey，收集三角面；
3. `Capture/TearArticles`：按 MeshKey 记录损伤、删三角形。

**1.4.0 之前这里有个真实缺陷**：`Custom()` 直接调 `ApplyArmorAddon(幻化件, …)`，引擎会把该槽的 `BIPOBJECT.item` 换成**幻化件本身**，于是第 1 步的集合里没有它、第 2 步就整槽跳过 —— 幻化外观永远撕不到。旧文档写着“会如实写回”，但代码里并没有这一步（1.0.0–1.3.0 都没有），这是 PAD 失效的根因。

1.4.0 起 `Custom()` 在每次贴图后做**诚实还原**（`Hooking::CustomSkinPlayer::RestoreRealItems`）：贴之前先按已穿装备建 `wornBySlot[32]`，贴完只把“被自己改写成幻化件”的槽 `item` 换回真实护甲，`addon/part/partClone` 保持幻化件 → 外观不变，但 PAD 的第 1 步照常成立、第 2 步门禁照常通过，撕坏的是幻化外观下那件真实护甲的模型（损伤记在真实护甲上，与“数值仍按真实护甲”一致）。开关：`[Compatibility] ProceduralArmorDamage`（默认 on）。

> 关于用户问的“背包里至少一个副本带 ExtraWorn（真穿着）”：那是 PAD **Papyrus 脚本 `TearArmor`** 的三道门禁之一，只作用于脚本调用路径；PAD 自己的损伤是原生 `ScanScene`/`TearArticles` 走的，不查这一条。所以需要修的不是绕过门禁，而是让 `item` 别被替换。

不需要、也没有对本插件或 PAD 打任何补丁。唯一缺口：**幻化件盖在完全没有真实护甲的槽位时**，该槽 `item == nullptr`，PAD 根本看不到它；`[Debug] ExtraLogging` 会在每次换装后列出这些件：

```
PAD compatibility: 囚犯 shows 'Iron Helmet' over slot mask 0x1 without a real armour underneath; Procedural Armor Damage cannot damage that piece.
```

### 4.3 Advanced Nudity Detection 兼容（写阵营，不碰代码）

AND 4.2.0 的判定结果通过阵营对外发布（逆向结论）：

- `ActorWornHasKeyword` / `PlayerWornHasKeyword` / `GetPlayerWornObjectList` 扫真实装备的护甲关键词，`Finalize` 写 `AND_NudeActorFaction` / `AND_ToplessFaction` / `AND_BottomlessFaction` 等阵营；
- `GetNude`（RVA 0x3D600）内部就是 `Actor::GetFactionRank(AND 阵营, isPlayer) > 0`；
- 因此**阵营等级就是 AND 对外的公开接口**：AND 自己的 Papyrus getter、对话条件、依赖它的其它 mod（OSR/OSL Aroused 一类）全部读这一份数据。

所以本版按幻化后的覆盖情况写这三个阵营（1 = 裸体 / 上身空 / 下身空，0 = 有遮蔽）：

| 判定 | 视为有遮蔽的槽位（bits） |
| --- | --- |
| topless | `Body(2)`、`ModChestPrimary(16)`、`ModChestSecondary(26)` |
| bottomless | `Body(2)`、`ModPelvisPrimary(19)`、`ModPelvisSecondary(22)`、`ModLegRight(23)`、`ModLegLeft(24)` |
| nude | `Body(2)` |

- 阵营按 **EditorID** 解析（`AND_NudeActorFaction` 等），不是按 FormID 或地址 → AND 升级只要不改 EditorID 就仍然有效；解析不到时只写一条 warn，判定退回原版行为。
- **不使用 hook / 入口补丁 / thunk**：不读 AND 的内部函数，也不改它的代码段，只调用引擎的 `Actor::AddToFaction`（等同于 Papyrus `SetFactionRank`）。
- 防漂移：AND 自己会在装备事件与游戏时间 tick 重扫真实装备并覆盖等级，所以 ① 每次幻化刷新后立即同步；② 另有输入事件心跳（节流 1 秒，切回主线程）补写一次。
- 近似性：只看槽位掩码，不按 AND 的关键词（curtain / top / bottom / bra / underwear）精判，混搭护甲可能被判成“有穿”。

### 4.4 兼容层结构

```
src/cpp/compat/
  DisplaySet.{h,cpp}   按当前幻化算出显示集（替代原 Perception 的计算部分）
  ModuleInfo.{h,cpp}   模块路径与文件版本（GetFileVersionInfo）
  PadCompat.{h,cpp}    PAD：检测、状态、诊断日志（不改 PAD 任何东西）
  AndCompat.{h,cpp}    AND：阵营解析、覆盖判定、写阵营等级
  ModCompat.{h,cpp}    统一入口 Install()/Reload()/Tick()/OnOutfitApplied()/GetStatus()
```

菜单 Settings 页新增 “Mod compatibility” 段，逐项显示 `not installed` / `detected (nudity factions follow the outfit)` / `detected, its nudity factions were not found` 等状态；ini `[Compatibility]` 段两个开关默认开启。

### 4.5 PAD 槽位诊断（1.2.0 新增）

实机反馈“挨打时幻化衣服没有变化、PAD 的 Tracked armors 列表是空的”。两条独立事实：

1. **Tracked armors 只列“已经产生过损伤记录”的护甲副本**（PAD 字符串：`No tracked damage in this save.`、`Each entry is one armor copy...`）。列表为空只说明还没撕过，不代表 PAD 没看到装备。
2. **PAD 只能撕“背包里真实穿着且有 ExtraWorn 标记”的护甲**。它的 Papyrus API `ArmorTear.TearArmor` 反汇编后同样有这条门禁（找不到带 ExtraWorn 的副本就返回 -1），所以**不能用 PAD 的 API 让幻化件被撕**——幻化件不在背包里。

因此本版不加补丁，而是让用户能自证：

- Settings → Mod compatibility → **Log slot report** 按钮 → `Compat::Pad::Diagnose(player)`：先逐条列出真实穿着的护甲，再逐槽输出 `PAD slot report: slot  N (0xM) shown 'X' | real 'Y'`；`real '<none>'` 即 PAD 摸不到的槽位。
- 每次换装刷新后，若幻化覆盖了没有真实护甲打底的槽位，按 actor 记录掩码并**只在变化时**打一条 INFO：`PAD compatibility: <actor> shows pieces over slot mask 0x... with no real armour underneath...`。
- 有真实护甲打底的槽位行为不变：PAD 撕的是画出来的那层 mesh，损伤记在真实护甲上。

### 4.6 菜单中英双语（1.2.0 新增）

- 新模块 `src/cpp/I18n.{h,cpp}`：`Lang{Auto,English,Chinese}`、`Tr`（文本）/ `TrId`（控件标签，返回 `中文###English` 让 ImGui 用英文做 id）、`ChineseAvailable`、`Status`。
- 翻译挂在 `src/cpp/include/ImGuiApi.h` 的 `IG::` 包装层，菜单代码保持英文字面量；词典在 `I18n.cpp` 的 `kDictionary`（约 85 条，含 13 个状态名）。
- 字形由菜单框架决定：读 `Data/SKSE/Plugins/SKSEMenuFramework.ini` 的 `[Fonts] EnableChinese` / `PrimaryFont`（默认 `CN.ttf`），并检查 `Data/SKSE/Plugins/Fonts/<PrimaryFont>` 是否存在。缺字体时保持英文并写一条 warn（`Language: Chinese was requested but ... no CJK font loaded`），不画方块、不注入字体、不 hook。
- 配置：`[UI] Language = auto | en | zh`，菜单里改动即时 `Settings::Save()` + `I18n::Refresh()`。

### 4.7 原版 MCM 功能补齐（1.3.0 新增）

原版 `SkyOutSysMCM.psc`（1,099 行、22 个控件家族）在 1.2.0 的 SMF 菜单里只覆盖了套装列表与角色列表。本版逐控件补齐，并把对照关系写成 `docs/MCM_PARITY.md`：

- **套装编辑器**（原版 `ShowOutfitEditor` 整块）：新模块 `src/cpp/MenuOutfitEditor.cpp`，从背包 / 当前穿戴 / 十六进制 FormID / 全护甲表搜索添加护甲（跳过模板护甲与不可玩条目，结果上限 400 条）；槽位冲突时内联确认「替换冲突件 / 取消」（`Outfit::conflicts_with` + 按槽掩码交集移除）；槽位清单按槽掩码展开并带 `Remove` 确认；Base policy + 32 槽逐槽策略 + `Reset all policies to the defaults`。
- **Apply to 目标选择**（原版 `OPT_SelectActorSelection`）：Outfits 页工具条下拉，列表 Wear/双击、编辑器 Wear/Take off 都作用于所选角色；当前穿戴该套装的角色名带 `* ` 前缀（原版 ActiveMark）。
- **Add a nearby actor...**（原版 `OPT_AddActorSelection`）：同 cell、未死亡、FormID 允许的 NPC。
- **快捷槽法术开关**（原版 `OPT_QuickslotsEnabled`）：原版通过 `SkyOutSysQuickslotManager` quest 脚本加/去法术，本版改成原生 `Features::ApplyQuickslotSpell()`（EditorID → `SkyrimOutfitSystem.esp:0x000182F` 回退），在 `kDataLoaded` / `kNewGame` / `kPostLoadGame` 同步，状态写入 `[General] QuickslotsEnabled`。
- **删除/移除确认**：套装删除与角色移除改为内联确认条（SMF 无弹窗 API）；新建/改名按 `GetOutfitNameMaxLength()` 在界面层校验并常显上限。
- **中文词条**：I18n 词典 85 → 135 条，覆盖编辑器、Apply to、确认条与版本行。

已知差异（有意为之）：没有右键上下文菜单、没有 MCM 的 InfoText 面板、没有逐项 Restore defaults、快捷槽改为原生窗口、Export 只含套装与角色分配。

`SkyOutSysMCM.psc` 保持空壳，但补回了原版对外暴露的 `GetModVersion()` / `GetVersion()` / `GetQuickslotManager()` 三个接口，第三方脚本的旧调用不会失效（pex 703 → 991 字节）。

### 4.8 外置可编辑翻译文件（1.4.0 新增，文件布局已被 1.4.1 的 §4.15 取代）

界面文本不再只存在 DLL 里。1.4.0 时读 `Data/SKSE/Plugins/SkyrimOutfitSystemNG_i18n.txt`（单文件 `[en]`/`[zh]` 两段）；**1.4.1 起改为 `Data/interface/translations` 下每语言一份文件，见 §4.15**，旧文件不再被读取。下面保留 1.4.0 的格式说明只作历史参考：

```ini
# 说明行
[en]
Wear=Wear
[zh]
Wear=穿戴
```

- 键是**英文原文**（与 `I18n::Tr` 的入参逐字符一致），值是译文；只写想改的行即可，缺的行回退内置词典（中文）或原样（英文）。
- 文件不存在时自动写出全量模板（153 条 × 2 段），**已存在的文件永不覆盖**。
- 支持 `\n`、`\t`、`\\` 转义；`#` 与 `;` 起头是注释；同一段里重复键后者生效。
- Settings → Language → **Reload the language file** 立即重读，无需重启；左侧树节点标题由菜单框架缓存，**要重启游戏**才更新（界面有提示）。
- 加第三种语言：把 `[en]` 段整段翻成目标语言，`[UI] Language = en`。
- 覆盖范围：菜单文案、左侧树节点、HUD 提示；不覆盖日志、Papyrus 函数名、ini 键名。细则见 `docs/I18N.md`。

### 4.9 快速选择窗口与列表可操作性（1.4.0 新增）

- **窗口不再是隐式 Debug 窗**：SMF 的 `Renderer::RenderWindows()` 只调 `window->Render()`，自己不做 `Begin/End`。此前 `RenderQuickPicker` 没有 `Begin/End`，实际画在 ImGui 的默认调试窗上（所以“尺寸太小、不居中”）。现在用它导出的 `igBegin/igEnd` 自绘窗口，`SetNextWindowSize(640×800, FirstUseEver)` + `SetNextWindowPos(视口中心, FirstUseEver, pivot 0.5)`，位置/尺寸由用户拖动后记忆。
- **键盘与手柄**：SMF 打开任一窗口时会把游戏输入置空并 `io.AddKeyEvent` 喂给 ImGui，所以这里统一查 ImGui 键：`W`/`↑`/`GamepadDpadUp` 上移，`S`/`↓`/`DpadDown` 下移，`A`/`←`/`DpadLeft` 上一页，`D`/`→`/`DpadRight` 下一页，`E`/`Enter`/`GamepadFaceDown` 穿上，`R`/`GamepadFaceUp` 随机，`Esc`/`GamepadFaceRight` 关闭。自写重复器：首次立即触发，按住 0.4 s 后连续、每 0.045 s 一次（越按越快）。
- **焦点记忆**：焦点是全局下标。打开窗口时定位到玩家当前套装；随机后定位到抽中的那套并滚到可见；翻页按整页移动并夹紧。搜索框有焦点时（`IsAnyItemActive`）只有 `Esc` 生效，避免打字触发换装。
- **取消幻化**：底部 `Remove outfit` = `set_outfit_c("", playerId)` + 刷新，HUD 回 `Outfit removed.`，角色露出真实护甲。
- **每页数量与列表宽度**：`[UI] OutfitPageSize`（默认 50）、`QuickPickerPageSize`（默认 20）、`OutfitListWidth`（默认 470），两个面板的滑条直接读改并 `Settings::Save()`。
- 依赖 14 个可选 `ig*` 导出（窗口/按键/尺寸/滚动）。SMF 版本太旧缺其中任何一个时，`ImGuiApi` 只记一条 warn，菜单其余部分照常工作，快速选择窗退化为普通窗口（无键盘导航）。

### 4.10 PAD 诚实还原的实现与开关（1.4.0 修复）

`src/cpp/hooking/Patches.cpp`：

1. `Custom()` 在贴图前用已穿装备建 `std::array<const RE::TESForm*, 32> wornBySlot{}`（按每件护甲槽掩码填位）；
2. 逐件 `ApplyArmorAddon(armor, race, actorWeightModel, isFemale)`；
3. 立即 `RestoreRealItems(actor, armor, wornBySlot)`：第一/第三人称两份 `BipedAnim` 都遍历，`if (slot.item == armor && wornBySlot[bit]) slot.item = wornBySlot[bit];` —— 只撤销自己写进去的 item，真实穿着的槽一个字节都不动。

开关 `[Compatibility] ProceduralArmorDamage = on|off`（默认 on）。设 off 可做对照：同样的操作幻化外观不再可撕，即证明这条路就是修复点。仍然撕不到的只有“完全没有真实护甲打底”的槽（裸槽贴裙子），日志会给出槽掩码。

### 4.11 前情（1.0.1–1.0.3 已修，此处仅备查）

- 载入存档无限载入：`OutfitServiceReadGuard/WriteGuard` 只有构造加锁、析构漏了解锁（照抄 Rust Drop 语义），递归互斥锁被主线程永久持有 → 渲染线程在幻化钩子里阻塞。
- 两套平行钩子实现合并为单套 REL::RelocationID 表；旧 co-save（protobuf v4/v5/v6）在读档时迁移为 JSON v7。
- 缺 `SkyrimOutfitSystemNativeFuncs.pex` 导致 63 个原生函数全部不绑定（日志特征：`Cannot open store for class` + `could find no matching static function`）。
- 法术 “Change Outfit” 的随机行为改由 `IsQuickSlotRandom()` 读 ini（ESP 的 VMAD 里 `RandomOnCast=True` 改不动 psc 默认值）。

### 4.12 快速选择窗口位置/尺寸固化（1.4.1）

- 原实现只有 `ImGuiCond_FirstUseEver` + 居中，而 SKSE Menu Framework 不保存 ImGui 窗口状态，进程结束即忘；插件侧也没有存过任何坐标。
- `Settings` 新增 `[UI] QuickPickerX / QuickPickerY / QuickPickerWidth / QuickPickerHeight`（默认 `-1 / -1 / 640 / 800`，`-1` 表示居中）。
- 每帧在 `BeginWindow`/`EndWindow` 之间用 `igGetWindowPos` / `igGetWindowSize` 读实际矩形；变化后置脏，**0.75 秒无变化**才写 ini（拖拽时不会每帧写盘），窗口关闭时立刻写盘。
- 两个符号是**可选绑定**：菜单框架没导出它们时只退化成“每次居中”，其余功能照常（日志有一行 warn）。

### 4.13 v4 老存档槽位策略错认（1.4.1）

- 现象：从老存档迁移到 v7 后，原本“需要已装备（槽位有装备时才显示套装）”= `XXXO` 变成了“始终使用套装”= `XXOO`。
- 根因：v4 的策略是数字字段（`Outfit.OBSOLETE_slot_policy = 7`、`OBSOLETE_slot_policies = 6`），v5 起才是字符串（9/10）；`LegacySave.cpp` 只解析字符串版，并把套用整段放在 `if (a_version >= 5)` 之下。结果 v4 一个策略都没套上，留下构造函数默认的 `blanket = XXOO` + 盾牌槽 `XEXO`。
- 修法：补 `case 6` / `case 7`（`map<uint32,uint32>`、`uint32`，`repeated uint32` 的 packed 与单值都读）；套用前先把 32 槽清成“继承”、blanket 清成 `XXXX`（= 原版 `from_proto_data` 的初始态），再按原版顺序套字符串策略 → 数字策略；不再看版本号。
- `Policy::FromInt` 映射逐条对齐原版 `Policy::from_int`，`>=128` 视为 `CustomPolicy%03d`。
- 验证点：日志 `Loaded N outfits ... from a legacy co-save`；进游戏看套装编辑器里 policy 文案是否与老存档一致。已迁移过一次且被写成 `XXOO` 的存档，重新导入旧 co-save 即可修正。

### 4.14 原版 `OutfitSystemData.json` 导入导出（1.4.1）

- 路径与原版相同：`Data\SKSE\Plugins\OutfitSystemData.json`。
- 结构按 protobuf JSON 映射（`protobuf_json_mapping::print_to_string` 默认选项）：lowerCamelCase、默认值字段省略、map 键是字符串、uint 是数字。示例：

```json
{
  "enabled": true,
  "outfits": [
    {
      "name": "Mage",
      "isFavorite": true,
      "armors": [ { "rawFormId": 123456, "localFormId": 1234, "modName": "Skyrim.esm" } ],
      "slotPolicy": "XXXO",
      "slotPolicies": { "9": "XEXO" }
    }
  ],
  "actorOutfitAssignments": {
    "20": { "currentOutfitName": "Mage", "stateBasedAutoSwitchEnabled": true, "stateBasedOutfits": { "12": "Mage" } }
  }
}
```

- 导入端也接受 snake_case 原名、`OBSOLETE_*` 数字策略、以及 `armors` 里直接写 FormID 数字的简化写法。
- 入口：Papyrus `ExportSettings` / `ImportSettings`（HUD 提示走翻译）；SMF 菜单 Settings → **Outfit data** 的两个按钮。导入成功会自动 `CreateRefreshAllArmorsTask()` 刷新全部外观。
- 与 co-save 的关系：co-save 仍是**权威存档**；JSON 只是可读写的交换格式（换存档、备份、批量编辑用）。

### 4.15 界面文案翻译补全（1.4.1）

- 词典 153 → **224** 条。典型缺口：12 条策略文案（`Require equipped (show the outfit only when the slot is worn)` / 需要已装备…、`Always use outfit` / 始终使用套装 …）、32 条槽位名、导入导出与错误提示、`[Unnamed]` 等。
- 拼接式 HUD 提示（`"Outfit: " + name`、`"Slot " + label + " uses " + policy`）改为 `I18n::T(en, zh)` 前缀，中文下整句中文。
- 新增语言 = 在 `Data\interface\translations` 放一个新文件，无需改插件。

### 4.16 快速选择窗口：自己开窗、几何真正生效（1.4.2）

- **根因**：`IG::WindowIsOpen()` 用 `igGetCurrentWindow() != nullptr` 判断“框架已开窗”。ImGui 每帧 `NewFrame()` 都会隐式 `Begin("Debug##Default")` 并把它设为当前窗口，因此这个判断**恒为真** → `standalone=false` → 既不 `SetNextWindowPos/Size` 也不 `Begin`，内容全部画进隐式 Debug 窗口（用户看到的标题 “Debug”），`[UI] QuickPickerX/Y/Width/Height` 一次都没被应用。
- **修法**：`MenuUI.cpp` 用 `g_renderingSection` 标志区分两种入口 —— F1 菜单里的“快速选择”页（`RenderQuickPickerSectionThunk` 置位）复用菜单窗口；`AddWindow` 注册的独立窗口（`RenderQuickPickerThunk`）永远自绘 `Begin/End`。窗口标题带 `###SOSQuickPicker`，换语言不丢窗口状态。
- **强制几何**：每次 `ShowQuickPicker(true)` 置 `g_quickGeometryForce`，下一帧用 `ImGuiCond_Always` 套用 ini 里的矩形（`FirstUseEver` 在同一 ImGui 上下文里只生效一次）。拖动/缩放后 0.75 s 节流写回，关闭时立即写。日志 `Quick picker: opening at x,y with wxh (restored from the ini|centred).`
- **可选符号不再一票否决**：`ImGuiApi` 改成逐符号 `GetProcAddress`，失败只 warn 并打印试过的名字；新增 `WindowPlumbingReady()` 只要求 `Begin/End/SetNextWindowPos/SetNextWindowSize`。`igIsKeyDown_Nil` / `igIsKeyPressed_Bool` / `igSetScrollHereY_Float` 都带备选名。日志 `ImGuiApi: N/M optional helpers resolved in ..., window geometry tracked|not tracked.`
- **长按**：`kRepeatDelay 0.40 → 0.22`、`kRepeatMinInterval 0.045 → 0.030`、`kRepeatAcceleration 0.045 → 0.030`。

### 4.17 OutfitSystemData.json 策略规范化（1.4.2）

- **根因**：策略字段被当作四字符码存取，而文件里可能写的是本地化标签（`需要已装备（槽位有装备时才显示套装）`、`始终使用套装`）。`FindPolicy()` 精确匹配失败 → `compute_display_set_c()` 直接 `continue` → 该槽位“什么都不显示”（用户看到的“从不显示任何东西”）。
- **修法**：`Policy::NormalizePolicyCode()` 接受四字符码、`$SkyOutSys_Desc_EasyPolicyName_XXXX`、英文标签、新旧中文标签（“套装/幻化”两套）、`CustomPolicyNNN`、纯数字（走 `PolicyFromInt`）。导出统一写四字符码（**1.4.3 更正：已改回数字枚举，见 §4.20**）；导入先规范化再落库；认不出只 warn 并回落基础策略。
- **兜底**：`compute_display_set_c()` 未知策略 → 基础策略 → `XXOO`；`slotPolicies` 的值同时接受字符串与数字。

### 4.18 槽位编号与“幻化”措辞（1.4.2）

- 策略编辑器所有槽位名追加编号：中文 `头部（槽位 0）`、英文 `Head (slot 0)`，与 `BIPED_OBJECT` 一一对应。
- 中文 7 条策略文案由“套装”改成“幻化”；旧措辞仍作为导入别名保留，1.4.1 写的 JSON 与语言文件不会失效。

### 4.19 槽位编号改为游戏里的 30–61（1.4.3）

- 内部 BOD2 索引是 0 基：`kHead = 0` … `kFX01 = 31`（`include/RE/B/BipedObjects.h`）。Creation Kit、xEdit 和游戏里给玩家看的编号是同一个槽位 **+30**（头 30、头发 31、身体 32 … 盾牌 39 … FX 61）。
- 1.4.2 直接把内部索引当显示号追加，所以全文都错了 30。1.4.3 在 `src/cpp/MenuOutfitEditor.cpp` 里加 `kDisplaySlotOffset = 30`，`SlotLabelFor()` 输出 `a_index + 30`，显示成 `头部（槽位 30）`、`盾牌（槽位 39）`、`FX（槽位 61）`。
- 同时按用户给的对照表重写 `kSlotLabels[32]` 的中文名：`头发, 短` / `头发, 长` / `项链`（原“护符”）/ `未知 (一般来说是 面部)` … `未知 (一般来说是 其他 2)` / `Decapitate-Head` / `Decapitate` / `FX`。英文列同步（`Unnamed (probably Face)` 等），`src/cpp/I18n.cpp` 词典跟着改（仍 224 条），两份语言文件重新生成。

### 4.20 OutfitSystemData.json 全面对齐原版：策略用数字枚举（1.4.3）

- **文件真实结构**（`兼容参考/OutfitSystemData.json`，530 套、2 个 actor）：`{ "enabled": true, "outfits": [ { "name", "isFavorite", "armors": [ { "rawFormId", "localFormId", "modName" } ], "slotPolicies": { "9": 8 }, "slotPolicy": 2 } ], "actorOutfitAssignments": { "<十进制 FormID>": { "currentOutfitName", "stateBasedAutoSwitchEnabled", "stateBasedOutfits": { "0": "名称" } } } }`。策略是 **protobuf 枚举数字**：2 = `XXXO`（需要已装备）、5 = `XXOO`（始终使用幻化）、8 = `XEXO`（穿透，盾牌默认）。
- **旧实现的错**：1.4.1/1.4.2 用 `FetchString()` 读 `slotPolicy`，遇到数字返回空串 → 基础策略保持默认 `XXXX`（从不显示任何东西）；只有 `slotPolicies` 里零星几个数字槽位是对的。导出则写四字符码字符串，与原版文件格式不一致。
- **现在的行为**：`src/core/OutfitService.cpp` 新增 `FetchPolicyCode()`：数字 → `PolicyFromInt()`（0–11，≥128 → `CustomPolicyNNN`）；字符串 → `NormalizePolicyCode()`（四字符码 / `$SkyOutSys_Desc_EasyPolicyName_*` / 中英文标签，含旧“套装”措辞）。导出用新增的 `PolicyToInt()` 写数字 id，字段顺序、省略默认值的规则都与原版 `protobuf_json_mapping` 一致（`enabled`、`isFavorite`、空 `armors`、`slotPolicy = 0` 都会省略）。
- 导入顺序复刻原版 `from_proto_data()`：先重置 32 槽为 nullopt、基础策略 `XXXX`，再套 `slotPolicies` 数字图，再套 `slotPolicy`，最后才是 `OBSOLETE_*` 兼容字段。

### 4.21 Outfits 删除按钮（1.4.4）

- 根因：`MenuUI.cpp` 的 `Delete` 只执行 `g_pendingDelete = name;`，全文件没有任何地方读取它，所以既不删也不打日志。
- 现在：列表上方出现确认条 `Delete outfit "X"? This also removes it from every actor and state assignment.`；`Delete` 调 `OutfitService::delete_outfit()`（会清掉所有 `current` 与 `stateBased` 引用），成功写 `Outfits: deleted 'X': yes.`，并清空选中/过滤缓存、`CreateRefreshAllArmorsTask()` 让在场角色立刻换回真实护甲；`Cancel` 只关确认条。

### 4.22 套装编辑器横向滚动（1.4.4）

- `##outfiteditor`、`##slotlist`、`##slotpolicies` 三个子窗口都带 `ImGuiWindowFlags_HorizontalScrollbar`。
- 槽位行不再用固定 `SameLine(320)`：行宽 = `CalcTextSize(行文本) + 16`，至少 320，长护甲名会把列宽撑开、按钮跟着右移，可横向拖动查看。
- 基础策略与 32 个逐槽策略的 combo 统一 `SetNextItemWidth(520)`，译文再长也不会顶到右侧。

### 4.23 日志分级（1.4.4）

- 65 条诊断日志（Settings 读取、MenuBridge/ImGuiApi 绑定细节、每个钩子的地址/字节、Hotkeys 注册与触发、兼容层检测、SetStatus/NotifyChanged 等）从 `info` 降为 `debug`。
- 保留在 `info` 的只有：`Skinning hooks applied.`、`Papyrus functions registered.`、`Game event sinks registered.`、`Saved N outfits...`、`Migrating a vN record...`、`vN is loading...`、`Plugin loaded, waiting...`、`Loaded/Imported N outfits...`、`Menu: sections registered...`、`Language: wrote the editable translation files...`、`AutoSwitch: native location...`、`Outfits: deleted...`、`Menu: export/import...`、`PAD slot report...`（用户主动点按钮生成）。
- `[Debug] ExtraLogging=true`（菜单 Settings → Verbose logging）时 spdlog 级别为 `trace`，全部 `debug` 重新可见。

### 4.24 地点/状态自动切换改为原生（1.4.4）

- 1.4.3 及更早：战斗由 `CombatStartSubscription`（C++，`TESCombatEvent`/`TESDeathEvent`）处理；地点靠 `SkyOutSysAutoSwitchTrigger.psc` 的 `OnLocationChange` → `Utility.Wait(10.0)` → `SetOutfitUsingStateForAllConfiguredActors()` + `RefreshArmorForAllConfiguredActors()`，最坏晚 10 秒。
- 1.4.4 新增 `src/cpp/AutoSwitch.{h,cpp}`：`LocationSink`（`TESActorLocationChangeEvent`）与 `FastTravelSink`（`TESFastTravelEndEvent`）注册到 `ScriptEventSourceHolder`；只处理 `should_override()` 且开了状态自动换装的 actor；`NGHotkeys` 的输入 sink 每次事件调 `AutoSwitch::Tick()`（2 秒节流）兜底天气变化/漏事件。
- 去抖：`AutoSwitch` 的 `FormID→state` 缓存 + `ActorAssignments::lastState` 两层，状态没变直接返回；只有变化才 `setOutfitUsingState()` + `refreshArmorFor()`。
- `skyoutsysautoswitchtrigger.psc` 改为空实现（保留 `Scriptname SkyOutSysAutoSwitchTrigger extends ReferenceAlias`），旧存档/ESP 仍能解析；打包的 pex 也换成空实现版本。

### 4.25 状态套装 `<随机>`（1.4.4）

- 数据层：`OutfitService::kRandomStateOutfit = "<random>"` 作为哨兵，允许写进 `stateBasedOutfits`（序列化、co-save、JSON 导入导出都放行）。
- 行为：`set_state_outfit(state, "<random>")` 立即随机抽一套并写 `current`；之后 `set_outfit_using_state` 在 `lastState` 与当前状态相同时不重抽（战斗事件反复触发保持同一套），状态真正变化时才重抽。
- 随机规则：尊重 `RandomOutfit.FavoritesOnly`，默认排除当前正在穿的那套（`_outfits.size() > 1` 时）；收藏过滤后为空则回退全部套装。
- UI：13 个状态的下拉在 `<inherit>` 之后多一项 `<random>`（中文 `<随机>`）；combo 预览直接显示译文。

### 4.26 Outfits 列表按钮被整行 Selectable 抢走点击（1.5.0）

- **现象**：左列表里「Wear（穿上）」几乎点不中；「Delete」也只能偶尔在边缘点中；点击大部分区域只会选中/取消选中条目。删除逻辑本身正常（点中时确实会删）。
- **根因**：行宽 `rowWidth = max(CalcTextSize(label).x + 150, listWidth - 24)`，`Selectable` 使用整个 `rowWidth`；Wear / 星标 / Delete 用 `SameLine(rowWidth - 142 + 8)` 叠在同一个矩形上。ImGui 的 `Selectable` 是整块可点击区域，先吃掉鼠标事件，按钮只剩极小边缘能命中。
- **修法**（`src/cpp/MenuUI.cpp`）：
  ```cpp
  constexpr float kButtonReserve = 150.0f;
  const float textWidth = IG::CalcTextSize(label.c_str()).x + 8.0f;
  const float buttonX = std::max(textWidth + 8.0f, std::max(listWidth - kButtonReserve, 0.0f));
  const float nameWidth = std::max(buttonX - 8.0f, textWidth);
  IG::Selectable(label.c_str(), isSelected, 0, ImVec2(nameWidth, 0.0f));
  IG::SameLine(buttonX);
  ```
- **结果**：名字区与按钮区完全分离；短名字时按钮固定在列表右侧，长名字时按钮跟在文字后面并撑出横向滚动；双击选中、tooltip、收藏/删除确认流程都不变。

### 4.27 快速选择窗口用手柄时出现两个光标（1.5.0）

- **现象**：手柄十字键移动列表时，另一个高亮同时出现在 `Remove outfit` 等控件上；按 A 会先换装、再执行那个按钮（例如取消幻化）。
- **根因**：窗口自己读 `GamepadDpad*` 移动 `g_quickFocus`，但独立窗口只设了 `NoCollapse`，ImGui 默认导航仍在同一窗口里移动另一个控件焦点。
- **修法**：`RenderQuickPicker` 的 flags 改为 `ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus`。手动 W/S/A/D、方向键、十字键、E/R/Esc 全部保留；SMF 的全局快捷键不受影响。
- **结果**：窗口内只有一个焦点（列表高亮），A 只执行换装，B/Esc 只关闭。

### 4.28 切换幻化时按角色重置 SMP（1.5.0）

- 新配置：`[Compatibility] AutoResetSMP = off`（默认关；设置页 Mod compatibility 勾选 “Reset SMP physics when the outfit changes”）。
- 调用链：`OutfitSystem::refreshArmorFor(actor)` 末尾 → `SOS::SMP::ResetActor(actor)` → `SKSE::GetTaskInterface()->AddTask` → 取 `ActorHandle`、判 `Is3DLoaded()` → `DynamicHDT.ResetPhysics(actor, false)`。
- 只影响目标角色：FSMP 的 `ResetPhysicsImpl` 按 `skeleton->GetUserData()->formID == actor->formID` 找到那一个 skeleton 后 `softReloadMeshes()`，不调用全局 `smp reset`，也不遍历 NPC。
- 兼容性：未装 FSMP / `DynamicHDT` 类型不存在时只写一条 debug 日志并跳过；设置页会显示检测结果。`full=false` 是温和重建，避免整件装备重新加载造成卡顿。
- 日志（debug）：`SMP: Faster HDT-SMP (DynamicHDT) is not installed; the physics reset is skipped.` / `SMP: DynamicHDT.ResetPhysics could not be dispatched for {:08X}.`

### 4.29 SexLab / OStim 场景自动换 `_sex` 套装（1.5.0）

- 新配置：`[Compatibility] SceneOutfitSwitching = off`（默认关；设置页 Mod compatibility 勾选 “Wear <current outfit>_sex during SexLab / OStim scenes”）。
- 事件：`SKSE::GetModCallbackEventSource()` 的 `ModCallbackEvent`；OStim 用 `ostim_start` / `ostim_end`，SexLab 用 `PlayerAnimationStart` / `PlayerAnimationEnd`（只监听 Player 前缀，NPC-only 线程不触发）。
- 开始：当前幻化名为空 → 不变；名称已以 `_sex` 结尾（大小写不敏感）→ 不变；否则 `candidate = 当前名称 + "_sex"`，在 `OutfitService` 里按忽略大小写精确查找；找不到 → 不变；找到 → 记录 `previous/sceneOutfit`，换上并 `refreshArmorFor(player)`。
- 结束：若玩家仍穿着场景套装（忽略大小写比较）→ 恢复 `previous`（空名表示取消幻化）；若玩家中途手动换了别的套装 → 不覆盖玩家选择。
- 日志（debug）：`SceneOutfits: ... scene started; 'A' -> 'A_sex'.` / `SceneOutfits: ... scene ended; 'A' restored.` / 找不到时 `no 'A_sex' outfit for the ... scene; the current outfit is kept.`
- 局限：事件只带线程号，不带参与者列表，因此只对玩家生效；OStim 的 NPC 子线程（`ostim_subthread_*`）不处理。

### 4.30 1.5.1 性能与健壮性审计

- **菜单性能**：OutfitService 增加原子 revision；MenuUI 的套装名与过滤结果按 revision 缓存；Outfit 增加槽位与策略缓存；服装编辑器的完整护甲列表按 TESDataHandler + 护甲数组 size/data 缓存；装备访客改 vector 并 reserve(32)。菜单打开时不再每帧复制全部套装名、不再每次搜索扫描全表。
- **锁范围**：Outfits / Actors / 编辑器先用短读守卫复制快照再绘制，写操作单独短写锁；避免渲染线程的皮肤钩子在菜单打开期间等待整帧。
- **线程安全**：随机数改 thread_local；热键状态改 mutex + atomic；MenuBridge 可用标志与 MenuUI 注册/快选刷新标志改 atomic；快选窗刷新与焦点定位延迟到渲染线程；修复文本框松开后 latch 卡住。
- **健壮性**：装备访客空指针判定、SKSE 任务接口判空、RefreshActor 的 Is3DLoaded 检查、Outfits 页码 clamp、套装名缓冲区 257 字节并在服务层强制 256 字节上限、DisplaySet 使用真实 enabled 状态、InstallHooks=0 真正生效、新游戏重置 enabled=true。
- **文案**：中文“套装”统一改为“幻化”（Policy.cpp 旧别名保留用于历史 JSON）；语言文件重新生成，仍为 233 条。
- **未改的已知限制**：AutoSwitch::Tick 仍由输入事件心跳驱动（2 秒节流），玩家完全静止且天气变化时要等下一次输入才评估；PadCompat 的 g_orphanMask 对已移除 actor 只做小缓存，不清理；Settings::IsGoldenItem 的懒解析依赖 WarmUp，异常路径无锁。

### 4.31 1.5.2 第二轮无人值守审计（线程安全 / 空指针 / 缓存）

- **线程安全**：`AutoSwitch::Tick` 在输入线程仅做 2 秒节流与 `g_tickPending` 排队，`TickOnMainThread` 才遍历 actor、评估状态并刷新；随机换装/快选热键的 `Fire` / `FireQuickPicker` 也改为 `SKSE::GetTaskInterface()->AddTask`。文本输入状态由渲染线程采样、在 `NGHotkeys` 缓存 500ms，输入 sink 不再读 ImGui。
- **Settings 黄金件**：`_goldenResolved` 改 `std::atomic<bool>` + `_goldenMutex`；`WarmUp` / `InvalidateResolvedForms` 加锁，`IsGoldenItem` 只读 atomic 后查集合。
- **空指针防护**：MenuUI 的 Add player 判玩家；TargetName / RenderOneActor / 删除确认 / MenuOutfitEditor 目标名对 `GetDisplayFullName()` 空值兜底。Patches 的 `FixEquipConflictCheck` 判 item/target/ActorEquipManager；战斗/死亡事件判玩家；`RegisterEvents` 判 ScriptEventSourceHolder；`CreateRefreshAllArmorsTask` 判任务接口。
- **缓存正确性**：`OutfitService::check_consistency` 在插入玩家或清理非法 FormID 后递增 revision；删除套装时同时失效快速选择列表与过滤结果；状态自动切换勾选与 13 状态套装修改后调用 `NGFeatures::ApplyCurrentStateOutfit` 立即应用当前状态。
- **界面**：所有菜单页推进 `FlushSettingsIfDue()`，在快选/Outfits 改页大小不必再打开 Settings 才落盘；热键输入框随 ini 值刷新；Page size 范围 5..500，List width 240..1200；删除套装当前帧不再显示旧行。
- **性能**：OutfitService 读写守卫改栈上返回值（去掉皮肤热路径每次 `make_unique`）；DisplaySet 的访客直接写调用方 vector、`ComputeDisplaySet` 复用 `thread_local` 缓冲；名称搜索无分配大小写折叠，仅收藏过滤时才查哈希；Outfits 页复用页内 vector，非当前穿着行不复制 label；编辑器背包/穿戴列表 500ms TTL 缓存；PadCompat 诊断映射上限 512。
- **场景重叠**：SceneOutfits 的 `g_activeSources` 支持 OStim/SexLab 同时开始，同一来源重复 start 忽略，最后一个场景结束才恢复进入前的套装。
- **构建**：version.h / CMake / vcpkg 升 1.5.2；本次因 version.h 触发全量编译，`--parallel 4` 首次报 C1076（页面文件不足），两个 buildtask 改为 `--parallel 1` 后 `BUILD_EXIT=0`、0 error。

### 4.32 1.5.3 第三轮无人值守审计（存档/脚本边界、状态判定、构建）

- **Papyrus 删除/覆盖不刷新**：DeleteOutfit 删除成功后排队刷新；OverwriteOutfit 改走 OutfitService::replace_outfit_armors()（整体替换 + revision 递增）并排队刷新。修复第三方脚本删除/覆盖当前显示套装后幻化残留到下次装备刷新。
- **状态套装漏判**：AutoSwitch、战斗事件、死亡事件的资格判定从只看 should_override()（隐含要求基础套装）改为 enabled && 该角色开启状态切换 && (有基础套装 || 任一状态套装已配置)；战斗/死亡事件同时补 per-actor 的 stateBasedAutoSwitchingEnabled 判定，关闭开关后不会再被战斗事件强行换回状态套装。
- **随机换装**：仅收藏且唯一收藏就是当前套装时，候选为空导致热键无反应；现在回退到全部套装，再回退到允许抽当前套装。
- **坏存档/坏 JSON**：co-save v7 与 OutfitSystemData.json 的字段类型异常原本会抛 nlohmann 异常终止进程；两处加载都加了 try/catch，只记 error 并返回失败。
- **金件读竞争**：_resolvedGolden 改为 shared_ptr 快照 + atomic_load/store，IsGoldenItem() 的渲染线程读路径无锁。
- **构建**：从 target_compile_options 移除 /MP（它会绕过 --parallel 1 再开满核 cl.exe，4.8 GB 页面文件下触发 C1076/C3859）。
- **翻译**：词典去重后 237 条唯一词条（233 + 12 新增中 8 条与旧条目重复，已合并），语言文件按 237 条重生成。
### 4.33 快速选择窗口手柄导航全局失效（1.5.4；结论已被 §4.34 修正）

- **1.5.5 更正**：本节结论不完整。实机确认 SMF 3.9 升级后 1.5.4 原样恢复，主因是旧框架对自定义窗口的 gamepad 处理；NoNavInputs 只是叠加因素。**1.5.6 再更正**：3.18 也已实测正常，3.23 不是分水岭，版本门控已下调到 3.18。
- 现象：1.5.0 起打开过快速选择窗口后，不仅窗口内手柄无反应，SMF 主菜单的十字键/A/B 也全部失效；键盘与鼠标仍正常。
- 根因：快选窗带有 `ImGuiWindowFlags_NoNavInputs`。ImGui 1.90 的 `io.NavActive` 要求当前 `g.NavWindow` 没有 NoNavInputs；快选窗成为 NavWindow 后，整个 ImGui 上下文的 NavActive 被置为 false，SMF 的 GamepadNavigation 随之停止。`NoNavFocus` 只影响 Ctrl+Tab，不是主因。
- 修法：去掉快选窗的 NoNavInputs/NoNavFocus，只保留 NoCollapse；运行期绑定 `igPushItemFlag`/`igPopItemFlag`，在 `RenderQuickPickerBody()` 前后压入 `ImGuiItemFlags_NoNav`（1 << 3），让快选窗内部控件不参与 ImGui 导航但窗口仍是 NavWindow。这样 `io.NavActive` 保持为真，SMF 主菜单手柄恢复；快选窗的 W/S/A/D、十字键、A/B/X/Y 仍由自己的 `IsKeyDown`/`IsKeyPressed` 处理。
- 这两个导出属于可选绑定：缺失时只记 `ImGuiApi: igPushItemFlag/igPopItemFlag are missing ...`，快选窗仍能打开。

### 4.34 快速选择窗口手柄：SMF 3.9 / 3.18 / 3.23（1.5.5–1.5.6）

- 现象：1.5.4 + SKSE Menu Framework 3.9 时，打开快速选择窗口后窗口内与 SMF 主菜单的手柄都无反应；键盘正常。
- 实机确认：**只把 SMF 3.9 升级到 3.18 或 3.23，1.5.4 未改任何代码，手柄全部恢复**。因此主因是 3.9 对自定义 `AddWindow` 窗口的 gamepad 处理，不是幻化钩子；3.23 不是分水岭，3.18 已可用。上游源码核对：3.14.0 引入 `GamepadNavigation` 并修复 gamepad 不可用的回归（提交 8fb2d29）。
- 兼容加固：`MenuBridge::WindowInterface` 显式置 `BlockUserInput = true`；1.5.6 起仅当 `FrameworkVersion() < 3.18`（或取不到）时注册 `RegisterInpoutEvent` 原始回调，快选窗打开时直接读 `RE::ButtonEvent` 的十字键/A/Y/B，渲染线程消费；3.18+ 不注册，避免 `InputEventHandler::Process` 把事件从框架队列中移除。1.5.5 初版阈值原为 3.23，1.5.6 按 3.18 实机结果下调。
- 日志：`Menu: SKSE Menu Framework <版本> (api <n>)`；旧版有 `raw gamepad input enabled ...`；3.18+ 有 `handles gamepad input ...`。

### 4.35 快速选择窗口 X 键取消幻化（1.5.7）

- 需求：快速选择菜单给“取消幻化”分配快捷键 X，键盘也沿用 X。
- 实现：`MenuUI.cpp` 新增 `QuickRemoveOutfit(RE::Actor*)`（清空当前套装、`Features::RefreshActor`、HUD `Outfit removed. / 已取消幻化。`）。键盘 `ImGuiKey_X`，手柄 `ImGuiKey_GamepadFaceLeft`；旧 SMF（<3.18）的 raw 路径用 `RE::BSWin32GamepadDevice::Key::kX` 置 `g_padRemove`。底部 Remove outfit 按钮也改调同一个 helper。
- 文本输入（搜索框等 `IsAnyItemActive`）时不响应 X；底部提示与按钮 tooltip 改为 `X: remove` / `（X）`。

### 4.36 `.tri` 形变实时刷新（1.5.7）

- 现象：幻化件带 `.tri` 形变（OBody、游戏内实时换身形预设等）时，改 morph 滑块或换预设后模型不跟着变，必须重新穿脱套装或先取消幻化。
- 根因：`ApplyArmorAddon` 对同一 `item` 会复用 BipedAnim 里已有槽位，`refreshArmorFor()` 只重跑皮肤钩子，不会重建 `partClone`；旧 clone 的顶点缓冲仍是旧形变。
- 实现：新增 `src/cpp/MorphRefresh.{h,cpp}`，运行期通过 `skee` messaging（0x9E3779B9）取 RaceMenu `BodyMorph` v4+ 接口，250 ms 节流轮询已幻化且 3D 已加载 actor 的 `VisitMorphValues` FNV 签名；变化时开 2 秒强制窗口并 `Features::RefreshActor()`。`Patches.cpp` 的 `Custom` 在窗口内先对幻化槽调用 `BipedAnimAugments::ClearBipedPart`（15496/15661）并清空 `item/addon/part`，再 `ApplyArmorAddon`；1.5.9 起改为按整个 displaySet 的并集清槽并重贴全部显示件。重建后 150 ms 调 `ApplyBodyMorphs(actor, true)` 处理 partClone 延迟挂载。
- 兼容：1.5.10 起没装 RaceMenu 或 BodyMorph < 4 时会尝试 OBody 4.4.0+ 插件 API；两者都没有才停用 `.tri` 刷新，其余功能不受影响；未幻化 actor 不参与轮询。
- 限制：RaceMenu 接口是按 ABI 手写的最小声明（`VisitMorphValues` 槽位 10、`ApplyBodyMorphs` 槽位 13），待实机验证。

### 4.37 身体槽位列表“移除”无反应（1.5.8）

- 现象：在 SMF 幻化栏选中套装后，右侧 Body slots 列表点“移除”没有任何可见反馈；日志只有 trace/debug，没有 Outfit editor 或 Removed 记录。
- 根因：第一次点击只把护甲指针写进 g_pendingRemove，确认条 RenderRemoveConfirmation 固定渲染在编辑器顶部；用户滚到下方的槽位列表时看不到它。另一个叠加因素是按钮 X 用护甲名宽度计算，长名字会把按钮推到子窗口可视区外。
- 修法：删除顶部确认条，改为行内确认——被点击行先显示 Remove ... from the outfit? 提示，同一行出现 Remove / Cancel；确认后才调用 RemoveArmor，取消只清状态。按钮 X 改为按 IG::ContentRegionAvail().x clamp，长名称不会再把它挤出可视区。Invalidate 清空 g_pendingRemoveSlot，切换套装时自动取消上一个套装的确认。
- 日志：删除成功写 info 级别 Outfit editor: removed 'X' from 'Y'.；NotifyChanged 仍打印套装当前件数（debug）。
- 未实机验证：行内确认与按钮位置需要进游戏点一次确认。

### 4.38 OBody 换身形后多件幻化不刷新（1.5.9）

- 现象：幻化套装有 .tri 形变时，在 OBody 换身体预设，32 槽位带身体参照模型的上衣刷新了，但裤子等其他部件仍是旧模型，直接穿模。
- 根因：1.5.7 的强制重建分支只清空并重贴 applySet（显示集中不属于实际穿戴的幻化件）。displaySet 里同时有真实穿戴的护甲时，这些护甲由引擎应用、被 Custom 的 applySet 过滤跳过，从未执行 ClearBipedPart，旧 partClone 的顶点缓冲仍是旧形变。
- 修法：Custom 先对整个 displaySet 每件护甲的 GetSlotMask().underlying() 取并集 displayMask；forceMorphRebuild 时 rebuildSet 改为 displaySet 全部件，先 ClearSlots(actor, displayMask) 对第一/第三人称一次清空涉及槽位（1.5.11 更正：只清当前 `Custom` 的 `ActorWeightModel`，清两套会把刚重建的另一套清空），再逐件 ApplyArmorAddon；每件后仍按 PAD 开关 RestoreRealItems。非强制路径仍只处理 applySet。
- 诊断：开 Verbose logging 后可见 debug 行 Custom: force-rebuilding N display pieces (mask 0x........) for ........ after a body morph change.；默认日志保持安静。
- 未实机验证：1.5.9 需要 OBody + RaceMenu BodyMorph v4+ 环境（1.5.10 起改为 OBody 4.4.0+ 插件 API 也可），按 docs/TESTING.md §3.22 / §3.23 跑多件/真实穿戴/第一人称用例。

### 4.39 OBody-only 预设刷新（1.5.10）

- 现象：1.5.9 仍复现 OBody 换预设后幻化不刷新；用户环境没有 RaceMenu，日志只有 `MorphRefresh: RaceMenu is not installed; the .tri morph refresh stays off.`，没有任何 `Custom: force-rebuilding`。
- 根因：1.5.9 的 `MorphRefresh` 只有 RaceMenu `BodyMorph` 后端。拿不到 `IBodyMorphInterface` 时 `g_api` 为空，`Tick()` 开头直接 return，`ShouldForceRebuild()` 永远 false；1.5.9 已写好的 displaySet 全量重建分支因此从未被触发。取消幻化/切别的套装会重新走 `Custom` 并重贴，所以看起来“切一下就好”。
- 修法：新增 OBody 插件 API 后端（OBody 4.4.0+）。`kDataLoaded` 时向 receiver `OBody` 发送 `RequestPluginInterface`（`0xc0B0D9cc`，v1），静态 `IOBodyReadinessEventListener` 跟踪 ready；250 ms 轮询 `GetPresetAssignedToActor` 的预设名并混入 FNV 签名；任一后端可用即工作。
- OBody-only 时签名变化先调用 `ApplyOBodyMorphsToActor`，再 `Features::RefreshActor()`；`Custom` 的 2 秒强制窗口按 displaySet 并集 `ClearBipedPart` 后重贴全部显示件。RaceMenu 存在时仍以其 BodyMorph 为权威。
- 日志：`MorphRefresh: OBody plugin API v1 acquired` / `MorphRefresh: OBody is ready` / `MorphRefresh: no body morph provider is available`。
- 未实机验证：OBody API 为按头文件 ABI 手写的最小声明；OBody-only 下 `ApplyOBodyMorphsToActor` 是否同步应用形态未知。按 docs/TESTING.md §3.23 验证。

### 4.40 OBody 换预设后身体与衣服整体消失（1.5.11）

- 现象：幻化带 `.tri` 的服装后，在 OBody 换身体预设，衣服模型与身体模型全部消失，只剩头部与手有模型；按 Z 重新点一次当前套装就恢复。
- 根因：`CustomSkinPlayer::Custom` 每次刷新会被调用两次（第一人称与第三人称各一次）。1.5.9 的 `ClearSlots(actor, displayMask)` 把两套 `BipedAnim` 一起清空，但每次 `Custom` 只把自己的 `actorWeightModel` 重新贴满；第二次调用把第一次刚重建的那套又清掉，于是第三人称的身体与衣服整体消失。
- 修法：`ClearSlots` 改签名 `void ClearSlots(RE::ActorWeightModel* a_model, std::uint32_t a_mask)`，从 `a_model`（实际是 actor 的 `BSTSmartPointer<BipedAnim>` holder 地址）取当前这套 `BipedAnim`，只清它；调用点改 `ClearSlots(actorWeightModel, displayMask)`。displaySet 全量重贴、PAD 诚实还原、NotifyRebuilt 均不变。
- 未实机验证：本机无 Skyrim；按 docs/TESTING.md §3.24 先测第三人称，再切第一人称与第三人称。
- RaceMenu：用户环境已装 RaceMenu 4.0.0（1.5.97），但日志仍是 `Failed to dispatch message to skee`，说明这次运行没有接上 skee 消息监听；不影响 1.5.11 的 OBody 修复，若要用 RaceMenu 后端请先确认 `skse64.log` 里 skee64.dll 加载成功。

### 4.41 OBody 换预设后其余幻化件仍穿模（1.5.12）

- 现象：1.5.11 修好模型整体消失后，OBody 换预设只有带身体参照的 32 槽上衣换了身形；裤子、鞋子、头发、发饰仍是旧模型，出现穿模；取消幻化或切走再切回就正常。
- 根因：OBody NG 的 `ApplyOBodyMorphsToActor` → `ReapplyActorMorphs` → `GenerateBodyByPreset` → `ApplyMorphs` 里，`ApplyBodyMorphs(actor, true)` 是排队生效的；1.5.11 在签名刚变时立刻重建，克隆仍从旧 morph 生成。并且 OBody 会在装备变化后重新派生预设/ORefit，而我们的重建本身就像一次装备变化，所以形态会再变一次。另一个隐患：RaceMenu 在 1.5.97 上 `kDataLoaded` 时可能尚未应答消息交换，1.5.10 只试一次就永久停用 RaceMenu 后端。
- 修法：
  1. `NotifyRebuilt()` 不再要求 RaceMenu —— RaceMenu 或 OBody ready 任一存在就排 150 ms morph pass；OBody-only 走 `ApplyOBodyMorphsToActor`，由 OBody 自己重写所有网格（含克隆）。
  2. 签名变化后除立即 `RefreshActor()` 外，再排一次 **500 ms 延迟重建**（`g_pendingRebuild` / `g_rebuildScheduled` 去重，受 2 秒强制窗口保护）；这次重建再触发一次 morph pass，等 OBody 稳定后克隆才会读到新形态。
  3. 新增 `RetryProviders()`：RaceMenu 接口每 2 秒重试、最多 30 次；成功后切回 RaceMenu 权威路径（`ApplyBodyMorphs(actor,true)`），不再依赖 OBody 重放。
- 日志：`MorphRefresh: queued the body morph pass for <FormID>.`（trace）、`MorphRefresh: rebuilding the illusion of <FormID> after the body morph provider settled.`（debug）。
- 未实机验证：本机无 Skyrim；按 docs/TESTING.md §3.25 复测，重点看裤子/鞋子/头发/发饰是否一起跟随。

### 4.42 手动重穿不刷新 / OBody 仍不跟随（1.5.14）

- 现象（m20054）：1.5.12 实机，OBody 换预设后裤子、鞋子、头发、发饰仍不跟随；按 Z 重新点当前套装没有任何变化。
- 根因一：1.5.12 只有 body morph 签名变化会进全量重建；玩家自己重穿同一套时只有 `OutfitService` 的 revision 变了，`Custom` 仍走 `applySet` 便宜路径，不清槽，`ApplyArmorAddon` 认为槽位已是最新，旧 clone 原样留下。
- 根因二：OBody 的 `ApplyBodyMorphs` 排队生效，并在装备变化后再派生一次形态；500 ms 的一次补建仍可能早于 OBody 落定。
- 修法：
  1. `RevisionRebuildPending(FormID, revision)`：per-actor 记录上次 revision，变化后 2 秒内全量重建（覆盖第三人称 + 第一人称两次调用）。
  2. `MorphRefresh` 两段延迟重建 400 ms / 1400 ms，强制窗口 4 秒。
  3. RaceMenu 重试 5 秒 × 12 次降噪。
  4. 全量重建逐件打印 slot clone 指针，便于判断是否真的换了几何。
- 日志：`Custom: rebuilding <n> display pieces (mask 0x........, outfit revision true/false, body morph true/false) for <FormID>.`、`Custom: <Armor> slot <nn> clone 0x...`、`MorphRefresh: final rebuild of the illusion of <FormID> a second after the body morph change.`
- 未实机验证：本机无 Skyrim；按 docs/TESTING.md §3.26 复测。

## 4.43 1.5.14 幻化循环：换身形后再不穿模

用户 m20054 / m20257 两次复报同一问题。1.5.9–1.5.13 一直在「原槽位清空 + 重贴」上打补丁，方向是错的：

- `OutfitService::set_outfit_c` 等指派类接口从不 `++_revision`，所以按 Z 重穿同一套时 `RevisionRebuildPending` 永远拿到 `false`，1.5.13 的整段逻辑根本没被触发（实机日志 16 次 `Custom` 全为 `outfit revision false`）。
- 即便触发，在同一个 `BIPOBJECT` 上 `ClearBipedPart` + `ApplyArmorAddon` 也只是重填条目，引擎不会为新克隆重新加载 `.tri` 形变目标（日志里 48 条 `slot .. clone 0x0`，此刻 `partClone` 尚未挂上）。

1.5.14 改成自动执行玩家手动验证过的那套动作：

1. 指派类接口补 `++_revision`。
2. `MorphRefresh` 增加「幻化循环」：签名变化或用户重穿同一套时，先清空指派并刷新（幻化摘掉、引擎按真实护甲走一遍），250 ms 后恢复指派并刷新（幻化重新穿上，克隆在形变落定后创建）。
3. 同一角色只排一次循环，`Reset()` 清表。

日志：`MorphRefresh: taking the illusion of ... off for one pass` / `MorphRefresh: illusion of ... restored with a fresh morph target.` / `Custom: rebuilding ... outfit revision true`。

## 4.44 1.5.15 幻化循环去闪烁：同帧完成摘掉与穿回

### 现象与根因

- 1.5.14 用 250 ms 间隔把循环拆成两个主线程任务，中间十几帧渲染的是真实护甲，玩家看到「先还原成原版装备再变回幻化」。
- 那次间隔并非必需：`Features::RefreshActor()` 内部会同步走完 AI 装备流程，日志顺序（`OutfitSystem.cpp:144: Armor Refresh ...` 紧跟 `Patches.cpp: Custom: ...`）证明皮肤钩子就在这次调用里执行。

### 修法

1. `MorphRefresh::RunIllusionCycle(actor, reason)` 顺序执行：`set_outfit_c("")` → `RefreshActor`（引擎按真实护甲走一遍，拿到新克隆）→ `set_outfit_c(name)` → `RefreshActor`，全部在同一个任务内，函数返回时才回到渲染循环。
2. `NGFeatures::AssignOutfit()` 遇到「重穿同一套」直接调用循环并 return，不再先做一次多余的 `RefreshActor`，也不走 250 ms 心跳节流，Z 键即时生效。
3. 心跳轮询发现身形签名变化时同样直接调用循环（先补一次 OBody morph 应用），不再排 250 ms 延迟。
4. 兜底：`[Compatibility] ReillusionGapMs`（0–1000，默认 0）。大于 0 时退化为两段式，间隔由该值决定（会短暂露出原版装备）。

### 日志对照

| 期望顺序（同一帧内） | 说明 |
| --- | --- |
| `MorphRefresh: taking the illusion of <FormID> off for one pass (body morph changed)` | 开始循环 |
| `Patches.cpp: Custom: rebuilding <n> display pieces (... body morph true) for <FormID>.` | 第 1 次刷新（引擎 pass） |
| `MorphRefresh: illusion of <FormID> restored with a fresh morph target.` | 紧接着开始第 2 次刷新 |

### 未验证

本机无 Skyrim，同帧循环的实际显示效果（是否还有 1 帧闪烁、两次刷新的帧耗时）未实机确认；`ReillusionGapMs` 仅作为退路。

## 4.45 幻化栏复制 / 粘贴整套预设（1.5.16）

需求（m20513）：选中一套幻化 → 点复制即把整套预设（名称、槽位规则、单槽位规则、部件）放进剪贴板；点粘贴读取剪贴板，按里面的套装名判断重名 —— 无重名沿用原名新建，有重名加 `_copy` 后缀新建，槽位规则与部件按原 JSON 结构还原；粘贴完成后左侧列表自动焦点到新套装。

### 剪贴板格式

写出去的就是**原版 `OutfitSystemData.json` 里单个 outfit 对象**的结构（1.4.3 起的数字枚举版）：

```json
{
  "name": "束缚巫女服-粉色",
  "isFavorite": true,
  "armors": [
    { "rawFormId": 16777218, "localFormId": 8450, "modName": "Skyrim.esm" }
  ],
  "slotPolicies": { "9": 8 },
  "slotPolicy": 2
}
```

- `slotPolicy` / `slotPolicies` 写数字枚举（0–11、128+），与原版一致；部件带 `modName` + `localFormId`，跨存档/跨机器也能按插件名+本地 ID 解析回来（解析不到再退回 `rawFormId` 查）。
- 粘贴同时接受外面那层 `{"outfits":[...]}`（取数组第一项），所以从 `OutfitSystemData.json` 里剪一段、或从别的玩家那里拿到的片段都能直接粘。

### 实现

| 位置 | 内容 |
| --- | --- |
| `src/cpp/include/OutfitClipboard.h` / `src/cpp/OutfitClipboard.cpp` | `Copy(name)` 组装 JSON 并用 Win32 剪贴板 API（`CF_UNICODETEXT`，UTF-16）写出；`Paste(createdName)` 读回文本、解析、解析部件与策略、算免重名、调用 store 创建 |
| `OutfitService::create_outfit_preset()` | 一次加锁内建好整套：先按「无覆盖」重置 32 槽 + 基础策略，再套用 JSON 里的基础策略与单槽覆盖，最后插部件 —— 缺字段不会继承构造函数默认值（这与导入 `OutfitSystemData.json` 的处理一致） |
| `MenuUI` 底部 | `Copy` / `Paste` 两个按钮（`Create empty` 之后），复制/粘贴结果全部走 HUD 提示；粘贴成功后 `FocusOutfitInList()` 选中新套装、翻到它所在页，行渲染时 `SetScrollHereY(0.5)` 把它滚到视野中间 |

重名规则：`名字` → 被占则 `名字_copy` → 再占则 `名字_copy2`、`名字_copy3`…（最多尝试 100 次，超长名字按 UTF-8 边界截断后再拼后缀）。

### 日志与提示

- `Outfits: copied 'X' to the clipboard (N bytes of JSON).`
- `Outfits: pasted 'base' as 'name_copy' (N pieces, blanket XXOO, M slot overrides).`
- HUD：`Outfit copied to the clipboard.` / `Pasted outfit 'name_copy'.`；失败分别是「剪贴板里没有文本」「其他程序正在占用剪贴板」「内容不是有效 JSON」「里面不是幻化数据」。

### 未验证

本机无 Skyrim：两个按钮的实际点击、剪贴板读写、粘贴后列表焦点与滚动都是静态实现，未实机验证；按 `docs/TESTING.md` §3.29 复测。

## 4.46 SLIF NG 膨胀框架兼容（1.5.17）

需求（m20732）：SLIF NG（BodyInflationNG.dll，github.com/crajjjj/SLIFNG，Nexus 192276）改变身形时，幻化件要跟着一起变形；不能像 OBody 那样靠「一换预设就整轮重建」的笨办法。

### 为什么原来完全没反应

| 环节 | 事实 |
| --- | --- |
| SLIF 的输出 | 经 RaceMenu skee 写两样东西：BodySlide morph（`SetMorph` + `ApplyBodyMorphs`）与骨架节点缩放（`AddNodeTransformScale` + `UpdateNodeTransforms`），全部挂在 key `SexLab Inflation Framework.esp` 下 |
| 我们的信号 | 只有两路：RaceMenu `BodyMorph` 的 `VisitMorphValues` 签名、OBody 的 `GetPresetAssignedToActor` 预设名签名 |
| 断点 | 用户环境（1.5.97 + RaceMenu 4.0.0）里 `Dispatch(0x9E3779B9, ..., "skee")` 一直失败，日志每 5 秒一条 `Failed to dispatch message to skee`；morph 签名从未工作，SLIF 改滑块我们看不到 |
| 为什么名字会 miss | SKSE 2.1.2+ 把 receiver 名解析成「注册了消息监听器的插件」列表下标，任何一个 DLL 加载失败都会让后面的名字整体错位；skee64 自己用 `RegisterListener(nullptr)` 收所有 sender |

### 三条修法

1. **skee / OBody 握手加无名广播**：命名 dispatch 没拿到时就再发一次 receiver = `nullptr` 的广播（SLIF NG 的 `SKEE::GetInterfaceMap()` 用同一手法，作者注释也写明 1.5.97 上 RaceMenu 会晚答）。
2. **SLIF NG 直连后端**：`GetModuleHandleA("BodyInflationNG.dll")` → `GetProcAddress("SLIFNG_GetQueryInterface")` → `IQueryInterface1*`。每 250 ms 对每个已幻化 actor：`IsTracked` 为真时读 4 个规范键的 `GetApplied`（1.0 中性；ramp 中是 in-flight 值），四点混进签名。因为它是**纯 C 导出**，不经过 SKSE 消息，不受名字错位影响；0.4.17 及更老的版本没有这个导出，退回 `0x534C4946 'SLIF'` 消息握手。
3. **签名去抖**：签名静置 250 ms 才触发幻化循环，同一角色两次循环至少隔 600 ms。膨胀是连续值（SLIF ramp、OBody 分步写），逐值重建会把画面刷爆；去抖后一次膨胀只重建一次，ramp 落定后再补一轮。

### 现在覆盖到的形变来源

| 来源 | 信号 |
| --- | --- |
| SLIF NG 膨胀（morph 路径） | SLIF 的 `GetApplied` + RaceMenu morph 签名 |
| SLIF NG 膨胀（节点缩放路径） | SLIF 的 `GetApplied` |
| OBody / OBody NG 换预设 | 预设名签名（+ `ApplyOBodyMorphsToActor` 重放） |
| RaceMenu 手动改 Body Morph 滑块 | RaceMenu morph 签名 |
| 其它写 skee morph 的 mod | RaceMenu morph 签名 |

### 日志

- `MorphRefresh: SLIF NG query interface v2 acquired (N tracked actors); inflation changes will rebuild the illusion.`
- `MorphRefresh: SLIF NG is not installed; inflation changes are not watched directly (the morph signature still covers them).`
- 变化触发时仍是 1.5.14/1.5.15 那一对：`taking the illusion of <FormID> off for one pass` → `illusion of <FormID> restored with a fresh morph target.`

### 未验证

本机无 Skyrim：SLIF 接口的 vtable 顺序按 `SLIFNG_API.h` 抄写（`Version / IsTracked / TrackedActorCount / GetValue / GetMinValue / GetMaxValue / GetApplied / GetCombinedMorph / GetCalculationType / IsIncrementalInflation`），广播兜底与去抖均未实机验证。按 `docs/TESTING.md` §3.30 复测。

## 4.47 1.5.18 三条身形症状：几何签名 + 去掉冗余重建

需求背景（m20924，两台机器的日志）：①幻化空套装时拖 RaceMenu Body Morph 滑块生效，换上带 `.tri` 的幻化后反而不跟随；②OBody NG 换身形预设明显变卡；③SLIF 膨胀后幻化仍不跟随。

### 三个根因

| 症状 | 根因 |
| --- | --- |
| RaceMenu 滑块不跟随 | 该环境（1.5.97 + RaceMenu 4.0.0）里 `Dispatch(0x9E3779B9, ..., "skee")` 一直失败（`Failed to dispatch message to skee` 每 5 秒一条），RaceMenu morph 签名从未工作 —— 与「之前的错误判断干扰」无关，是根本没有信号；空套装外观本来就是真实裸体，看起来才像「生效」。 |
| OBody 换预设变卡 | 一次预设变化同时触发 `ApplyOBodyMorphsToActor` + 幻化循环（2 次刷新）+ 1.5.13 遗留的 400 ms 与 1400 ms 两轮额外全量重建，同一角色共 4 次 |
| SLIF 膨胀不跟随 | 签名只读 4 个规范键（`slif_belly` / `slif_breast` / `slif_butt` / `slif_scrotum`，中性值 1.0），而 SLIF 与 Fill Her Up 走 `SLIF_Morph` 写 `morph:PregnancyBelly` 一类滑块，签名完全不动 |

### 五条修法

1. **几何签名（供应商无关）**：新增 `ComputeGeometrySignature(RE::Actor*)`。遍历第一与第三人称两套 `actor->GetBiped(firstPerson)` 的 `objects[i].partClone`，`AsDynamicTriShape()` → `GetDynamicTrishapeRuntimeData()`，要求 `dynamicData` 非空且 `dataSize >= 64`；对头 1024 字节逐字节、其后步长 251 稀疏采样，混入 `dataSize`，再按 `0x9E3779B97F4A7C15 * (i + 1)` 与槽位序号混合（`BIPED_OBJECTS::kTotal` = 42，单槽上限 16 个 shape）。任何写顶点缓冲的东西都会让签名变化，不再依赖 provider。
2. **SLIF 滑块进签名**：除 4 个规范键外再读 10 个滑块（`PregnancyBelly` / `PregnancyBelly_Edge` / `PregnancyBelly_SLIF` / `BelatedBelly` / `Breasts` / `BreastsSH` / `Breasts_SC` / `Butt` / `Butt_SC` / `GenitalsScrotum`）的 `GetCombinedMorph` 与 `GetApplied("morph:<滑块>")`。
3. **去掉冗余重建**：删除 `g_pendingRebuild2` / `g_rebuildScheduled2`，400 ms + 1400 ms 两段合并成一次 `kSettleRebuildDelay = 700 ms`；OBody 换预设的刷新次数从 4 次降到 3 次（循环 2 次 + 700 ms 补建 1 次）。
4. **quiet 窗口防自触发**：幻化循环结束后立刻重算签名作新基线，并在 `kPostCycleQuiet = 2500 ms` 内把任何位移都当新基线（不触发）；`kSignatureSettle` 250 → 350 ms、`kMinCycleInterval` 600 → 1200 ms。
5. **无 provider 也工作**：`PollOnMainThread` 去掉「三个 provider 全无就 return」的早退，`Tick` 去掉 `haveAny` 门槛 —— 几何探针不需要任何 provider，RaceMenu 完全不应答时照样跟随。

SLIF 探测同时强化：模块名依次试 `BodyInflationNG.dll` / `SLIFNG.dll` / `SLIF.dll`，receiver 依次试 `SLIFNG` / `BodyInflationNG` / `SLIF`，全失败再发 receiver = `nullptr` 的广播。

### 新的日志

| 日志 | 含义 |
| --- | --- |
| `MorphRefresh: no body morph provider answered; morph changes follow the geometry probe only.` | RaceMenu / OBody / SLIF 三路接口全没拿到（info）；此后靠几何签名跟随 |
| `MorphRefresh: SLIF NG is not installed (no ... export and no handshake answer); inflation follows the geometry probe only.` | 没装 SLIF 或版本太老（debug） |
| `MorphRefresh: SLIF NG query interface vN acquired (N tracked actors); inflation follows the slider and geometry probes.` | SLIF NG 已接上（info） |

变化触发时仍是 `taking the illusion of <FormID> off for one pass` → `illusion of <FormID> restored with a fresh morph target.`，之后只再补一次 `rebuilding the illusion of <FormID> after the body morph provider settled`（debug，700 ms）。

### 未验证

本机无 Skyrim：几何签名的采样参数（`dataSize >= 64`、头 1024 字节、步长 251）是按 BipedAnim 顶点量级估的，未在实机确认同一身形两次采样的哈希稳定性（不稳定会自触发循环，靠 quiet 窗口兜底）；OBody「4 次→3 次」的实际观感、SLIF 滑块路（`morph:PregnancyBelly`）是否被 `GetCombinedMorph` 返回非中性值，都待实机。按 `docs/TESTING.md` §3.31 复测。

## 4.48 1.5.19 三条症状一条没解决：skee 握手不再判死 + 探针递归 + provider-first

需求背景（m21255，附日志 `bug/SkyrimOutfitSystemNG.log`）：1.5.18 交付后用户实测「几个问题一个没解决」，并追问「前面说的参考 SLIF NG 框架作者踩的坑又忘了？」（m21281）。

### 日志证据（1.5.97，5323 行）

| 行 | 内容 | 说明 |
| --- | --- | --- |
| 加载期 | `MorphRefresh.cpp:812: RaceMenu did not answer the interface exchange; no RaceMenu BodyMorph interface.` | 只出现一次，之后再无重试日志 |
| 加载期 | `SLIF NG query interface v2 acquired (0 tracked actors)` | SLIF 接上了，但当时没有跟踪中的演员 |
| 00:00:59.459 | `the body morphs of 00000014 changed; rebuilding the illusion.` | 一次 OBody 预设变化 |
| 00:00:59.459 → 00:01:00.090 | `taking the illusion of 00000014 off for one pass` → `illusion of 00000014 restored with a fresh morph target.` | 631 ms 期间真人穿的是原版护甲（露馅 + 卡） |
| 00:01:00.371 | `rebuilding the illusion of 00000014 after the body morph provider settled.` | 又一次全量刷新（+281 ms） |
| 拖滑块期间 | （无任何 MorphRefresh 行） | 几何签名没有看到任何变化 |

### 三个根因

| 症状 | 根因 |
| --- | --- |
| RaceMenu 滑块不跟随 | 重试是「12 次 × 5 秒、从 kDataLoaded 起算」，用户 23:57 加载、00:00:16 才进游戏，窗口在进游戏前就耗尽；而 1.5.97 的 skee 只从自己的消息处理器里才注册监听器（SLIF NG 作者 `src/Skee.cpp:442-459`），握手本来就该在进游戏后继续试 |
| 空套装「生效」、带 .tri 不跟随 | 几何探针只对 `objects[i].partClone` 本身调 `AsDynamicTriShape()`，而它多数是容器节点 → 探针恒 0；空套装看到的是引擎自己重刷的真实裸体 |
| OBody 换预设变卡 | 一次变化跑两套流程：幻化循环（631 ms 露真甲）+ 700 ms 后又一次全量重建 |

### 五条修法

1. **握手永不放弃**：heartbeat 前 8 次每 5 秒、之后每 30 秒重试到会话结束；`kPostLoadGame` 再试；每次真正要用接口前惰性再试（`EnsureProvidersForUse()`，间隔 2.5 秒）。`kMaxProviderRetries` 已删除。
2. **失败诊断**：`ReportRaceMenuSilence()` 用 `GetModuleHandleA` 探测 `skee64.dll` / `skee.dll` / `skeevr.dll`，每 5 分钟最多报一次（上限 8 次），区分「在进程里但不答」与「没装」。
3. **探针递归**：`HashSubtree()` 深度上限 4、每槽 24 个形状，命中 `BSDynamicTriShape` 就混入 FNV，按槽位序号混合。
4. **provider-first**：先 `ApplyBodyMorphs(actor, true)`（RaceMenu）或 `ApplyOBodyMorphsToActor(actor)`（OBody），350 ms 后比对签名，最多再等 4 × 350 ms；仍不动才回退幻化循环。成功路径完全不摘幻化。
5. **删掉二次重建**：移除 1.5.18 的 `kSettleRebuildDelay = 700 ms` 补建登记。

### 新的日志

| 日志 | 含义 |
| --- | --- |
| `MorphRefresh: <dll> is in the process but RaceMenu still has not handed out the BodyMorph interface; kept asking (N tries). The .tri morph refresh waits for it and the geometry probe covers the gap.` | skee 在进程里但不答（info，5 分钟一次） |
| `MorphRefresh: no skee module in the process - RaceMenu is not installed.` | 没装 RaceMenu（info） |
| `MorphRefresh: the body morphs of <FormID> changed; following them.` | 签名变化，走 provider-first（debug） |
| `MorphRefresh: the provider pass moved the meshes of <FormID>; the illusion followed without a cycle.` | provider 刷新生效，没有摘幻化（trace） |
| `MorphRefresh: the provider pass left the meshes of <FormID> alone; taking the illusion off for one engine pass.` | provider 没带动克隆，回退幻化循环（debug） |

### 未验证

本机无 Skyrim。以下都只能靠实机：RaceMenu 接口是否在进游戏后才应答；`ApplyBodyMorphs(deferUpdate=true)` 的落地延迟是否落在 4 × 350 ms 内；provider 是否真的会重建我们的克隆（不重建就会走回退循环，功能不受影响、只是回到 1.5.18 的观感）。按 `docs/TESTING.md` §3.32 复测。

## 4.49 1.5.20 幻化件真正跟随身形：给幻化网格补 `.tri` 目标 + morph 落地即重放

需求（m00002 / m00270）：①RaceMenu 窗口里拖 Body Morphs 滑块；②OBody NG 换身形预设；③SLIF 膨胀 —— 三种情况下带 `.tri` 的幻化件身形必须实时跟着变，且不得出现「先还原原版装备再变回」的中间态。用户明确要求按「morph 真正落地后再刷一次」的方向做，参考 OBody 自己的 `NotifyMorphApplied`。

| 项 | 内容 |
| --- | --- |
| 症状 | 裸体外观（空套装）拖滑块生效，换成带 `.tri` 的幻化件后身形卡住；日志（1.5.18，5323 行）里一次预设变化是 `taking the illusion off`（00:00:59.459）→ `restored`（00:01:00.090）→ 又一次全量重建（00:01:00.371），中间 631 ms 真人穿原版护甲；滑块拖动期间一条 MorphRefresh 记录都没有 |
| 真因 | 幻化 clone 由 `CustomSkinPlayer::Custom()` 里 `ClearBipedPart` + `TESObjectARMA::ApplyArmorAddon` 直接造出，**不经过 skee 的 `AttachBipedObject` 钩子**。引擎正常挂载护甲时会给节点生成 `BODYTRI` 额外数据（相对 `meshes\` 的 `.tri` 路径），而 skee 的 `MorphCache::ApplyMorphs()` 只在子树里找到 `BODYTRI` 时才读 `.tri` 并写顶点，**没有就整棵子树跳过** → 幻化 clone 永远没有 morph 目标，RaceMenu / OBody / SLIF 写多少遍都写不到它。1.5.19 的 provider-first 因此注定看不到顶点变化，只能回退幻化循环 |
| 修法一 | 新增 MorphPush 子模块（`src/cpp/MorphRefresh.cpp`）：幻化每次重建后 `Patches.cpp` 调 `MorphRefresh::NotifySlotApplied()`，排队一次主线程任务；对每个被覆盖槽位的 `partClone` 按 OBody 的规则从 ARMA `bipedModels[性别].GetModel()` 推导 `.tri`（去 `.nif`、去 `_0`/`_1` 再试、`BSResourceNiBinaryStream` 实测存在、按 FormID+性别缓存），子树里没有 `BODYTRI` 就挂一个 `NiStringExtraData`（value 用 `RE::malloc` 从引擎堆分配，插件是 `/MT` 静态 CRT） |
| 修法二 | 事件驱动：skee v5 `IBodyMorphInterface::AddMorphShapeCallback`（RaceMenu 写完顶点、拷到其它 partition 之前触发）+ SKSE `ModCallbackEvent` 订阅 OBody 的 `Obody_ApplyMorph`。回调里**只标脏**（`ApplyMorphs` 持锁，回调里回调 skee 会死锁），节点若已是我们的 clone 则直接忽略 |
| 修法三 | 主线程任务对每个覆盖槽位调 `ApplyVertexDiff(actor, node, false)`：`erase=false` ⇒ `isAttaching=false` ⇒ 先用 `SHAPEDATA` 还原原始顶点再重放全部 morph，幂等；不摘幻化、不重建、任何一帧都不露原版装备 |
| 修法四 | 新增 `AbsorbOwnWrite()`：重写后把结果写成新基线、清掉该角色的 `g_verify`、开 1.2 s 静默窗，避免我们自己的顶点位移被下一轮轮询读成「provider 动了」再跑幻化循环（又会闪原版）；窗口内真有外部变化仍由 1.5.19 的 `quietDrift` 逻辑延后补放 |
| 修法五 | skee `IBodyMorphInterface` 声明补齐到 v5 全 23 槽（原先 `ApplyVertexDiff` 少 `rootNode` 形参、`ApplyBodyMorphs` 之后整段缺失，直接追加回调会落到错误槽位并越界读 vtable）；`GetVersion() < 5` 不装回调；`TryRaceMenu()` 依次试 `"skee64"` → `"skee"` → 无名广播并记录应答通道 |
| 兼容 | 只走公开契约（skee `IPluginInterface` v5 虚表、SKSE `ModCallbackEvent`、引擎自己的 `BODYTRI` 约定），不 hook / patch 任何第三方函数；skee / OBody / SLIF 升级后最坏是回调拿不到，自动退回轮询，不崩、不改原 mod 行为 |
| 日志 | `MorphRefresh: RaceMenu BodyMorph v{} acquired through {}`、`… hands out MorphShapeCallback; the illusion is re-morphed as soon as RaceMenu writes the vertices.`、`… has no MorphShapeCallback (v5 added it); the .tri morph of the illusion follows the poll instead.`、`.tri morph target support active|inactive … for illusion slots.`、`MorphRefresh: re-morphed {:08X} on N node(s), M new .tri target(s) (…); the vertices moved|were already current.`（每角色 5 秒最多一条，debug） |
| 边界 | 本身没有 `.tri` 的护甲件没有可 morph 的顶点数据，行为与不装幻化时一致；1P 模型沿用 `bipedModels` 推导，若某 mod 给第一人称单独配 `.tri` 尚未覆盖 |

### 未验证

本机无 Skyrim。只能靠实机：`patClone` 上的 `BODYTRI` 是否被 skee 认到（看 `re-morphed … N new .tri target(s)` 与 `the vertices moved`）、RaceMenu 4.0.0 的 skee 是否 `GetVersion() >= 5`（否则退回轮询）、`ApplyVertexDiff(…, false)` 的「还原再重放」是否与 RaceMenu 自己的写入叠加出正确结果。按 `docs/TESTING.md` §3.33 复测。

## 4.50 1.5.21 幻化不再全裸：CustomSkinPlayer 站点自定位 + 失败降级

需求（m00746）：「幻化基础功能都没了，幻化套装直接啥都不显示只有裸体了」。1.5.20 交付后基础幻化完全失效，必须先修，同时不碰 1.5.20 的 `.tri` morph 跟随（m00002 / m00270）。

| 项 | 内容 |
| --- | --- |
| 症状 | 菜单里换任何套装都不显示，角色只剩裸体；游戏不崩、菜单/存档/脚本正常。日志（`bug\SkyrimOutfitSystemNG.log`，2062 行，`v1-5-20-0`，runtime `1-5-97-0`）只有一条 error：`HookSupport.cpp:104: CustomSkinPlayer: the call at +0x81 targets 0x7FF66C1610E8, but 0x7FF66C3651D0 was expected; the hook stays disabled so the game keeps running.`，紧接 `HookSupport.cpp:55: CustomSkinPlayer: site rva 0x364301 -> E8 E2 CD C7 FF`；其余四个钩子（vanilla player skinning / shim worn flags / FixEquipConflictCheck / FixSkillLeveling）全部 `verified` 并安装 |
| 为什么一条钩子失败=裸体 | `DontVanillaSkinPlayer::ShouldOverride` 对「不在幻化 displaySet 里的已穿护甲」返回 true，引擎跳过其 `ApplyArmorAddon` —— 真护甲被藏；幻化件不是真实穿戴物，只有 `CustomSkinPlayer::Custom()`（`ClearBipedPart` + `ApplyArmorAddon` 造 clone）会贴上去。前者在、后者缺 ⇒ 真护甲没了、幻化没贴 ⇒ 裸体 |
| 为什么站点对不上 | `E8 E2 CD C7 FF` 解出的调用目标是 `base - 0x1EF18`（模块外，不可能是真 call）；而地址库 ID 24231 → `0x364280 + 0x81 = 0x364301`、ID 15856 → `0x1E51D0` 与代码期望完全一致，同函数区另两个钩子也能对上。结论：站点算法没错，是**这个 exe 该函数内部布局与记录偏移不同**（那处不是这条调用）。本机无游戏本体，无法复现字节，因此改为运行时自定位 + 失败降级 |
| 修法一（自定位） | `Hooking::ResolveSite()` 新增 `a_expectedTarget` / `a_scanWindow`。目标不符时在站点 ±窗口内逐字节找 `E8` 且 `rel32` 精确等于期望函数的调用点：唯一 → warn `moved by N bytes; hooking the relocated site` 并采用；多候选 → 拒绝并逐个打印 `candidate at rva 0x… (N bytes from the nominal site)`（判断不了哪个调用点进入时目标寄存器是 actor，钩错会带错寄存器状态）；窗口内没有但全 `.text` 只有唯一一处调用 → 采用（warn `hooking that one`）；仍没有 → 放弃该钩子并按全部调用点统计留证。只有 `CustomSkinPlayer` 传 `0x800`，其余钩子保持精确比对 |
| 修法二（失败降级，关键） | 新增 `Hooking::SetIllusionDisplayEnabled(bool)` / `IllusionDisplayEnabled()`（`std::atomic_bool`，默认 false，fail-closed）。`ApplyPlayerSkinningHooks()` 装完后按「`DontVanillaSkinPlayer` 与 `CustomSkinPlayer` 是否都装上」置位；缺一个就保持关闭，`ShouldOverrideSkinning()` 恒 false → 原版皮肤流程原样执行，玩家看到**真实护甲**，日志一行 `critical` 说明幻化本局关闭、其它功能不受影响。`Custom()` 只被该钩子调用（已 grep 确认无第二条应用路径），所以门住这一处即可 |
| 修法三（防错钩崩） | `ShouldOverrideSkinning()` 在 `skyrim_cast` 前加 `IsReadableObject()`：`VirtualQuery` 查 `MEM_COMMIT`、排除 `PAGE_NOACCESS`/`PAGE_GUARD`、8 字节对齐、区间够 `0x20`。自定位若挑中不合适的调用点，这里挡掉并 warn，而不是崩在 RTTI 上 |
| 修法四（诊断） | 装钩子前先打 `Skinning hooks: SkyrimSE.exe <版本>, <字节> bytes, written <本地时间>, base 0x…, .text rva 0x… + 0x…`；失败时额外打站点 `±0x10` 十六进制窗口、全 `.text` 到期望函数的调用点总数与最近 5 个（rva + 距离）。下一份日志就能判断是偏移漂移（自定位会自己修好）、exe 被改过（版本/大小/时间对不上）还是 Address Library 不对 |
| 不受影响 | 1.5.20 的 `.tri` 目标与 morph 事件驱动重放原样保留；幻化关闭时它们自然不工作（没有幻化可跟随） |

### 未验证

本机无 Skyrim，未实机验证。用户环境能否自定位成功，取决于那份 exe 附近是否存在唯一一条到 `ExecuteVisitorOnWorn` 的调用：日志出现 `hooking the relocated site` 或 `.text holds exactly one call … hooking that one` 即成功。无论成功与否都不会再裸体 —— 最差是列出候选、`Illusion display is off …`、显示真实护甲。按 `docs/TESTING.md` §3.34 复测。

## 4.51 1.5.22 钩子站点由函数体扫描决定：不再有任何每运行时字节偏移

需求（m01170）：「依旧幻化失败。。这项目是要兼容AE以及最新版本的，让你在CommonLibSSE-NG基础上改不是让你专门去钩1597」。1.5.21 的「偏移对不上就在 ±0x800 内找替身」只解决了这一次，仍是围绕一个写死偏移做补救；本版把站点定位整个换掉。

| 项 | 内容 |
| --- | --- |
| 症状 | 1.5.21 装上后依旧没有幻化。若日志是 `Illusion display is off for this session: … (vanilla pass hooked, display pass missing)`，就是 `CustomSkinPlayer` 的壳钩子没装上、幻化被总开关关掉（显示真实护甲、不裸体）；装了但站点错则会贴错或贴不上 |
| 为什么写死偏移不可能跨版本 | 那些 `+0x302` / `+0x7C` / `+0x81` / `+0x97` / `+0x52` 全部抄自上游 1.5.73 的注释。地址库只保证**函数入口**；函数体内部布局随编译器/内联变化。实测：`24231` 在 1.5.97 入口 rva `0x364280`，而下一条地址库条目只比它大 `~0xD0`，`+0x81` 落在垃圾字节上（`E8 E2 CD C7 FF` 解出的目标在模块外）。⇒ 站点必须运行期在函数体内找 |
| 修法一（地址库定函数） | 新增 `Hooking::FindCallInFunction(hook, hostId, targetId)`：`REL::RelocationID(seID, aeID)` 给出宿主函数入口，目标函数地址同样由 ID 给出（`TESObjectARMO::ApplyArmorAddon` / `InventoryChanges::GetWornMask` / `InventoryChanges::ExecuteVisitorOnWorn` / `BGSBipedObjectForm::TestBodyPartByIndex` / `BipedAnim` 的两个会员函数）。在 `[entry, entry + kBodyScanLimit)`（`0x800`，已知最深站点 `0x373`）逐字节扫 `E8`，`rel32` 精确等于目标（或经一跳 thunk 到达）即候选。`SiteOffset()` 已删除，代码里**没有**任何 `+0x81` 之类的常量 |
| 修法二（候选处理） | 1 条命中 → 采用，debug 打 `the call to 0x… sits at rva 0x… (N byte(s) into the function at rva 0x…)`；多条 → 钩第一条并 warn `calls 0x… N times; hooking the first one`（其余可能属于相邻函数，寄存器语义无法保证）；0 条 → error `does not call 0x… within 0x800 bytes` + 模块身份 + 全 `.text` 调用数与前 5 个 `call at rva`，该钩子放弃（不让游戏崩） |
| 修法三（去掉寄存器假设） | `ShimWornFlags` 与 `CustomSkinPlayer` 的壳代码只保存/恢复 `rcx`（成员调用点它必是 `InventoryChanges*`），C++ 侧 `inventory->owner` 取 actor；`ShimWornFlags` 在无幻化时直接 `return a_inventory->GetWornMask()`（原版掩码，不再返回 0）。`DontVanillaSkinPlayer`（actor 在 `r13`）与 `FixEquipConflictCheck`（`rdi`/`rbx`）保持不变 —— 那两处是调用约定里的固定寄存器，不随版本漂移 |
| 修法四（钩不到也能贴件） | `CustomSkinPlayer` 在该构建里没有可钩调用点时，`DontVanillaSkinPlayer::ShouldOverride` 每次要藏真甲就 `MarkAttachPending(actor)`：按 FormID 100 ms 节流入队，`SKSE::GetTaskInterface()->AddTask` 在主线程 `DrainAttachPending()` → `Custom(actor, nullptr)`；`Custom` 的 model 为 null 时对 `{false, true}` 两套 biped 各调一次 `CustomOne`，指针用 `&actor->GetBiped(fp)`（与引擎传进来的 `BSTSmartPointer<BipedAnim>` 持有者地址相同）。壳钩子装上时走快路径、不进队列 |
| 修法五（门控放宽） | 幻化总开关只要求「藏真甲」的 `DontVanillaSkinPlayer` 装上；`CustomSkinPlayer` 缺失不再等于幻化关闭。三种日志：`Illusion display is active: the player skinning hooks are installed.`（快路径）/ `…the hiding hook is installed and the display set is attached from the deferred pass.`（延迟路径，功能正常）/ critical off（藏真甲的钩子都没装上，显示真实护甲） |
| 兼容性 | 仍是同一份 `Hooks.cpp` + `REL::RelocationID(seID, aeID)` 单表；`REL::Module::IsAE()` 只出现在「注入机器码本身不同」的地方（`DontVanillaSkinPlayer` 取 actor 的寄存器、`FixSkillLeveling` 的 AE 内联分支）。SE 1.5.97 / AE 1.6.1130–1.6.1179 / 1.7.x 走同一条代码路径，没有第二份实现、没有第二张 ID 表 |
| 不受影响 | 幻化数据流（`compute_display_set_c` / `ClearSlots` / 诚实还原）、`.tri` morph 跟随、PAD / AND 兼容、菜单与 Papyrus 接口均未改 |

### 未验证

本机无 Skyrim，未实机验证。按代码推断：用户 1.5.97 上 `DontVanillaSkinPlayer`（`0x364652`）、`ShimWornFlags`（`0x362F0C`）、`FixEquipConflictCheck`（`0x60CAC7`）、`FixSkillLeveling`（`0x624012`）都在各自入口 `0x800` 内、目标精确，扫描必中；`CustomSkinPlayer`（入口 `0x364280`、函数体约 `0xD0`）大概率扫不到 → 走延迟附加，幻化恢复。即使扫到非预期站点也不会崩：壳代码只用 `rcx` 且保存/恢复 `rdx`。要看的日志：`the call to 0x… sits at rva`（四个）、`Illusion display is active: …`（任一分支）、`CustomSkinPlayer: the display set is attached from inside the skinning routine.` 或 `…from the deferred pass…`。按 `docs/TESTING.md` §3.35 复测。

## 4.52 1.5.23：恢复 1.5.17 的贴件站点（幻化不再全裸，也不再乱套）

### 症状

- 用户 m01837：「1.5.17 都测过了，全部功能正常，只有 BodyMorphs 实时形变的问题。哪来的 1.5.17 能不能在机器上恢复显示？1.5.20 版本后你底层的幻化都乱套了。」
- 1.5.20 装上后角色全裸；1.5.21 改成 fail-closed（幻化整体关闭，显示真实护甲）；1.5.22 改成「地址库定函数 + 函数体内扫 `E8`」，扫不到仍拒绝安装 ⇒ 依旧没有幻化。

### 真因：1.5.20 新增的目标校验把 1.5.17 一直可用的站点判死

- 1.5.20 的日志已经把答案写在脸上：`CustomSkinPlayer: the call at +0x81 targets 0x7FF66C1610E8, but 0x7FF66C3651D0 was expected; the hook stays disabled so the game keeps running.`（`src/cpp/hooking/HookSupport.cpp`）。
- 1.5.20 之前的 `ResolveSite` 只看站点首字节是不是 `E8` / `E9` / `90`（见 1.5.17 的 `docs/HOOKS.md` §1.2），不校验 `E8` 的 `rel32` 指向哪里。1.5.17 在用户这台 1.5.97 上正是这样装上去的，而且**全功能正常** ⇒ 该站点的调用语义（成员调用，`rcx` = `InventoryChanges*`）在新运行时仍然成立，被否掉的只是「目标必须是 `ExecuteVisitorOnWorn`」这个过强的假设。
- 1.5.21 的 ±0x800 自定位与 1.5.22 的纯扫描都在同一个错误前提上继续加码：把「站点目标不同」当成「站点不存在」。

### 修法

1. **新增 `Hooking::FindCallInFunctionOrOffset(hook, hostId, targetId, legacyOffset)`**（`HookSupport.h/.cpp`）：先走 1.5.22 的扫描（宿主函数入口起 `0x800` 内找 `E8` 且目标精确命中，或经一跳 thunk 到达）；**扫不到时**回退到 `entry + legacyOffset`，且必须同时满足「在 `.text` 内」「该处是 `E8` 开头的 5 字节 call」才安装，并 warn：`{}: the function at rva 0x{:X} has no call to 0x{:X} within 0x{:X} bytes, but the site this hook has used since 1.0.0 - rva 0x{:X} (+0x{:X} into the function) - is still a 5-byte call; the hook is installed there.`；若历史站点连 `E8` 都不是，保持 1.5.22 的 error（打印模块身份 + 全 `.text` 调用统计 + 前 5 个候选 rva），该钩子放弃、不 patch、不崩。
2. **只给 `CustomSkinPlayer` 传历史偏移**：`const std::ptrdiff_t kLegacyOffset = REL::Module::IsAE() ? 0x1EF : 0x81;`（1.0.0 起就是这个值，1.5.17 在 1.5.97 上实测可用）。其余四个钩子（`DontVanillaSkinPlayer` / `ShimWornFlags` / `FixEquipConflictCheck` / `FixSkillLeveling`）仍用纯扫描 `FindCallInFunction`，不引入任何偏移。
3. **门禁恢复成对判定**：`illusionAvailable = g_dontVanillaSkinPlayerInstalled && g_customSkinPlayerInstalled`。只装上「藏真甲」一半时整条幻化关闭（`SetIllusionDisplayEnabled(false)`，原版流程照跑，显示真实护甲而不是裸体），日志 critical `Illusion display is off for this session: …`；两半都在时才 info `Illusion display is active: the player skinning hooks are installed.`（1.5.22 那条「display set is attached from the deferred pass」的 info 已删除——实机证明只有藏真甲时会看到裸体）。

### 兼容性

- 仍是**一份实现 + 一张 `REL::RelocationID(seID, aeID)` 表**（1.0.1 起的架构约束），没有第二套钩子实现、没有第二张 ID 表；`REL::Module::IsAE()` 只出现在「注入机器码本身不同」的两处（`DontVanillaSkinPlayer` 取 actor 的寄存器、`FixSkillLeveling` 的 AE 内联分支）与这里选历史偏移。
- 新区（AE 1.6.1170+ / 1.7.x）：若函数体内能扫到那条 call，走扫描，历史偏移完全不参与；只有扫不到时才用 1.0.0 起的历史站点，且仍要求那里是 5 字节 `E8`。
- 不动幻化数据流、`.tri` morph 跟随（1.5.20 起的 MorphPush / `MorphShapeCallback` 订阅）、PAD / AND 兼容、菜单与 Papyrus 接口。

### 未验证

本机无 Skyrim，未实机验证。实机要确认两行日志：`CustomSkinPlayer: the function at rva 0x… has no call to 0x… within 0x800 bytes, but the site this hook has used since 1.0.0 - rva 0x364301 (+0x81 into the function) - is still a 5-byte call; the hook is installed there.` 与 `Illusion display is active: the player skinning hooks are installed.`。若看到的是 `… and the legacy site rva 0x… is not a 5-byte call either`，把那几行发回，按真实反汇编重新定位。按 `docs/TESTING.md` §3.36 复测。

## 4.53 1.5.24：贴件必须用引擎传进来的 ActorWeightModel（只显示头手 + 46 槽外露）

### 症状

1.5.23 日志里五个钩子全部装上、幻化也确实在跑：

```
Custom: rebuilding 5 display pieces (mask 0x00002186, outfit revision true, body morph false) for 00000014.
Custom: FE2D6D6B slot 01 clone 0x0
Custom: FE2D6D6D slot 02 clone 0x0
Custom: FE2D6D76 slot 07 clone 0x0
Custom: FE2D6D72 slot 08 clone 0x0
Custom: FE2D6D6C slot 13 clone 0x0
```

五件全部 `clone 0x0` —— 一件都没贴上，人物只剩头、手（那是原版皮肤部件，不走护甲通道）。切换幻化时还会看到 `Custom: 1 slot(s) left the display set (mask 0x00010000); restored 1 real piece(s)`，即原装备的「槽 46」被重新贴了回来。

### 真因一：贴件用的是假的 ActorWeightModel

1.5.20 起把贴件入口改成只传 `InventoryChanges*`（`CustomFromInventory`），模型在 `Custom(actor, nullptr)` 里用

```cpp
auto* holder = const_cast<RE::BSTSmartPointer<RE::BipedAnim>*>(&actor->GetBiped(firstPerson));
CustomOne(actor, reinterpret_cast<RE::ActorWeightModel*>(holder));
```

强转而来。`RE::ActorWeightModel` 在 CommonLibSSE-NG 里**只有前向声明**，真实布局未知；`ApplyArmorAddon(armor, race, model, female)` 拿到这样一个指针，读到的是错位字段，于是每一件都建不出克隆 —— 日志里就是 `clone 0x0`。

1.5.17 之所以能用，是因为它直接用引擎传进来的寄存器（上游 `Hooks_PRE_AE.cpp:152-197`）：`rbx` = 正在蒙皮的对象（`TESObjectREFR*`），`rdi` = 引擎刚刚填好的 `ActorWeightModel*`。这两个寄存器就是该站点的契约。

### 真因二：先清空再重贴

1.5.20 起 `fullRebuild`（套装 revision 变化或身形变化）会先 `ClearSlots(actorWeightModel, displayMask)` 再把整份 displaySet 重贴一遍，包括那些本来已经由**原版通道**贴好的真实装备件。清空重贴在 1.5.14 就被证伪过（48 条 `clone 0x0`），加上模型指针本来就是错的，等于每次刷新都把已经好的槽也清空。

同一段代码在旧槽离开 displaySet 时还会「还原真甲」——把真实护甲重新 `ApplyArmorAddon` 贴回去，这就是用户看到的「46 槽露出来」（`0x10000` = 内部位 16 = CK 槽 46）。

### 修法

1. **壳代码回到 1.5.17 的寄存器契约**（`Hooks.cpp`）：
   `push rcx/rdx` → `call ExecuteVisitorOnWorn`（保持其它 mod 的共钩行为）→ `mov rcx, rbx` → `call ShouldOverrideSkinning` → `mov rcx, rbx; mov rdx, rdi` → `call Custom` → 恢复 `rcx/rdx` → `jmp site+5`。不再从 inventory 猜模型。
2. **只贴「未真实穿戴」的件**（与上游一致）：displaySet 里真实穿着的件由原版通道贴（`DontVanillaSkinPlayer` 对 displaySet 内的件放行），我们不重复贴。
3. **不再 `ClearSlots(displayMask)`**：切套装时槽里的件变了，`ApplyArmorAddon` 自己会重建；同款件保持不变。
4. **离开 displaySet 的槽只清空、不还原真甲**：幻化接管整个身体，套装没覆盖的槽就该露皮肤。
5. `ClearSlots`/`SlotClone` 需要 `BipedAnim*`，新增 `ModelBiped(actor, model)`：读模型第一个字段，并**只接受它是该 actor 两套 biped 之一**，否则 warn 一次并跳过（不拿未知内存开刀）。
6. `ApplyArmorAddon` 返回 false 时打 5 秒节流的 warn（`ApplyArmorAddon refused …`），不再静默留空槽。

### 新的日志

| 行 | 含义 |
| --- | --- |
| `Custom: attaching N outfit-only piece(s) of M displayed (mask 0x…, outfit revision …, body morph …) for …` | 每次贴件的开启行，N 是我们要贴的件数 |
| `Custom: … slot NN clone 0x…` | 该槽克隆节点。**非 0 才算贴上**；仍是 0x0 说明 `ApplyArmorAddon` 拒绝 |
| `Custom: ApplyArmorAddon refused …` | 引擎拒绝为这件建 addon，该槽会空着 |
| `Custom: N slot(s) left the display set (mask 0x…); cleared for …` | 1.5.24 起只清空、不再贴回真甲（1.5.25 删除了该清空，行改成 `left to the engine`） |
| `Custom: the weight model 0x… does not point at a biped model of …` | `ModelBiped` 校验没通过，跳过（只打一次） |

### 兼容性

- 钩子仍是单套实现、单张 `REL::RelocationID(seID, aeID)` 表；`CustomSkinPlayer` 的站点来源不变（先扫函数体，扫不到回退 1.0.0 历史站点）。
- SE 与 AE 的壳代码完全一样：两边的该调用点都以 `rbx`/`rdi` 传参（上游 1.5.73 的 SE/AE 两份实现用的是同一组寄存器）。

### 未验证

本机没有 Skyrim。判定标准只有一条：日志里 `Custom: … slot NN clone 0x…` 必须是非 0 指针；若还是 `clone 0x0` 并伴随 `ApplyArmorAddon refused`，把这两行发回。

## 4.54 1.5.25：身体槽消失——离开幻化集的槽不能清空

### 症状

1.5.24 之后幻化件确实贴上了（日志有 `Custom: attaching N outfit-only piece(s) of M displayed`），但**身体没了**：

- 人物只剩头和手（那是原版皮肤部件，不走护甲通道）；
- 在物品栏里把 32 槽（身体）的护甲脱掉，身体仍是空模型；
- 切换到空的幻化套装，身体也回不来。

日志（`D:/Skyrim-dev/bug/SkyrimOutfitSystemNG.log`，v1-5-24-0）在 14:42:11 出现：

```
Custom: 1 slot(s) left the display set (mask 0x00000004); cleared for 00000014.
```

`mask 0x4` = 内部位 2 = CK 槽 32 = 身体。同一份日志里没有一条 `ApplyArmorAddon refused` —— 贴件本身没失败，是**清空**把身体拆掉了。

### 真因

1.5.24 的 `CustomOne` 用按模型指针缓存的 `TakeStaleSlots` 记住「上一次在幻化集、这一次不在」的槽，然后对它们调用 `ClearSlots(actor, actorWeightModel, staleMask)`：

```cpp
RE::BipedAnimAugments::ClearBipedPart(anim, &anim->objects[bit], true, 0);
anim->objects[bit].item  = nullptr;
anim->objects[bit].addon = nullptr;
anim->objects[bit].part  = nullptr;
```

`BipedAnim::objects[bit]` 不只是「我们贴上去的那个克隆」，它是引擎给这个槽维护的**唯一**条目：里面还有引擎自己挂的皮肤/身体部件、`addon`（ARMA 解析结果）与 `part`（几何）。三个字段一齐清空后，只要装备栏本身没有变化，引擎就再也没有理由重走一遍 `ApplyArmorAddon`，槽便永久空着 —— 所以脱掉 32 槽护甲、换空套装都救不回来。

对照上游 1.5.73（`D:/Skyrim-dev/src/SkyrimOutfitSystemSE/src/cpp/hooking/Patches.cpp`）：`CustomSkinPlayer::Custom` 只做「displaySet 去掉 `visitor.equipped`，逐个 `ApplyArmorAddon`」，**从不清空任何槽、也从不还原真甲**。这也正是 1.5.17 在用户机器上一切正常、唯独 `.tri` 不跟随的原因。1.5.24 为了修「46 槽外露」引入的清空策略，等于用更大的破坏换一个小问题。

### 修法

1. **删除 `ClearSlots` 与 `SlotClone`**（`Patches.cpp`）：该文件已无任何 `ClearBipedPart` 调用。离开幻化集的槽不再清空 —— 槽里还留着上一套的克隆，但换装/脱装会让引擎重贴该槽，用真实装备覆盖；即使不覆盖，看到的也是上一套的幻化件，而不是空模型。
2. **残留槽只记录**：`TakeStaleSlots` 保留，只用来打一行 `Custom: {} slot(s) left the display set (mask 0x{:08X}); left to the engine for {:08X}.`
3. **贴件后补状态探针**：新增 `LogSlotState(actor, model, armor, bit)`，对每个刚 `ApplyArmorAddon` 过的槽打 debug 行 `Custom: {:08X} slot {:02} item {:08X} addon {:08X} part {} clone {}`。`addon 0` = ARMA 没解析出来；`part true` = 几何已建。这是给「贴上了但看不见」用的判定行（`clone 0x0` 本身不能当失败证据：`partClone` 由引擎在 `ApplyArmorAddon` 返回之后才填）。
4. **`ModelBiped` 保留**：`LogSlotState` 需要从 `ActorWeightModel` 拿到 `BipedAnim*`，仍然只接受该 actor 的两套 biped 之一，否则 warn 一次并跳过。

### 新的日志

| 行 | 含义 |
| --- | --- |
| `Custom: N slot(s) left the display set (mask 0x…); left to the engine for …` | 1.5.25 起只是告知：这些槽不再由幻化接管，交回引擎；不会再清空（1.5.24 的 `cleared for …` 已删除） |
| `Custom: … slot NN item … addon … part … clone …` | 贴件后的槽状态（debug）：`addon 0` = ARMA 未解析；`part true` = 几何已建 |

### 兼容性

- 钩子层完全没动：仍是单套实现、单张 `REL::RelocationID(seID, aeID)` 表，`CustomSkinPlayer` 仍「先扫函数体、扫不到回退 1.0.0 历史站点」。
- 与 1.5.24 相比只少了「清空」这一个动作，1.5.24 修好的两点（`rbx`/`rdi` 取模型、只贴未穿戴件）保持原样。

### 未验证

本机没有 Skyrim。判定标准：切套装 / 脱 32 槽护甲 / 切空套装后，身体始终是原版身体，日志里只出现 `left to the engine`、不再出现 `cleared for`。若仍空，把 `Custom: … slot NN item … addon … part … clone …` 行发回。

## 4.55 1.5.26：幻化件跟随身形——不再等 RaceMenu 的接口

1.5.25 把基础幻化彻底修好了（用户确认幻化服装与空幻化都正常），剩下的唯一问题回到最初那一条：**RaceMenu 拖 Body Morphs 滑块 / OBody 换预设 / SLIF 膨胀时，幻化件的身形不动**（m00002 / m00270 / m02536）。

#### 为什么 1.5.20 到 1.5.25 都没修好

这几版把「先拿到 skee 的 `BodyMorph` 接口」当成前提：

```cpp
BakeResult BakeIllusion(RE::Actor* a_actor, BakeReason a_reason)
{
    if (!g_api || !a_actor || !a_actor->Is3DLoaded()) {   // 没有接口就整段跳过
        return BakeResult::kSkipped;
    }
    ...
```

`NotifySlotApplied()` 同样在 `!g_api` 时直接 return。于是接口拿不到 ⇒ 幻化克隆件上一个 `BODYTRI` 都没有 ⇒ skee 的 morph 遍历整棵子树都找不到目标 ⇒ 拖滑块时幻化件纹丝不动。1.5.20 写好的 `AttachBodyTri()` 与 `.tri` 路径推导一直都在，只是被这个门禁挡在门外。

#### 证据一：接口为什么拿不到（skse64.log）

- 我们的 `0x9E3779B9` 广播在 skse64.log 里**恒投给 24 / 35 / 39 / 45 / 66 / 91 / 95(OBody) / 140** 这 8 个插件，从不包含 132(skee) 与 99(OStim)；14 次尝试 14 次一模一样。
- 这 8 个恰好是「在 `kPostLoad` 投递循环**中间**才注册 catch-all 监听者」的插件（`registering plugin listener for (null) at 24 of …` 出现在 `sending message type 0 to plugin 24` 之后）；skee 与 OStim 是**加载期**就注册的。
- 连定时器任务里发起的、日志末尾外面没有任何 dispatch 在跑的那一次，收件集仍是同样 8 个 ⇒ 与线程、嵌套无关，是**监听者的注册时机**。
- 对照：别的插件的广播（type 1 以及几个自定义 type）收件集里 99 与 132 都在，说明 skee 的监听者本身正常，只是轮不到我们的消息。

结论：在这套加载顺序下不能把「拿到接口」当作功能前提。

#### 证据二：`.tri` 跟随本来不需要接口

skee 的 `BodyMorphInterface.cpp`：

- `Impl_ApplyBodyMorphs` → `MorphCache::UpdateMorphs(refr)`（718–741 行）用 `VisitBipedNodes` 遍历每个 biped 槽的 `data.object`，逐个调 `ApplyMorphs()`；
- `ApplyMorphs`（690–716 行）在该子树里找名字为 `BODYTRI` 的 `NiStringExtraData`，找到就 `CacheFile` 并应用。

RaceMenu 拖滑块时调的正是 `ApplyBodyMorphs`，OBody 换预设也走同一条。所以：**只要克隆件带 `BODYTRI`，RaceMenu 自己就会 morph 它**，不需要我们持有任何接口。（第一次被 morph 时若 `geometry` 上还没有 `SHAPEDATA`（未变形基准顶点），skee 会拿当前顶点自动建一份 `NiBinaryExtraData` 挂上去，见 `ApplyMorph` 541–547 行，所以基准也不需要我们造。）

#### 修法

1. **`BakeIllusion()` 去掉 `!g_api` 门禁**：无接口也推导 `.tri` 路径并 `AttachBodyTri()`，只有 `ApplyVertexDiff()` 留在 `if (g_api)` 里。返回 `kBaked` 的条件改成「贴了目标 或 重放了 morph」。
2. **`NotifySlotApplied()` 不再因 `!g_api` 提前返回**：皮肤钩子每次贴完件都排队一次 bake，克隆件因此总能拿到 `.tri` 目标。
3. **新增证据日志**（每个 actor 一次，info）：`MorphRefresh: <FormID> carries a .tri morph target on N of M cloned slot(s) (…); RaceMenu and OBody morph those clones in their own apply pass - no interface needed.`
4. **握手改到主线程任务**：新增 `QueueProviderRetry()`（`std::atomic<bool>` 去重 + `SKSE::GetTaskInterface()->AddTask`），`Install()` 与 `EnsureProvidersForUse()` 都改走它。后者原来在皮肤钩子里**同步** Dispatch —— 1.5.25 日志里那两条 `Failed to dispatch message to skee64` / `to skee` 就夹在 `ShouldOverrideSkinning` 中间。接口仍是加分项：拿到就能立刻 `ApplyVertexDiff` 重放，并订阅 skee v5 的 `MorphShapeCallback`。
5. **几何探测补上蒙皮网格**：新增 `HashSkinnedShape()`，读 `BSGeometry::skinInstance->skinPartition->partitions[0].buffData->rawVertexData`（绑定姿势顶点，动画不影响哈希），接在 `HashDynamicShape()` 之后。1.5.18 起只采样 `BSDynamicTriShape`，而身体和护甲都是 `BSTriShape`，所以 250 ms 轮询在拖滑块时从不触发。

#### 新的日志

| 行 | 含义 |
| --- | --- |
| `MorphRefresh: <FormID> carries a .tri morph target on N of M cloned slot(s) (…); … no interface needed.`（info，每角色一次） | 幻化克隆件已带上 `.tri` 目标，RaceMenu/OBody 自己的 morph 遍历能走到它们。`N < M` 说明有幻化件推导不到 `.tri`（模型名不符合「去掉 `_0`/`_1` + `.tri`」的约定） |
| `MorphRefresh: .tri morph targets are attached to every illusion clone; … The provider interface only adds the immediate re-morph.`（info，kDataLoaded） | 1.5.26 起这行取代 1.5.20 的 `.tri morph target support active/inactive` |
| `MorphRefresh: … has no BodyMorph interface; N .tri target(s) of M slot(s) stay armed for RaceMenu's own pass (…)`（debug） | 没有接口但目标已就位 —— 正常状态，不是错误 |
| `MorphRefresh: re-morphed … on N node(s), M .tri target(s) of K slot(s) (…); the vertices moved.`（debug） | 有接口时的即时重放证据 |

#### 兼容性

- 钩子层完全没动：仍是单套实现、单张 `REL::RelocationID(seID, aeID)` 表，`CustomSkinPlayer` 仍「先扫函数体、扫不到回退 1.0.0 历史站点」。
- 只读 skee 的公开契约（`BODYTRI` 这个 extra data 名字来自 skee 源码里的字面量，`.tri` 命名规则来自 OBody 的 `CacheSkinMorphFiles`），不 hook skee、不写它的内存，skee 换实现也不影响。
- 没装 RaceMenu 时同样成立：OBody 换预设会自己调 `ApplyBodyMorphs`。
- 没有 `.tri` 的护甲依旧不跟随（这是 BodySlide 生成物，不是我们能补的）。

#### 未验证

本机没有 Skyrim。判定：幻化一套带 `.tri` 的衣服，开 RaceMenu 拖体重滑块 → 幻化件应实时跟着变，中途不闪原版护甲。日志先有 `carries a .tri morph target on N of M cloned slot(s)`，开 Verbose 后每 5 秒最多一条 `stay armed …` 的 debug。若拖滑块仍不动，把 `N` / `M` 与实际护甲模型路径发回。

## 4.56 1.5.27：一个 `.tri` 目标都没贴上——模型路径改问引擎、候选名全试、贴上了就不再摘幻化

1.5.26 交付后用户实测（m02992）：「跟上轮一样幻化外观依旧无法跟随RaceMenu的BodyMorphs实时形变」。日志 `bug/SkyrimOutfitSystemNG.log`（432,709 B / 5,046 行 / v1-5-26-0 / runtime 1-5-97-0）里，1.5.26 新加的那条诊断把问题钉死了：

```
MorphRefresh.cpp:1212  MorphRefresh: 00000014 has no BodyMorph interface; 0 .tri target(s) of 5 slot(s) stay armed for RaceMenu's own pass (the illusion was rebuilt).   ← 5 次
MorphRefresh.cpp:1212  MorphRefresh: 00000014 has no BodyMorph interface; 0 .tri target(s) of 6 slot(s) stay armed … (the provider wrote new morphs).               ← 1 次
```

一整局**一个 `BODYTRI` 都没贴上**。1.5.26 的整个思路是「把 `.tri` 目标挂到克隆件上，让 RaceMenu / OBody 自己的 morph 遍历去写它」；目标数为 0 就等于什么都没做 —— `carries a .tri morph target on N of M cloned slot(s)` 与 `re-morphed …` 在整份日志里 0 条。同一份日志里的 5 条 `the provider pass left the meshes of 00000014 alone; taking the illusion off for one engine pass.` 就是这条死路的代价：克隆件没有 morph 目标，provider 写不动它，只能摘幻化再穿回（会闪原版）。

### 真因：`.tri` 路径是从一个不可靠的 ARMA 查询推出来的

1.5.20–1.5.26 的 `BodyTriPathFor()` 先 `armor->GetArmorAddon(race)` 再取 `bipedModels[sex].GetModel()`：

```cpp
if (const auto* addon = a_armor->GetArmorAddon(a_race)) {          // 自定义种族 / 套装没列该种族 ⇒ nullptr
    path = DeriveTriPath(addon->bipedModels[a_female ? 1 : 0].GetModel());
}
```

`TESObjectARMO::GetArmorAddon` 只回答「这个 ARMA 的 `race` / `additionalRaces` 命不命中传入的种族」（`TESObjectARMA::IsValidRace`）。自定义种族、套装没列该种族、或引擎自己走了默认种族回退时它返回空 ⇒ 路径为空 ⇒ 连 `AttachBodyTri()` 都不调用。

而同一份日志的槽位转储说明引擎每件都解析出了 ARMA：

```
Custom: FE28F803 slot 01 item 1039FCD8 addon FE28F802 part 1 clone 0
Custom: FE28F801 slot 02 item 1039FCD5 addon FE28F800 part 1 clone 0
Custom: FE28F80B slot 07 item FE023800 addon FE28F80A part 1 clone 0
Custom: FE28F807 slot 16 item 3400504A addon FE28F806 part 1 clone 0
```

`addon` 全部非 0（`part 1` = 引擎解析出了模型；`clone 0` 只是 `ApplyArmorAddon` 返回那一瞬间 `partClone` 还没填，属于正常）。`biped->objects[i].addon` 就是引擎真正挂上去的那份 ARMA，没有理由再问一次 `GetArmorAddon`。

### 修法（五条）

1. **模型路径改问引擎**：`BodyTriPathFor(armor, addon, race, female)` 优先用 `biped->objects[i].addon`，`GetArmorAddon(race)` 只在没有 addon 时兜底；查不到 addon 时**不写缓存**（护甲穿上之后可能就有，空值缓存会把该槽永久判死）。
2. **候选名全试**：新增 `TriCandidates()`，一次生成 `<stem>.tri`（去掉 `_0`/`_1`）、`<stem>_0.tri`、`<stem>_1.tri`、原样 `<stem>.tri`，按「本性别优先」排序后逐个用 `BSResourceNiBinaryStream`（与 RaceMenu 的 `CacheFile` 同一问法）探测。1.5.26 在模型名不带 `_0`/`_1` 时只试一个名字。
3. **数目标时不再只数自己**：`NodeHasBodyTri(node)` 的计数移出「路径非空」分支 —— `.nif` 自带 `BODYTRI` 的件同样算武装，1.5.26 只数本模块贴上去的。
4. **每个 ARMA 一条诊断**（debug）：`… slot model <path> (from the armour addon the engine attached) uses .tri <file>.`，没有就是 `… has no .tri beside it, so RaceMenu cannot morph that slot. Probed: a, b, c, d.` —— 下一次日志能直接区分「路径推导错了」与「这套衣服真没有 `.tri`」。
5. **目标贴上了就不再摘幻化**：新增 `morphPush::g_armed`（每个角色是否武装）。`PollOnMainThread()` 与 `VerifyProviderPass()` 在「签名变了」时若已武装就只重设基线（`carries .tri targets; leaving the clones to the next morph pass instead of cycling`），不再跑 `CycleIllusion` —— 1.5.26 日志里那 5 次摘幻化正是从这里来的。

### 新的日志

| 行 | 含义 |
| --- | --- |
| `MorphRefresh: <armor> slot model <path> (from the armour addon the engine attached) uses .tri <file>.`（debug，每护甲/性别一次） | 该件的 `.tri` 目标来源与文件名。有这行说明克隆件会被 RaceMenu morph |
| `MorphRefresh: <armor> slot model <path> (from …) has no .tri beside it, so RaceMenu cannot morph that slot. Probed: a, b, c, d.`（debug） | 四个候选名都不存在。要么这套衣服不是 BodySlide 生成的，要么 `.tri` 在别的目录 / 名字完全不同 |
| `MorphRefresh: <armor> has no armour addon yet; no .tri target can be derived.`（debug） | 引擎还没给该槽解析 ARMA。每次重建都会再问一次（不缓存） |
| `MorphRefresh: the illusion of <FormID> carries .tri targets; leaving the clones to the next morph pass instead of cycling.`（debug） | 已武装：签名变了但不再摘幻化。这是 1.5.27 想要的状态 |
| `MorphRefresh: the provider pass left the meshes of <FormID> alone, but the illusion carries .tri targets; leaving it to the next morph pass.`（debug） | provider 没带动网格，但克隆件有目标，下一次 RaceMenu 的 pass 会写它 |

### 兼容性

- 钩子层与 1.5.26 完全一致（单套实现、单张 `REL::RelocationID(seID, aeID)` 表、`CustomSkinPlayer` 先扫函数体再回退历史站点）。
- 仍然只读 skee 的公开契约（`BODYTRI` 这个名字来自 skee 源码字面量，`.tri` 命名规则来自 OBody 的 `CacheSkinMorphFiles`）与引擎的 `BipedAnim::objects[].addon`；不 hook、不写任何第三方 mod 的内存，skee / OBody / SLIF 升级不影响。
- 没装 RaceMenu 时同样成立：OBody 换预设会自己调 `ApplyBodyMorphs`，遍历到的就是带 `BODYTRI` 的克隆件。

### 未验证

本机没有 Skyrim。判定：幻化一套带 `.tri` 的护甲，开 Verbose，切一次套装后看 `uses .tri <file>` 与该角色第一次的 `carries a .tri morph target on N of M cloned slot(s)` —— `N` 应等于幻化覆盖的槽数（例：0x00010086 的 4 个槽）。然后拖 Body Morphs 滑块 / 换 OBody 预设 / SLIF 膨胀，幻化件应实时跟着变，且日志里不再出现 `taking the illusion off for one engine pass`。若 `N` 仍为 0，把 `has no .tri beside it … Probed: …` 那几行发回，那里会直接列出试过的四个文件名与该件真实模型路径。

## 4.57 1.5.29：场景结束后幻化件残留与「要再按一次键才刷新」

### 症状（m05502，两组 7f5a2ce 日志）

- A 组：OStim 场景中 `_sex` 幻化没显示（看到裸体），场景结束切回普通幻化时闪退。
- B 组：场景中按 Tab/Z 打开快速换装窗后 `_sex` 幻化生效；场景结束切回普通幻化**没有**闪退，但模型仍是旧的一套，需要**再按一次** Tab/Z 让窗口重刷才正确。

### 日志证据

- `SkyrimOutfitSystemNG-TAB.log`：6112 打开快选窗 → 6119 `Armor Refresh for JueLunV6` → 6143 `Custom: attaching 5 outfit-only piece(s) of 5 displayed (mask 0x00002096, outfit revision true …)` → 6198 `OStim started … _sex` → 之后 **16.7–34.9 秒之间零条 CustomOne**（18 秒里没有任何蒙皮 pass，这段时间用户看到的自然是裸体或旧外观）→ 34.989 起跨线程贴件风暴 → 8961 又一次 `Armor Refresh` → 9041/9136 贴普通套装 `mask 0x00002186` → 9149 `SceneOutfits.cpp:137 … ended; restored`。
- 两套套装覆盖的槽不同：`_sex` 是 `0x00002096`（bit 0,1,2,4,7,13），普通套装是 `0x00002186`（bit 1,2,7,8,13）。**bit 4（CK 槽 34）只在 `_sex` 里有** —— 退出场景后，这个槽的幻化克隆没有任何人拆，而引擎也不会为它重建真甲，于是视觉一直停在 `_sex` 的那件，直到下一次 Armor Refresh。

### 真因

1.5.25 起「离开幻化集的槽一律只打 `left to the engine`、绝不清空」，这是为了避免再出现 1.5.24 的身体空模型（m02242）。但当时没有区分两种情况：**槽里原本是引擎的真实条目**（交回引擎正确），还是**槽里是我们上一套幻化的克隆**（交回引擎就没人拆了）。后一种情况在「引擎那边没有真甲可重建」（场景套装、幻化件本来就不是穿戴物）时就会残留。

### 修法（两条，都在主线程，均不引入框架分支）

1. **只拆自己的残留克隆**（`Patches.cpp`）。`g_appliedMasks`（uint32）升级为 `AppliedState { displayMask; ownedMask; }`，其中 `ownedMask` 记录「当我们贴进该槽的东西里，有多少件是**没有真实穿戴物打底**的」（即 `applySet` 中 `wornBySlot[bit] == nullptr` 的槽）。离开幻化集的槽只有在**四个条件同时成立**时才拆：
   - `previousOwned & flag`：这个克隆是我们上一轮自己放的；
   - `wornBySlot[bit] == nullptr`：现在这一轮没有真实穿戴物占用该槽；
   - `(skinMask & flag) == 0`：不是 `Actor::GetSkin()` 提供的皮肤/身体槽；
   - `anim && anim->objects[bit].partClone`：槽里确实还挂着克隆。

   满足则 `ClearBipedPart` 并打 `Custom: detached N illusion clone(s) whose slot left the display set (mask 0x…, …) for <FormID>.`；不满足仍打原来的 `left to the engine`。

2. **幻化几何对账**（`MorphRefresh.cpp` 主线程任务）。新增 `CustomSkinPlayer::IllusionMissingSlots(actor)`：只读，按已记录的 `displayMask` 检查每个幻化槽的 `objects[bit].partClone`（要求 `anim->root` 非空，避免在 3D 未就绪时误判），返回缺失槽掩码。`Tick` 的主线程任务里（在已有的玩家分支后面）若发现缺失，就调 `ReattachDroppedIllusion()` → 用**已经验收的** `::OutfitSystem::refreshArmorFor(actor)` 重建（同一条路径场景套装结束恢复也在用），并对同一角色做 300 ms 退避、最多 3 次；三次仍缺失就 warn 一次 `still holds no illusion geometry … after 3 rebuilds` 并放手，不再空转。

这条对账不针对任何 mod：OStim/SexLab 场景、OBody 换预设、手动换装、读档后只要出现「幻化集说该有、biped 槽里没有」，都会被它修一次。

### 新的日志

| 日志 | 含义 |
| --- | --- |
| `Custom: detached N illusion clone(s) whose slot left the display set (mask 0x…) for <FormID>.` | 窄清生效：这些槽里是我们上一套的克隆，已拆掉（debug） |
| `Custom: N slot(s) left the display set (mask 0x…); left to the engine for <FormID>.` | 这些槽交回引擎（槽里原本是引擎的真实条目，或没有克隆可拆） |
| `MorphRefresh: <FormID> lost the illusion in slot mask 0x…; rebuilding the actor's equipment.` | 对账发现幻化槽空了，正在用普通刷新重建（debug，同角色 300 ms 退避） |
| `MorphRefresh: <FormID> still holds no illusion geometry in slot mask 0x… after 3 rebuilds; it is left alone from now on.` | 三次重建仍空，多半是 `ApplyArmorAddon` 拒绝或没有可用的 ARMA（warn，每角色一次） |

### 兼容性

- 不改 `Hooks.cpp`、不改注入壳代码、不加任何字节偏移：1.5.23–1.5.25 的钩子行为与 1.5.25 已实测正常的基础幻化完全不变。
- 拆克隆只发生在「我们放的 + 无真实穿戴物 + 非皮肤槽」的槽上，引擎自己的条目一律不碰（1.5.24/1.5.25 的教训写成守卫测试 `tests/skinning_stale_slots.ps1`，`ClearBipedPart` 在 `Patches.cpp` 里只允许出现一次，且前后必须是这四个条件）。
- 对账与重建里不出现任何第三方 mod 名字（守卫测试会拦），因此换框架、升级框架都不影响。

### 未验证

本机没有 Skyrim。判定方式：复现 m05502 的 B 组 —— OStim 场景中用 Tab 切到 `_sex`，结束场景看是否**不需要再按键**就换回普通幻化；日志里应看到 `detached …` 或 `lost the illusion in slot mask … rebuilding` 之一，**不应**再需要第二次 Tab。A 组的崩溃（`EXCEPTION_ACCESS_VIOLATION` 于渲染阶段读空指针）与本版的关系仍未实证；若重装后仍崩，请连同 `crash-*.log` 与同一次会话的 `SkyrimOutfitSystemNG.log` 一起发回，本版新增的 `detached` / `rebuilding` 行可以判断是否是本插件在错误时机改了 biped。

## 4.60 1.5.32：场景切换只做「找对名字 → 切套装」（m07382 / m07424）

### 症状（1.5.31 实机）
- OStim 场景开始切 `_sex` 套装：模型不出现，按一次 Tab / Z 打开快速选择窗口才出现。
- 场景结束回普通幻化：同样必须按一次窗口才变回来。
- 用户描述得很具体：**切到 `_sex` 后头饰/假发是正常的**，只有身体/手/脚（身体槽）是空模型 —— 说明套装确实切了、`_sex` 套装里属于头发与发饰的槽都贴对了。
- 用户给的方向（m07424）：「你只用找对名字然后切换，切换套装不用管什么之前逻辑里头饰假发显示正常了直接不用那个了删了，不行你就参考按地点/战斗切换幻化套装这里的逻辑。」

### 日志证据（1.5.31，`D:/Skyrim-dev/bug/SkyrimOutfitSystemNG.log`）
- 19:20:35.403 场景开始前的 3P pass 完整：`Custom: attaching 5 outfit-only piece(s) of 5 displayed (mask 0x00002096, outfit revision true, body morph false)`，五个槽都是 `part 1`。
- 19:20:35.421 `SceneOutfits: OStim scene started; … -> …_sex`；随后 1.5.31 的三次随访全部执行：`repeating the equip pass … (follow-up 1 of 3)`、`asking the game to rebuild the model of 00000014 … (follow-up 2 of 3)` + `Model Rebuild for JueLunV6`、第三次 `Armor Refresh`。**模型仍不出现。**
- 19:20:44.056 起引擎自己连跑十几轮装备 pass（几十条 `attaching … (outfit revision false)`，没有对应的 `Armor Refresh` 行），用户随后看到模型。
- 19:20:48.781 `HairTintDiagnostics` dump：3P biped 的 slot 1/2/4/7/13 全部有 addon 与几何。⇒ 数据层（biped 条目 + 克隆几何）完全正确。
- 结论：问题不在「切换没执行」，也不在「装备 pass 跑得不够多」。用户在场景里的观感是「头饰假发正常、身体空」，与**引擎自己那一份 Skin 覆盖槽**（身体/手/脚）有关，而靠我们反复跑 pass 或重建 3D 都修不掉（1.5.30 / 1.5.31 两次实机已证）。

### 修法（1.5.32）
1. **删掉全部补跑与重建机件**：`MorphRefresh.cpp` 的 `kOutfitPassCount` / `kOutfitPassDelay` / `struct OutfitPassState` / `g_outfitPasses` / `RequestOutfitPassesInternal` / `DrainOutfitPasses` / 公开的 `RequestOutfitPasses` 与 `Reset()` 里的清表块全部删除；`OutfitSystem::rebuildActorModel()` 与其声明一并删除。`NGFeatures::RefreshActor()` 回到只调 `::OutfitSystem::refreshArmorFor(a_actor)`。
2. **场景套装只做切换**：`SceneOutfits.cpp` 的 StartScene / EndScene 一律调 `Features::AssignOutfit(player, name)` —— 就是「点套装列表」那条函数，与菜单 Wear、按地点/战斗切换走到同一处，不再自己拼 `set_outfit_c()`。
3. **参考按地点/战斗切换的时序**：新增 `OnGameThread()`，场景事件在游戏线程上**同步**执行（`AutoSwitch` 的 location/fast-travel sink 就是这么做的），非主线程事件才落到原有 `AddTask` 队列；`g_dispatchMutex` 改 `std::recursive_mutex`，避免同类事件同步重入死锁。
4. **名字查找放宽**：先试 `has_outfit("<当前套装>_sex")`，没有再遍历 `get_outfit_names(false)` 做**忽略大小写**匹配，命中就用真实大小写的名字；仍找不到才打 info `no … outfit for the … scene; create an outfit with exactly that name…`。`_sex` 套装里的所有槽位规则都由用户自己配置，代码里不再有任何槽位/时机特判。

### 新日志（判定用）
- `SceneOutfits: OStim scene started; 'A' -> 'A_sex'` / `… ended; 'A' restored` —— 与之前相同；区别是紧随其后的贴件由**同一个事件帧**内的 `AssignOutfit` 触发。
- 不再有 `repeating the equip pass for`、`follow-up N of 3`、`Model Rebuild for`、`asking the game to rebuild the model of`（这些字符串已从 DLL 中消失）。
- 找不到套装时：`SceneOutfits: no '…' outfit for the … scene; create an outfit with exactly that name to use it here.`

### 兼容性 / 未验证
- 没有改任何钩子、注入壳、贴件语义、槽位清理与 `.tri` 推导 ⇒ 1.5.25 实测正常的基础幻化、1.5.26–1.5.27 的 `.tri` 目标不受影响。
- **本机没有 Skyrim，本版未实机验证**。若场景开始后身体槽仍为空，则说明问题不在「切换路径」也不在「pass 次数」，需要在下一版从「引擎 Skin 槽与幻化克隆争同一槽」这个方向单独定位（日志里 `HairTintDiagnostics` 的 3P/1P 对比是关键证据）。
## 4.59 1.5.31：场景开始/结束都必须弹一次窗才显示（m07050）

### 症状（1.5.30 实机）
- OStim 场景开始切 `_sex` 套装：模型不出现；按一次 Tab / Z 打开快速选择窗口才出现。
- 场景结束回普通幻化：同样要按一次窗口才变回来（1.5.30 之前只有前者有这个问题）。
- 用户给的方向：「直接把原来 `_sex` 套装切换部分删了，直接按『点套装列表』原来走的那条函数走」。

### 日志证据（584,253 字节 / 6,129 行，v1-5-30-0，runtime 1-5-97-0，18:38:27 加载）
- 18:38:28.728 DLL 改名生效：`interface probe sender 101 receiver skee64 payload 8 delivered false map returned false.` 与 `receiver skee payload 8 delivered true map returned true.` → `RaceMenu BodyMorph v4 acquired through skee`。
- 18:40:22.114 `SceneOutfits: OStim scene started; '…简化' -> '…_sex'.`；1.5.30 的三次随访 22.250 / 22.672 / 23.588 全部按时执行，每次都 `Armor Refresh for JueLunV6` + `Custom: attaching 5 outfit-only piece(s) of 5 displayed (mask 0x00002096, outfit revision true…)`。**模型仍不可见。**
- 22.381 `HairTintDiagnostics` 全量 dump：3P biped 的 slot 1/2/4/7/13 **全都有真实几何**（`TFPZ`、`mesh_65shangyi1..5`、`(FE2D6D6E)[1]`、`Feet/mesh_65xiezi`、`mesh_69`）。⇒ 「biped 已贴好」≠「画面上可见」。
- 18:40:42.955 引擎自己又跑了 2+2 轮 pass（线程 29720 / 14624，`outfit revision false`），模型依旧不出来 ⇒ 纯装备 pass 不是解法。
- 18:40:47.193 打开快速选择窗口 → 47.228 `re-morphed … 7 node(s), 7 .tri target(s) of 7 slot(s)` → 47.236 `scene ended; '…简化' restored.` 恢复普通套装 0x00002186（随后 3 次随访也全部执行）。**模型这时才变回普通幻化。**
- 线程：所有 `Custom:` 行与 skinning pass 都在 29720，`MorphRefresh UI probe … game-thread= expected=29720` ⇒ 菜单路径与脚本路径同线程，差别不在线程。

### 真因
装备 pass 只把克隆写进 `BipedAnim::objects[slot]`，被绘制的那份 3D 模型仍是切换前建的；**开/关一次菜单会让引擎重建该模型**，这才是「一按就出来」的真正机制。1.5.30 的「补跑 3 次 `refreshArmorFor`」只是重复写 biped，从来没有触发模型重建。

### 修法（三条）
1. `OutfitSystem` 新增 `bool rebuildActorModel(RE::Actor*)`：`Is3DLoaded()` 守卫 + `LOG(debug, "Model Rebuild for {}")` + `actor->Update3DModel()`，失败返回 false（无 3D 时打 `Skipping model rebuild for {}: no 3D yet.`）。
2. `MorphRefresh` 的随访序列里，`kModelRebuildAttempt = 2`（第 2 次随访）之前调一次 `rebuildActorModel`，日志 `MorphRefresh: asking the game to rebuild the model of {:08X} so the illusion reaches the drawn 3D (follow-up {} of {}).`；其余两次仍是 `refreshArmorFor`。
3. `SceneOutfits` 的场景开始 / 场景结束**删掉**自己拼的 `set_outfit_c` + `Features::RefreshActor`，改调 `Features::AssignOutfit(player, name)`（= 点套装列表那条函数）；失败时 `warn` 并保持/回滚，不再静默。

### 新日志一览
| 日志（debug/warn） | 含义 |
| --- | --- |
| `Model Rebuild for <name>` | 已向引擎请求重建该角色的 3D 模型 |
| `Skipping model rebuild for <name>: no 3D yet.` | 3D 未加载，本轮跳过重建（下次随访会再试） |
| `MorphRefresh: asking the game to rebuild the model of <FormID> so the illusion reaches the drawn 3D (follow-up 2 of 3).` | 随访链第 2 步就是重建，模型应在这一步之后出现 |
| `SceneOutfits: '<outfit>' could not be assigned for the <source> scene; the current outfit is kept.` | 场景套装赋值失败（套装已删除等），幻化保持原样 |
| `SceneOutfits: '<outfit>' could not be restored after the <source> scene.` | 场景结束恢复失败，需要手动切回 |

### 兼容性
- 只增加一次 `Update3DModel()` 调用，并把场景入口换成已存在的公共函数；不动钩子、注入壳、贴件语义、槽清理（1.5.25 / 1.5.29 的行为保持不变）。
- 框架无关：`SceneOutfits` 的改动只是换成通用入口，函数体里没有 OStim / SexLab / SAM 之类名字。
- 重建模型会让该角色当帧重走一次 biped→3D，开销与打开一次菜单相当；只对发生切换的角色、每次切换最多一次。

### 未验证
本机没有 Skyrim。判定：复现 OStim 场景开始，**不要按 Tab**，看 `_sex` 是否直接出现；日志应有 `Model Rebuild for …` 与 `asking the game to rebuild the model of …(follow-up 2 of 3)`。场景结束同理。若重建之后仍不显示，把这几行连同前后 20 行发回。
## 4.58 1.5.30：场景里切 `_sex` 不显示、必须弹一次窗（m06712）+ DLL 改名（m06781）

### 症状
- OStim 场景开始时 `_sex` 套装不生效（角色还是裸的或上一套），场景结束回普通幻化也不更新。
- 按一次 Tab / Z 打开快速换装窗口后立刻正常；用户提出「直接把点套装列表切换的逻辑拿过来用」。

### 日志证据（1,767,358 字节 / 18,260 行，v1-5-29-0，runtime 1-5-97-0）
- 17:51:50.339 `Armor Refresh for JueLunV6` → `Custom: attaching 5 outfit-only piece(s) of 5 displayed (mask 0x00002096, outfit revision true, body morph false)` → 五条槽位转储 → 17:51:50.357 `SceneOutfits: … scene started; … -> …_sex`。
- 之后 16.5 秒内零钩子调用。按秒统计 `CustomOne` 次数：17:51:46 = 2、17:51:50 = 2、**17:52:06 = 2、17:52:07 = 28**、17:52:11 = 2，其余全 0 —— 用户在 17:52:06.9 按下 Tab，引擎紧接着跑了 14 轮 pass，模型才出现。
- 全日志 `clone 1` 0 条、`detached` 0 条、`lost the illusion` 0 条 ⇒ 1.5.29 的 `IllusionMissingSlots`（只看 `partClone` 是否为空）在这种时序下永远判不出问题：一次 pass 之后 `partClone` 仍为空属于正常中间态。

### 真因
「幻化集已切换」与「渲染模型已更新」是两件事。`OutfitSystem::refreshArmorFor()` 只做一次装备 pass，几何要等后续 pass 物化；菜单路径之所以灵，是交互本身产生了大量 pass。场景切换走纯脚本路径（`SceneOutfits`），一次 pass 之后没有任何后续 pass，模型就停在旧几何上，直到玩家再与菜单交互。

### 修法（四条，全部框架无关）
1. `MorphRefresh` 新增公开入口 `RequestOutfitPasses(actor)`：登记 3 次随访（120 / 400 / 900 ms）；主线程 `PollOnMainThread()` 里的 `DrainOutfitPasses()` 到期调用 `OutfitSystem::refreshArmorFor()`；幻化总开关关闭（`IllusionDisplayEnabled()` 为假）时直接返回。
2. `Features::RefreshActor()`（套装列表点选、快速选择窗口、热键、Papyrus 都走它）末尾调用 `RequestOutfitPasses()` —— 于是「点列表」与「脚本切换」共用同一条序列，脚本路径现在也会补跑。
3. `SceneOutfits` 的两处 `OutfitSystem::refreshArmorFor(player)` 改为 `Features::RefreshActor(player)`（场景开始 / 场景结束）。
4. DLL 改名：`CMakeLists.txt` 设 `OUTPUT_NAME "OutfitSystemNG"`，部署目标 `dist/SKSE/Plugins/OutfitSystemNG.dll`；旧名不再发布，仓库 `dist` 与 `build/Release` 里的旧文件已删除。

### 新的日志
| 日志 | 级别 | 含义 |
| --- | --- | --- |
| `MorphRefresh: repeating the equip pass for <FormID> (follow-up N of 3), because one pass alone does not materialise the illusion.` | debug | 随访 pass 真的在跑；每次切换最多 3 条 |
| `Deploying OutfitSystemNG.dll to dist/SKSE/Plugins` | 构建输出 | 模块文件名已改（装包前确认没有旧 `SkyrimOutfitSystemNG.dll`） |

### 兼容性
- 未改钩子、贴件、槽清理；`RequestOutfitPasses` 与 `DrainOutfitPasses` 内没有任何 OStim / SexLab / OBody / SLIF / RaceMenu 名字，只依赖自身「换了套装就刷新」的状态。
- 每个角色最多 3 次额外装备 pass，且只对 `Is3DLoaded()` 的 actor 执行；同一角色重复切换会重置计数，不会累积成风暴。
- 插件身份不变：`Plugin::NAME`（`SkyrimOutfitSystemNG`）、日志文件名 `SkyrimOutfitSystemNG.log`、Papyrus 翻译前缀、存档标识全部保持原值，只改磁盘上的 DLL 文件名。

### 未验证
- 「不弹窗也能显示」未实机验证；120 / 400 / 900 ms 是按菜单路径的 pass 节奏估的，若某套件仍需更多 pass，可再加一档而不用改结构。
- DLL 改名后必须在实际 VFS 里只剩一份；若旧 `SkyrimOutfitSystemNG.dll` 没删，会加载两份插件并互相覆盖幻化结果。

## 4.61 1.5.33：场景套装回退到 1.5.29，只保留「找名字 → 用状态切换那套方法换装」（m07666）

### 症状（1.5.32 实机）

OStim / SexLab 场景开始切 `<当前套装>_sex`，模型不出现；场景结束回普通幻化也要按一次 Tab/Z 才刷新。1.5.30 的补跑 pass、1.5.31 的 `Update3DModel()`、1.5.32 的 `Features::AssignOutfit()` 三种写法实测都没解决。

### 用户判定（m07666 原话）

「依旧和之前一样的问题，我发现你改的地方是不是错了，我说的是 Sexlab 和 OStim 场景的专用换装 `_sex`，你不会改的是按地点/状态自动切换吧？不行直接回退到 1.5.29，从这开始改，把原来的 Sexlab 和 OStim 场景的专用换装 `_sex` 套装逻辑删了，参照按地点/状态自动切换这里的换装方法去换装。」

### 先回答「改错了模块没有」

- `git diff v1.5.29 HEAD -- src/cpp/AutoSwitch.cpp src/cpp/AutoSwitch.h` 输出**为空** ⇒ 按地点/状态自动切换从 1.5.29 到 1.5.32 一行都没动过。
- SexLab / OStim 的 `_sex` 换装在另一个文件：`src/cpp/SceneOutfits.cpp`（`EndsWithSexSuffix()`、`candidate = current + "_sex"`、`ostim_start` / `ostim_end` / `PlayerAnimationStart` / `PlayerAnimationEnd` 四个 mod 事件），MCM 勾选项是 `Wear <current outfit>_sex during SexLab / OStim scenes`。
- `git diff v1.5.29 HEAD -- src` 的全部改动只有：`SceneOutfits.cpp`（1.5.32 的入口与线程改动）、`MorphRefresh.cpp` / `MorphRefresh.h`（注释搬家）、`OutfitSystem.h`（空行）、`version.h`。

### 状态切换到底怎么换装（`src/cpp/AutoSwitch.cpp`，`Evaluate()` 61–73 行）

```cpp
{
    auto service = outfit_service_get_mut_singleton_ptr();
    OutfitSystem::setOutfitUsingState(service->inner(), a_actor);
}
// The refresh needs the outfit lock released.
OutfitSystem::refreshArmorFor(a_actor);
```

触发源：`RE::TESActorLocationChangeEvent`（location sink 在 `ProcessEvent` 里直接调 `Evaluate()`）、`TESFastTravelEndEvent`、以及输入心跳里的 2 秒节流轮询。**没有 SKSE 任务队列、没有专用 `Tick()` 机件、也没有针对某个场景的规则。**

### 修法（1.5.33）

1. **回退源码基线**：`git checkout v1.5.29 -- src`，撤掉 1.5.30 / 1.5.31 / 1.5.32 在 `src/` 下的改动（CMake 的模块名 `OutfitSystemNG.dll` 保留）。
2. **新增 `OutfitSystem::setOutfitByName()`**（`include/OutfitSystem.h` / `OutfitSystem.cpp`，与 `setOutfitUsingState()` 并排）：`has_outfit()` 检查 → `set_outfit_c()` → 返回 `true`；名字不存在返回 `false`，且不动任何状态。
3. **`SceneOutfits.cpp` 只剩「找名字 + 换装」**：StartScene / EndScene 共用同一个 `SwitchTo()`：

```cpp
{
    auto service = outfit_service_get_mut_singleton_ptr();
    if (!OutfitSystem::setOutfitByName(service->inner(), a_actor, a_name.c_str())) {
        return false;
    }
}
OutfitSystem::refreshArmorFor(a_actor);   // The refresh needs the outfit lock released.
```

4. **删掉场景专用机件**：`QueueSceneEvent` / `DrainSceneEvents` / `ScheduleDrain` / `g_generation` / `g_pendingEvents` / `g_drainScheduled` / `std::deque` / `GameState::IsLoading` 分支、`SceneOutfits::Tick()`，以及 `main.cpp:279` 与 `NGHotkeys.cpp:255` 的调用。事件在 `ModCallbackEvent` sink 里**同步**执行（与 location sink 同构），只保留一个 `std::recursive_mutex` 串行化，以及「不在游戏线程就 warn 并跳过」的守卫。
5. **日志升到 info**：`SceneOutfits: OStim scene started; 'A' -> 'A_sex'.` / `SceneOutfits: OStim scene ended; 'A' restored.` 不再需要 Verbose 才能看到。

### 新日志（判定用）

| 日志 | 含义 |
| --- | --- |
| `SceneOutfits: OStim scene started; 'A' -> 'A_sex'.`（info） | 名字已找到并完成指派 |
| `Armor Refresh for <name>` + `Custom: attaching N outfit-only piece(s) …` | 紧随其后的装备 pass，与「点套装列表」完全同一条路 |
| `SceneOutfits: no 'A_sex' outfit for the … scene; create an outfit with exactly that name to use scene switching. The current outfit is kept.`（info） | 名字对不上，幻化保持不变 |
| `SceneOutfits: 'A_sex' could not be assigned for the … scene; the current outfit is kept.`（warn） | 指派失败（套装已删除等） |
| `SceneOutfits: 'A_sex' could not be restored after the … scene.`（warn） | 场景结束恢复失败 |
| `SceneOutfits: '<event>' arrived off the game thread; the scene switch is skipped.`（warn） | 事件不在游戏线程（正常不应出现） |

### 兼容性 / 未验证

- 只碰 `SceneOutfits.cpp` / `include/SceneOutfits.h` / `include/OutfitSystem.h` / `OutfitSystem.cpp` / `main.cpp` / `NGHotkeys.cpp` / `include/version.h` / `CMakeLists.txt`；钩子层、注入壳、贴件语义、槽位清理、`.tri` 推导零改动。
- **未实机验证**。静态侧新增 `tests/scene_outfit_switch.ps1`：断言场景模块里没有任何队列 / `Tick` / 补跑 / 重建 / `set_outfit_c` 字样，换装必须是 `setOutfitByName` 紧跟 `refreshArmorFor`，且 `AutoSwitch` 仍是同样两步顺序；另删掉过时的 `tests/scene_outfit_queue.ps1`。
- **已知风险（写明）**：1.5.29–1.5.32 的日志都证明场景开始时装备 pass 其实执行了（`Armor Refresh for …` + `Custom: attaching 5 outfit-only piece(s) …`）。若 1.5.33 下模型仍不出现，说明差异不在这两步调用本身（换地点/快速旅行本身会让引擎重建被绘制的 3D），需按新日志另找方向，不能再用「多刷新几次」的补丁。

## 4.62 1.5.34：1.5.33 把场景事件整条丢了——`OnGameThread()` 门禁与工作线程（m07842）

### 症状（1.5.33 实机）

OStim / SexLab 场景开始后幻化**压根没切换**（1.5.33 之前是「切了但模型不显示 / 要按一次 Tab」，这一版连切都没切）。

### 日志证据（`D:/Skyrim-dev/bug/SkyrimOutfitSystemNG.log`，700,653 字节 / 7,497 行，`v1-5-33-0`，runtime `1-5-97-0`，base `0x7FF605EB0000`）

- `SceneOutfits` 相关只有 4 行：一条 `SceneOutfits: SexLab / OStim scene detection installed.`（正常），加三条被丢弃：
  - `20:37:33.801` 线程 `39516`：`'ostim_start' arrived off the game thread; the scene switch is skipped.`（warn）
  - `20:38:11.749` 线程 `10604`：`'ostim_end' … skipped`
  - `20:38:29.203` 线程 `27040`：`'ostim_start' … skipped`
- 线程分布：`34628` 有 3,843 行（含 `v1-5-33-0 is loading` 与绝大部分 `Custom: attaching …` / 蒙皮日志，是游戏主线程）；`14492` / `3768` / `27040` / `39184` / `2492` / `38112` / `39516` 各几十到近千行。
- 同一份日志里其它环节都正常：`RaceMenu BodyMorph v4 acquired through skee`、基础钩子全部装好、菜单里切套装时 `Armor Refresh for …` + `Custom: attaching N outfit-only piece(s) …` 照常出现。

### 真因

1.5.33 按 m07666 重写 `SceneOutfits.cpp` 时，把 `AutoSwitch` 的线程模型也照抄了进来 —— 包括 `OnGameThread()`（`RE::Main::GetSingleton()->threadID == GetCurrentThreadId()`）。但两者的**触发源根本不同**：`AutoSwitch` 的 `TESActorLocationChangeEvent` / `Tick` 都在游戏线程上，而 OStim / SexLab 的 mod 事件是从它们自己的工作线程投过来的。于是：

```cpp
if (!OnGameThread()) {
    SKSE::log::warn("SceneOutfits: '{}' arrived off the game thread; the scene switch is skipped.", name);
    return kContinue;
}
```

这一行把三条事件全部丢掉：名字没查、`setOutfitByName()` 没调、`refreshArmorFor()` 没跑 —— 所以症状是「压根没切换」，而不是上一版那种「切了但模型不出来」。1.5.29–1.5.32 之所以能切（即使模型不显示），是因为那时还有 SKSE 任务队列 / `AddTask` 把换装投到主线程。

### 修法

1. **离线程事件用一个 task 交接**：`const std::string eventName(name);` → `tasks->AddTask([eventName]() { Dispatch(eventName); });`。要交接的是「事件名」，不是 `RE::TESObjectREFR*` / sink 内的临时对象 —— 事件到主线程时 sink 早返回了。
2. **游戏线程上的事件仍走内联快路径**：直接 `Dispatch(eventName)`，与 `AutoSwitch` 在 sink 里同步换装的做法一致，不加额外延迟。
3. **新增单一入口 `Dispatch(const std::string& a_event)`**：`std::lock_guard switchLock(g_switchMutex);` 内按事件名调 `StartScene()` / `EndScene()`，两条路径共用同一份逻辑。
4. **不建队列、不加 `Tick()`、不做 generation 计数**：队列那套是 1.5.33 按用户要求删掉的（m07666），这里只借「一次 `AddTask`」这一个既有 API，与 `AutoSwitch::Tick` 回主线程的手段相同。
5. **任务接口缺失时只降级不崩**：`SKSE::GetTaskInterface()` 为空时 warn `… there is no task interface, so the switch cannot be handed over; the scene outfit is left as it is.`，不尝试别的投递方式。
6. **留线程号便于下一轮取证**：离线程时 debug `SceneOutfits: '{}' came from thread {} (game thread {}); the switch is handed to the game thread.`

### 新日志（1.5.34）

| 日志 | 含义 / 处置 |
| --- | --- |
| `SceneOutfits: 'ostim_start' came from thread 39516 (game thread 34628); the switch is handed to the game thread.` | 正常路径（debug，需 Verbose）：事件来自工作线程，已用一个 task 交接到游戏线程 |
| `SceneOutfits: OStim scene started; 'A' -> 'A_sex'.` | 开关真正执行了（info，无需 Verbose）；紧跟同一瞬的 `Armor Refresh for …` + `Custom: attaching N outfit-only piece(s)` |
| `SceneOutfits: 'ostim_start' came from another thread and there is no task interface, so the switch cannot be handed over; the scene outfit is left as it is.` | SKSE 任务接口不可用（极端情况）：本版不换装，也不崩 |
| `'ostim_start' arrived off the game thread; the scene switch is skipped.` | **1.5.33 的丢事件行，1.5.34 起不应再出现**；若见到，说明装的不是 1.5.34 |

### 兼容性

- 钩子层 / 注入壳 / 贴件语义 / 槽位清理 / `.tri` 推导 / DLL 文件名一律未改：`src/cpp/hooking/*` 本版零改动。
- `AutoSwitch.cpp` / `AutoSwitch.h` 本版仍为空 diff（可 `git diff v1.5.33 HEAD -- src/cpp/AutoSwitch.*` 复核）。
- 场景套装仍只做「找对名字 → `setOutfitByName()` + `refreshArmorFor()`」；`_sex` 套装里的槽位规则依旧由用户自己在套装编辑器里配置。

### 未验证

- 「场景开始/结束不按键就切」未实机验证：本版只保证事件不再被丢弃、开关会在游戏线程上执行。若日志出现 `scene started; 'A' -> 'A_sex'.` 而模型仍不出现，那说明问题回到「切了但 3D 不更新」那一层（1.5.30 / 1.5.31 试过的方向），需要按新日志再定位，不是本版能声称修好的。
- `AddTask` 交接会晚一个任务周期（下一次主线程任务）才换装，正常感知不到；若某些机型上看到明显延迟，可以再讨论是否在主线程上直接 `Dispatch()`。

## 4.63 1.5.39：逐槽策略按套装独立 + 从 ESP 生成套装 + 金件页（含服装过滤）

### 症状与诉求

- 幻化栏选中一套装时勾选右侧「逐槽策略编辑器」，会把**所有套装**的该选项一起打开（m09191②）。
- 需要「按 ESP 自动生成套装」：点按钮弹 ESP 列表（按加载顺序，最后加载的在最上面），选中后扫描该 ESP 的装备生成新套装，默认名 = ESP 名称，生成后左侧列表焦点跳到新套装；同槽位多件拆成 `ESP名称_1`、`ESP名称_2` …；槽位策略统一「需要已装备（槽位有装备时才显示幻化）」；设置里有「ESP 自动生成套装自动收藏」开关，默认打开（m09191③）。
- 需要「金件」独立栏：左侧按 ESP 自动分组、右侧显示该组具体金件，可增可删，支持「从 ESP 自动生成」与「按 FormID 选单件」（`plugin.esp:LOCALFORMID`，十六进制，必须先验证存在）；给选中组加单件时 ESP 固定；金件数据与幻化数据分开存（m09191④）。
- 生成套装扫描时**只收正常服装**：FX 特效槽、没有 EditorID、以及各种不该算作服装的记录全部排除；**金件不适用这条**，按金件自己的规则放开（m09426）。

### 实现要点

1. **逐槽策略展开状态按套装名保存**：`src/cpp/MenuOutfitEditor.cpp` 里 `std::unordered_set<std::string> g_slotPolicyOpen;`，`showSlotPolicies = g_slotPolicyOpen.find(a_outfit) != end()`，勾选时对该套装 `insert` / `erase`。JSON 侧未改：每套仍是 `slotPolicy`（基础策略）与 `slotPolicies`（逐槽覆盖），旧文件与 `OutfitSystemData.json` 互通。
2. **`src/core/PluginScan.h/.cpp`（新）**：只读的加载顺序视图。`LoadedPlugins()` 先列普通插件（`GetLoadedMods()` + `GetLoadedModCount()`）再列 light 插件（`GetLoadedLightMods()` + `GetLoadedLightModCount()`），`order` 递增；护甲用 `GetFormArray(RE::FormType::Armor)` 按 `OwningPlugin()`（`TESForm::GetFile(0)->GetFilename()`）归组。只有 `Invalidate()` 会丢缓存。
3. **`PluginScan::IsOutfitArmor()`（本版新增，m09426 的落地点）**：三条同时满足才算服装 —— ①`GetFormEditorID()` 非空（无名记录是补丁占位，列出来也没法认）；②`GetSlotMask().underlying() & ~(1u << 31)` 非零（只覆盖 FX 槽的记录是特效，不是衣服）；③`armorAddons` 里至少一个 addon 的 `bipedModels[SEXES::kMale].GetModel()` 或 `bipedModel1stPersons[SEXES::kMale].GetModel()` 非空（没有模型就没有东西可画）。`PluginsWithArmor()` 同时给出 `armors`（全量槽位记录）与 `outfitArmors`（通过上面三条的数量），幻化页用后者、金件页用前者。
4. **`src/core/EspOutfits.h/.cpp`（新）**：`GenerateFromPlugin()` 先用 `IsOutfitArmor()` 过滤，被排除的记录计入 `notClothing` 并在结果行里说明；分桶时 `mask & ~kFxBit`，FX 位不进任何槽；按 32 槽分桶、每桶第 i 件进第 i 套（不足时复用第 0 件），变体上限 32；同名套装原地覆盖（`erase_all_armors()` + `insert_armor()` + 清逐槽策略 + `set_blanket_slot_policy_c("XXXO")`），不存在则 `create_outfit_preset()`。生成后 `FocusOutfitInList()` + 刷新全部外观。
5. **`src/core/GoldenItems.h/.cpp`（新）**：金件独立存储 `Data\SKSE\Plugins\OutfitSystemGoldenItems.json`（`{"version":1,"groups":[{"plugin":…,"items":["000800",…]}]}`），读写走不可变快照，皮肤钩子随时可查；`Settings::IsGoldenItem()` 委托 `Golden::IsGolden()`，`Outfit.cpp` 的贴件判断逻辑未改。`AddLocal()` 先用 `PluginScan::ResolveArmor()` 验证记录真实存在（要求槽位掩码非零）；`AddAllFromPlugin()` **不做服装过滤**（m09426 的「金件放开」）。旧 `SkyrimOutfitSystemSE.ini` 的 `goldenItems` 在 kDataLoaded 时一次性 `ApplyLegacyEntries()` 后清空，并写 `[Compatibility] GoldenItemsMigrated`。
6. **菜单**：幻化页底部新增「从 ESP 生成套装」块（ESP 列表倒序、搜索框、`Mark generated outfits as favourites` 勾选、生成结果行）；新增「金件」页（`RenderGolden()`，左列表右编辑、导出/导入按钮、按 FormID 添加）；设置页新增「套装生成」段放同一个自动收藏开关。全部文案走 `I18n::T()`，中文侧同步。

### 新日志（排查用）

| 日志 | 含义 |
| --- | --- |
| `PluginScan: {} plugins loaded, {} slot armour record(s) indexed.`（debug） | 加载顺序视图建好 |
| `ESP outfits: … FX effects and other non clothing records were left out (…; {} wearable of {} slot record(s)).` | 生成结果的权威行，含过滤统计 |
| `ESP outfits: That plugin holds no wearable clothing: every slot record is an FX effect, has no editor id or has no model.` | 整个 ESP 没有可生成的服装 |
| `Golden items: {} piece(s) of {} added.` | 金件「添加整个 ESP」的结果（不过滤） |
| `Golden items: {} was added as a golden item.` | 金件单件添加成功 |

### 兼容性

- 幻化 JSON（`OutfitSystemData.json`）结构未变，`slotPolicy` / `slotPolicies` 仍是每套自己的值；金件是**另一个文件**，旧 ini 列表自动迁移一次，不会与幻化数据混写。
- 引擎侧（钩子、贴件、槽位清理、`.tri`、场景套装）零改动，1.5.25 / 1.5.34 的实机结论不受影响。

### 未验证

- 本机没有游戏，菜单交互、ESP 扫描结果、生成出的套装外观全部**未实机验证**；静态侧只有编译与守卫测试。
- 生成规则是「每槽第 i 件配成第 i 套、不足复用第 0 件」，若某个 ESP 的槽位组合本身互斥（例如 4 个上衣 + 1 条裤子 → 4 套），需要用户手动调整；变体上限 32 是硬编码。

## 5. 静态验证


| 项 | 结果 |
| --- | --- |
| 编译 | BUILD_EXIT=0，0 error / 0 warning（1.5.39 Release x64 重编，MSBuild + `/p:BuildProjectReferences=false`；日志 `build/build_task.log`，旁路留档 `build/build_1_5_39.log`；末尾有 `Deploying OutfitSystemNG.dll to dist/SKSE/Plugins`） |
| 版本资源 | `FileVersion = 1.5.39.0` / `ProductVersion = 1.5.39.0` / `FileDescription` / `ProductName` = `Skyrim Outfit System NG` / `LegalCopyright` = `MIT License`（`src/cpp/include/version.h` + `src/cpp/version.rc`，1.4.2 起把 .rc 显式加入 target_sources；此前 glob 只收 .cpp，资源从未编入）。PowerShell `(Get-Item <dll>).VersionInfo` 实测四项均有值 |
| 导入表 | 仅 KERNEL32 / USER32 / ole32 / VERSION / SHELL32 |
| 延迟导入 | 目录 RVA = 0、size = 0（无延迟导入项） |
| 导出 | SKSEPlugin_Load / SKSEPlugin_Query / SKSEPlugin_Version 均存在 |
| 独立加载 | 空目录 `LoadLibraryExW(path, 0, 0)` 返回句柄且 `GetProcAddress` 取到 `SKSEPlugin_Load` / `SKSEPlugin_Query` |
| 1.5.14 关键字符串 | `Custom: rebuilding ` / `slot {:02} clone ` / `outfit revision ` / `MorphRefresh: final rebuild of the illusion of` 可检索 |
| 1.5.15 关键字符串 | `MorphRefresh: taking the illusion of` / `restored with a fresh morph target` / `ReillusionGapMs` / `CycleIllusion` 均可检索；`FileVersion = 1.5.15.0` |
| 1.5.16 关键字符串 | `Outfits: copied ` / `Outfits: pasted ` / `The clipboard does not contain valid JSON.` / `Could not create the pasted outfit.` 可检索；`FileVersion = 1.5.16.0` |
| 1.5.17 关键字符串 | `SLIF NG query interface v` / `SLIFNG_GetQueryInterface` / `BodyInflationNG.dll` / `SLIF NG is not installed` / `inflation changes will rebuild the illusion` 均可检索；`FileVersion = 1.5.17.0`；导入表仍只有 KERNEL32 / USER32 / ole32 / VERSION / SHELL32 |
| 1.5.19 关键字符串 | `MorphProviderFirst` / `has not handed out the BodyMorph interface` / `no skee module in the process` / `the provider pass moved the meshes of` / `the provider pass left the meshes of` / `following them.` 均可检索；`FileVersion = 1.5.19.0`、`ProductVersion = 1.5.19.0`（PowerShell `VersionInfo` 实测）；导入表与延迟导入目录同上，未新增依赖（`skee64.dll` / `skee.dll` / `skeevr.dll` 只以 `GetModuleHandleA` 字面量出现） |
| 1.5.22 关键字符串 | `the function at rva` / `does not call ` / `hooking the first one` / `the deferred pass` / `CustomSkinPlayer.CustomOne` / `the skinning routine of this game build has no call site` 均可检索；`FileVersion = 1.5.22.0`、`ProductVersion = 1.5.22.0`（PowerShell `VersionInfo` 实测）；DLL 大小 1,859,584 字节，sha256 `ca0d3aad…76f94158`；导入依赖未新增（只用已有 KERNEL32 / SKSE 接口与 `SKSE::GetTaskInterface()`） |
| 1.5.24 关键字符串 | `outfit-only piece(s) of` / `slot {:02} clone ` / `left the display set (mask ` / `ApplyArmorAddon refused` / `does not point at a biped model of` / `1.5.24` 均可检索；`FileVersion = 1.5.24.0`、`ProductVersion = 1.5.24.0`（PowerShell `VersionInfo` 实测）；DLL 大小 1,866,752 字节，sha256 `2530C1E5FDE6CF22E87C011BAC0D078F22EF7C53407771DCD6CCC47F71A0204B`；导入依赖未新增 |
| 1.5.26 关键字符串 | `carries a .tri morph target on` / `no interface needed` / `stay armed` / `.tri morph targets are attached to every illusion clone` / `only adds the immediate re-morph` / `has no BodyMorph interface;` / `1.5.26` 均可检索；1.5.20 的 `.tri morph target support` 已从 DLL 中消失；`MorphShapeCallback` / `Obody_ApplyMorph` / `BODYTRI` 仍在；`FileVersion = 1.5.26.0`、`ProductVersion = 1.5.26.0`（PowerShell `VersionInfo` 实测）；DLL 大小 1,868,288 字节，sha256 `7ca36b0d…e54fecf`；导入依赖未新增 |
| 1.5.30 关键字符串 | `repeating the equip pass for` / `because one pass alone does not materialise the illusion` / `follow-up` 可检索；模块文件名 `OutfitSystemNG.dll`（旧名 `SkyrimOutfitSystemNG.dll` 已不在 dist 与 build/Release）；FileVersion 1.5.30.0、DLL 1,939,456 字节 / `2d0d9689…`、导入依赖未新增 |
| 1.5.31 关键字符串 | `Model Rebuild for` / `Skipping model rebuild for` / `asking the game to rebuild the model of` / `so the illusion reaches the drawn 3D` / `could not be assigned for the` / `could not be restored after the` / `rebuildActorModel` 可检索；`FileVersion = 1.5.31.0`、`ProductVersion = 1.5.31.0`；DLL 1,941,504 字节，SHA256 `c8c8ae5db004c0017b029110b23dd707e1e519789c2acf81ba637a5ed63ad6b2`；模块文件名仍 `OutfitSystemNG.dll` |
| 1.5.32 关键字符串（历史） | `no '{}' outfit for the {} scene` / `could not be assigned for the` / `could not be restored after the` / `SceneOutfits: ` / `1.5.32` 可检索；1.5.30/1.5.31 的 `repeating the equip pass for` / `follow-up` / `Model Rebuild` / `rebuild the model of` / `RequestOutfitPasses` **已全部从 DLL 中消失**；`FileVersion = 1.5.32.0`、`ProductVersion = 1.5.32.0`（PowerShell `VersionInfo` 实测）；DLL 1,937,920 字节，sha256 `da9ec8995c599f00cb249b611d66a13c2f3f030f845951b3d6b5eacdca05f875`；模块文件名仍 `OutfitSystemNG.dll`；导入依赖未新增 |
| 1.5.33 关键字符串 | `setOutfitByName` / `SceneOutfits: ` / `scene started;` / `scene ended;` / `create an outfit with exactly that name` / `the scene switch is skipped` / `1.5.33` 可检索；1.5.30/1.5.31 的 `repeating the equip pass for` / `follow-up` / `Model Rebuild` / `rebuild the model of` / `RequestOutfitPasses` 以及 1.5.32 队列相关的 `scene event dropped` **已全部从 DLL 中消失**；`FileVersion = 1.5.33.0`、`ProductVersion = 1.5.33.0`（PowerShell `VersionInfo` 实测）；DLL 1,934,848 字节，sha256 `8c23e5eec3ae0f4561125ab2cabb71e63ef7c21d01e6a8b85755687454716aaf`；模块文件名仍 `OutfitSystemNG.dll`；导入依赖未新增；旧名 `SkyrimOutfitSystemNG.dll` 不在 dist |
| 1.5.34 关键字符串 | `handed to the game thread` / `came from thread` / `there is no task interface` / `scene started;` / `scene ended;` / `setOutfitByName` / `1.5.34` 可检索；1.5.33 的丢弃行 `arrived off the game thread` **已从 DLL 中消失**，1.5.30/1.5.31 的 `repeating the equip pass for` / `follow-up` / `Model Rebuild` / `rebuild the model of` / `RequestOutfitPasses` 仍全部不存在；`FileVersion = 1.5.34.0`、`ProductVersion = 1.5.34.0`（PowerShell `VersionInfo` 实测）；DLL 1,937,408 字节，sha256 `e1cdb9518149d0e1f7a23d1835317620b8258c1e030d6d987b658e4718283782`；模块文件名仍 `OutfitSystemNG.dll`；导入依赖未新增；旧名 `SkyrimOutfitSystemNG.dll` 不在 dist || 1.5.39 关键字符串 | `OutfitSystemGoldenItems.json` / `ESP outfits: ` / `wearable clothing` / `FX effects and other non clothing records were left out` / `skipped as not clothing` / `Golden items: ` / `PluginScan: ` / `1.5.39` 可检索；`FileVersion = 1.5.39.0`、`ProductVersion = 1.5.39.0`（PowerShell `VersionInfo` 实测）；DLL 2,030,592 字节，sha256 `c72a60a6f4a400bbb4cf06a9b2e1c81a86baca487fbf901119e264421aee83da`；模块文件名仍 `OutfitSystemNG.dll`；导入依赖未新增（只多用 `GetFormArray` / `GetLoadedLightMods` 等 SKSE+引擎既有接口）；1.5.34 的 `handed to the game thread` / `scene started;` 与 1.5.30/31 已删机件仍保持原状态 |
| 1.5.29 关键字符串 | `detached ` / `illusion clone(s) whose slot left the display set` / `lost the illusion in slot mask` / `after 3 rebuilds` / `rebuilding the actor` / `left to the engine` 可检索；`ClearBipedPart` 与 `IllusionMissingSlots` 作为内部符号不出现在字符串表（前者只剩一处调用）；FileVersion 1.5.29.0、DLL 1,936,384 B / `4f0bcae1…`、导入依赖未新增 |
| 1.5.27 关键字符串 | `uses .tri ` / `has no .tri beside it` / `Probed: ` / `the armour addon the engine attached` / `has no armour addon yet` / `leaving the clones to the next morph pass instead of cycling` / `but the illusion carries .tri targets` / `every illusion clone whose armour model ships a .tri` / `1.5.27` 均可检索；1.5.26 的 `.tri morph targets are attached to every illusion clone` 已从 DLL 中消失；`carries a .tri morph target on` / `stay armed` / `BODYTRI` / `MorphShapeCallback` 仍在；`FileVersion = 1.5.27.0`、`ProductVersion = 1.5.27.0`（PowerShell `VersionInfo` 实测）；DLL 大小 1,875,456 字节，sha256 `22eb7794…a91af178f`；导入依赖未新增 |
| 1.5.25 关键字符串 | `left to the engine` / `slot {:02} item {:08X} addon {:08X} part {} clone {}` / `1.5.25` 均可检索；1.5.24 的 `cleared for` 与 `ClearBipedPart` / `ClearSlots` 已从 DLL 中消失；`FileVersion = 1.5.25.0`、`ProductVersion = 1.5.25.0`（PowerShell `VersionInfo` 实测）；DLL 大小 1,866,240 字节，sha256 `ae797d9d…efc3936b5`；导入依赖未新增 |
| 1.5.23 关键字符串 | `the site this hook has used since 1.0.0` / `legacy site rva` / `is not a 5-byte call either` / `Illusion display is active: the player skinning hooks are installed.` / `1.5.23` 均可检索；`FileVersion = 1.5.23.0`、`ProductVersion = 1.5.23.0`（PowerShell `VersionInfo` 实测）；DLL 大小 1,863,168 字节，sha256 `be282e7a…7569991df`；导入依赖未新增 |
| 1.5.21 关键字符串 | `hooking the relocated site` / `hooking that one` / `refusing to guess` / `Illusion display is off for this session` / `Skinning hooks` + `SkyrimSE.exe` / `is not a readable object` / `SetIllusionDisplayEnabled` 均可检索；`FileVersion = 1.5.21.0`、`ProductVersion = 1.5.21.0`；DLL 大小 1,866,752 字节，sha256 `d8158be3…6be2a2a3`；导入依赖未新增（`VirtualQuery` 在 KERNEL32 已有导入中） |
| 1.5.20 关键字符串 | `MorphShapeCallback` / `re-morphed ` / `BODY`+`TRI`（代码里拼成字面量） / `Obody_ApplyMorph` / `skee64` / `.tri morph target support` 可检索；`FileVersion = 1.5.20.0`、`ProductVersion = 1.5.20.0`；DLL 大小 1,852,416 字节，sha256 `5352a414…1b094b`；导入依赖未新增（skee64 仍只以 `GetModuleHandleA` 字面量出现，`AddMorphShapeCallback` 是按虚表槽位调用，不产生导入项） |
| 1.5.18 关键字符串 | `morph:%s` / `PregnancyBelly` / `no body morph provider answered` / `inflation follows the slider and geometry probes` 均可检索；`FileVersion = 1.5.18.0`、`ProductVersion = 1.5.18.0`（PowerShell `VersionInfo` 实测）；导入表仍只有 VERSION.dll[6] / KERNEL32.dll[144] / USER32.dll[8] / ole32.dll[1] / SHELL32.dll[1]，延迟导入目录 RVA=0 / size=0；`BodyInflationNG.dll` / `SLIF.dll` / `SLIFNG.dll` 只以 `GetModuleHandleA` 字面量形式出现，不是导入项 |
| 1.5.12 关键字符串 | `MorphRefresh: queued the body morph pass` / `MorphRefresh: rebuilding the illusion of` / `RaceMenu did not answer the interface exchange` / `MorphRefresh: OBody plugin API v1 acquired` 可检索；`Custom: force-rebuilding` / `display pieces` 仍保留 |
| 1.5.11 关键字符串 | `MorphRefresh: OBody plugin API v1 acquired` / `MorphRefresh: OBody is ready` / `MorphRefresh: no body morph provider` / `GetPresetAssignedToActor` 可检索；`Custom: force-rebuilding` / `display pieces` / `after a body morph change` 仍保留 |
| 关键字符串 | `Menu: raw gamepad input enabled` / `RegisterInpoutEvent` / `RegisterInputEvent` / `input-event export` / `Menu: SKSE Menu Framework` /  `Advanced Nudity Detection` / `PAD compatibility` / `PAD slot report` / `Log slot report` / `Outfit editor:` / `Replace the conflicting pieces` / `Menu: sections registered` / `Trampoline ready` / `Skinning hooks applied` / `translation lines read` / `AutoSwitch: native location` / `<random>` / `State random: actor` / `Outfits: deleted` / `DynamicHDT` / `SceneOutfits: SexLab / OStim scene detection installed` / 可选 `ig*`（`igIsKeyDown_Nil`、`igSetNextWindowPos`、`igGetMainViewport`、`igCalcTextSize`、`igGetWindowPos`、`igGetWindowSize`、`igSetScrollHereY_Float`）均在；`###SOSQuickPicker`、`Quick picker: opening at`、`optional entry point` 可检索；UTF-8 `语言`、`快速选择`、`取消幻化` 可检索 |
| Papyrus | 6 个脚本 papyrus.exe compile 全部 status=0；SkyrimOutfitSystemNativeFuncs.pex 与 SkyOutSysQuickslotEffect.pex 已入包 |
| 函数数量 | OutfitSystem.cpp 注册 66 个原生函数，与 psc 里 66 条 Global Native 声明一致（1.3.0 未改 Papyrus 端） |
| 翻译文件 | `interface/translations/SkyrimOutfitSystemNG_english.txt`（29,016 字节）与 `_chinese.txt`（21,530 字节），UTF-16LE+BOM，各 248 条 `$英文<TAB>译文`；槽位条目已换成 30–61 那套名字（`$Amulet<TAB>项链`、`$Unnamed (probably Face)<TAB>未知 (一般来说是 面部)`），1.5.16 追加复制/粘贴 11 条，用脚本回读校验 248/248 条 |
| JSON 策略 | `PolicyToInt()` 与 `PolicyFromInt()` 互逆：0–11 与 128+ 区间全覆盖，其它值返回空；导出样例 `"slotPolicy": 2` / `"slotPolicies": {"9": 8}` 与 `兼容参考/OutfitSystemData.json` 一致 |
| 包内哈希 | SHA256SUMS.txt 逐文件列出并复核（30 个文件 + SHA256SUMS.txt） |

## 6. 首次实机建议顺序

0. 确认 SKSE64 与 Address Library 与游戏版本匹配，能进主菜单。
1. 装本包，进游戏看 `Documents/My Games/<game>/SKSE/SkyrimOutfitSystemNG.log`：`Trampoline ready`、五条 `... patched.`、`Menu: sections registered`、以及 `Compatibility: Advanced Nudity Detection ... detected`（装了 AND 时）。
2. 读旧存档：`Migrating a vN record` → `Migrated co-save vN` → `Post-load: hooks resumed` → `Player 3D is ready`。
3. 菜单建套装 → 应用 → 外观变化；换同槽真实护甲，外观仍是套装件。
4. **AND**：穿上完整护甲应用幻化 → 用 AND 自己的状态显示（或 Papyrus `AND_ModEventListener.GetNude(player)`）确认是“有穿”；把套装换成**空套装** → 应变成裸体/上身空/下身空。日志开 `ExtraLogging` 可看到 `AND compatibility: ... rank 1.`。
5. **PAD（1.4.0 重点）**：先点 Settings → Mod compatibility → **Log slot report**，逐槽确认 `shown 'X' | real 'Y'`；`real '<none>'` 的槽位 PAD 摸不到（裸槽贴件，预期）。然后穿真实护甲 + 幻化，挨打 → 幻化外观应出现破损，PAD 的 Tracked armors 页出现条目。仍不可撕时做对照：`[Compatibility] ProceduralArmorDamage = off` 重启，同样操作若表现一致，说明不在还原路径，请附日志反馈。
6. Ctrl+Shift+L 打开快速选择窗口：**首次**居中 640×800、焦点落在当前套装；拖动/缩放后关闭，重启游戏再打开应回到上次的位置与尺寸（`[UI] QuickPickerX/Y/Width/Height`）；`W/S`/`↑↓`/手柄十字上下选择、`A/D`/`←→`/十字左右翻页（按住加速）、`E`/`Enter`/手柄 A 穿上并关窗、`R`/手柄 Y 随机、`Esc`/手柄 B 关闭；底部 `Remove outfit` 恢复真实外观。详见 `docs/TESTING.md` §3.9。
7. 施放 “Change Outfit” 法术：默认弹出快速选择窗口；菜单 Settings → Random outfit 勾选随机后再施放应直接随机。
8. 存档 → 读档：套装与指派都在；AND 阵营状态在读档后仍然是幻化的结果（心跳会在 1 秒内补写）。
9. **语言**：Settings → Language 选“中文”。若 SMF 有中文字体（`[Fonts] EnableChinese = true` 且 `Data/SKSE/Plugins/Fonts/CN.ttf` 存在），界面立刻变中文；否则保持英文并在日志写 `Language: Chinese was requested but ...`。改 `Data\interface\translations\SkyrimOutfitSystemNG_chinese.txt` 后点 **Reload the language files** 应立即生效（左侧树节点需重启）。
10. **导入导出**：Settings → Outfit data → **Export outfits to OutfitSystemData.json**，确认 `Data\SKSE\Plugins\OutfitSystemData.json` 的键是 `enabled` / `outfits` / `actorOutfitAssignments`（原版结构）；删一个套装再 **Import** 应恢复。
11. **老存档策略**：用 v4 旧存档读档，检查原本设成 `XXXO`（需要已装备）的套装没有变成 `XXOO`（始终使用套装）。
12. **快选窗口（1.4.2 重点）**：按 Z（快捷槽能力/热键）打开 —— 标题应是“天际幻化系统 - 快速选择”而不是 “Debug”；窗口应在屏幕居中，拖动 + 缩放后关闭、重启游戏再打开应回到上次的位置与尺寸；直接改 `[UI] QuickPickerX/Y/Width/Height` 后重启也应生效。日志有 `Quick picker: opening at ...`。
13. **快选长按**：按住 `S`/`↓`/手柄十字下，约 0.2 秒就应开始连续下移（不必按住一秒）。
14. **JSON 往返（1.4.3 重点）**：Export 后确认 `slotPolicy` / `slotPolicies` 写的是**数字**（`"slotPolicy": 2`、`"slotPolicies": {"9": 8}`），与老的 `OutfitSystemData.json` 完全同格式；再 Import 回同一份文件，套装的槽位策略必须原样保留（`需要已装备` 仍是 `需要已装备`，不能变成“从不显示任何东西”）。**最直接的一条**：用 `兼容参考/OutfitSystemData.json`（530 套）覆盖 `Data/SKSE/Plugins/OutfitSystemData.json` 后点 Import，日志应打印 `Imported 530 outfits and 2 actor assignments`，随便挑几套看基础策略都是“需要已装备（槽位有装备时才显示幻化）”。
15. **槽位编号（1.4.3 修正）**：套装编辑器的槽位列表应显示 `头部（槽位 30）`、`头发, 短（槽位 31）`、`项链（槽位 35）`、`盾牌（槽位 39）`、`未知 (一般来说是 面部)（槽位 44）`、`FX（槽位 61）`。
16. **措辞**：中文下策略项应显示“始终使用幻化”“只有幻化时显示幻化”，不再出现“始终使用套装”。
17. 按 `docs/TESTING.md` §3 跑功能矩阵（1.4.3 新增用例在 §3.12，1.4.4 新增在 §3.13）。
18. **删除按钮（1.4.4）**：Outfits 左列表选一套 → Delete → 出现确认条 → Delete；套装从列表消失，日志 `Outfits: deleted '...': yes.`；该套装原本指派给某个 actor/状态，也应一并清掉。
19. **横向滚动（1.4.4）**：套装编辑器里找一条名字很长的槽位行，槽位列表与逐槽策略面板都应能横向滚动，行尾按钮不被文字挡住。
20. **日志分级（1.4.4）**：默认日志只应有生命周期行；勾选 Settings → Diagnostics 的 Verbose logging 重启后，才会出现 Settings/ImGuiApi/Hooks 的逐项诊断行。
21. **原生自动切换（1.4.4）**：让 NPC/玩家换地点，外观应在事件后立即切换（不再等 10 秒）；日志（Verbose）有 `AutoSwitch: ... switched to state N.`；Papyrus.0.log 不应再有 `SetOutfitUsingStateForAllConfiguredActors` 的周期调用。
22. **状态随机（1.4.4）**：给某状态选 `<随机>` → 立刻抽一套并显示；离开该状态再回来应换一套；同一状态内反复触发战斗事件不应反复更换。

23. **列表按钮（1.5.0）**：Outfits 左列表点任意一行的 **Wear**，应当立刻换上并刷新外观；点 **Delete** 应弹出确认条，不再需要“瞄准边缘”。短名字与超长名字各试一次，按钮位置都不应压住文字或跑到窗口外。
24. **快选手柄（1.5.0）**：用手柄十字键在快速选择窗口上下移动，屏幕里只应有一个高亮；按 A 只换装并关窗，不应同时触发 `Remove outfit`；按 B 只关窗。
25. **SMP 重置（1.5.0）**：装 Faster HDT-SMP 后，Settings → Mod compatibility 应显示 `Faster HDT-SMP (DynamicHDT): detected`；勾选 Reset SMP physics，穿带物理的幻化 → 换另一套带物理的套装，目标角色的裙摆/头发物理应重新初始化（不会残留上一套的骨骼状态）。同屏其他 NPC 不应被重置。未装 FSMP 时该行显示 not detected，勾选也不报错。
26. **场景套装（1.5.0）**：建两套 `测试服装` 与 `测试服装_sex`，给玩家穿上前者；勾选 SceneOutfitSwitching；进入 OStim / SexLab 场景 → 应自动换成 `测试服装_sex`；结束 → 应恢复 `测试服装`。再测：当前套装本身叫 `X_sex` → 进入场景不变；只有 `测试服装`、没有 `_sex` 版本 → 进入场景保持原套装。开 Verbose logging 可看 `SceneOutfits: ...` 行。
27. **线程与边角（1.5.2）**：按 `docs/TESTING.md` §3.16 的 A–E 跑：菜单内连续翻页、文本框内按热键、OStim/SexLab 重叠、删除当前过滤套装、关闭菜单后页大小落盘、编辑器列表缓存。
28. **边界与状态（1.5.3）**：按 `docs/TESTING.md` §3.17 的 A–E 跑：Papyrus 删除/覆盖当前套装立即刷新、只配状态套装也能战斗/死亡切换、仅收藏唯一项时随机仍有反应、坏 JSON/坏 co-save 不崩、金件幻化与重复换装不崩。
29. **手柄导航（1.5.4）**：按 `docs/TESTING.md` §3.18 的 A–C 跑：快选窗打开时 SMF 主菜单仍可用手柄、快选窗只移动列表高亮、按 A 不触发 Remove outfit、关闭后主菜单仍可用；无 `_sex` 套装时日志打印期望名字。
30. **SMF 版本（1.5.6）**：先用 SMF 3.18+（3.18 或 3.23 均可）跑 `docs/TESTING.md` §3.19 A；若手上有 3.9，再跑 §3.19 B 验证 raw fallback。日志先确认 `Menu: SKSE Menu Framework 3.18 ...`（或你的实际版本）与是否有 `raw gamepad input enabled`。
31. **X 键（1.5.7）**：打开快速选择窗口，按键盘 X → 幻化取消、HUD 提示“已取消幻化”；按手柄 X → 行为一致；搜索框输入 X 不触发；Remove outfit 按钮行为一致。见 §3.20 A。
32. **`.tri` 刷新（1.5.7）**：幻化一套带 `.tri` 的服装（OBody 身形），在 OBody 换预设、RaceMenu 改 Body Morph 滑块 → 不穿脱、不取消幻化，模型约 1 秒内刷新；日志有 `MorphRefresh: the body morphs of ... changed; rebuilding the illusion.`；第一/第三人称都正确。见 §3.20 B。
33. **身体槽位移除（1.5.8）**：选中一套有多件护甲的套装，滚到右侧 Body slots，点任意一行的“移除” → 该行上方立刻出现确认提示，同一行出现“移除 / 取消”；再点“移除”该件消失，日志有 `Outfit editor: removed ...`；点“取消”则保留。用超长护甲名再试一次，按钮应始终在可视区内。见 §3.21。
34. **OBody 多件刷新（1.5.9/1.5.10）**：幻化一套带 `.tri` 的上衣 + 裤子（其中至少一件是真实穿戴），在 OBody 换身体预设 → 上衣与裤子一起按新身形刷新，不再只有 32 槽上衣变化、裤子穿模；第一/第三人称都正确。开 Verbose logging 应看到 `MorphRefresh: OBody plugin API v1 acquired`、`MorphRefresh: OBody is ready` 与 `Custom: force-rebuilding ... display pieces ...`。见 §3.22 / §3.23。
35. **OBody-only 刷新（1.5.10 重点）**：确认 `Data/SKSE/Plugins` 里没有 RaceMenu；只装 OBody 4.4.0+。幻化一套带 `.tri` 的服装，在 OBody 里换预设 → 约 1 秒内整套幻化件按新身形刷新；取消幻化/重进游戏再试一次。日志不应再出现 `RaceMenu is not installed`（改为 `OBody plugin API v1 acquired`）。见 §3.23。

36. **OBody 换预设后模型消失（1.5.11 重点）**：幻化一套带 `.tri` 的服装 → 在 OBody 换身体预设 → 第三人称身体与衣服应完整（不再只剩头手）；再切第一/第三人称确认两套 biped 都完整。开 Verbose logging 应看到两条 `Custom: force-rebuilding ...`（第一/第三人称各一条）。见 §3.24。

37. **OBody 其余幻化件跟随（1.5.12 重点）**：幻化一套带 `.tri` 的上衣 + 裤子 + 鞋子（至少一件真实穿戴），在 OBody 换预设 → 约 1 秒内**全部**幻化件按新身形刷新，不再只有上衣变化、其余穿模；切第一/第三人称再确认。开 Verbose logging 应看到 `MorphRefresh: rebuilding the illusion of ... after the body morph provider settled.`。见 §3.25。

38. **手动重穿与 OBody 全部件跟随（1.5.14 重点）**：幻化一套带 `.tri` 的衣服，OBody 换一个明显不同的预设 → 按 Z（或快选窗选中当前套装按 E）重穿当前套装 → 幻化件立刻按新身形重建，不再需要「取消幻化再切回」。开 Verbose logging 应看到 `Custom: rebuilding ... outfit revision true`，以及每件护甲的 `Custom: <Armor> slot .. clone 0x...`；两次重建之间同一槽位的 clone 指针应变化。见 §3.26。
39. **幻化循环无可见闪烁（1.5.15 重点）**：幻化带 `.tri` 的衣服后 OBody 换预设，观察画面**不应**出现原版装备；日志里 `taking the illusion ... off for one pass` 与 `illusion of ... restored` 应在同一帧、成对出现。若仍能看到原版装备一闪，把 `[Compatibility] ReillusionGapMs` 设为 20 试一次（会短暂露出，属预期），并在回报里注明。见 §3.28。

40. **复制 / 粘贴整套预设（1.5.16 重点）**：在 Outfits 左列表选一套（名字高亮）→ 点底部 **Copy** → HUD 提示已复制，日志 `Outfits: copied ...`；点 **Paste** → 新套装出现在列表里并被自动选中、滚动到视野中间；因为原名已存在，新名字应是 `<原名>_copy`。再粘一次应得到 `<原名>_copy2`。把粘贴出来的那套打开套装编辑器，**基础策略与单槽位规则必须与源套装完全一致**（例如源是「需要已装备」+ 盾牌「穿透」，粘贴件也要一样）。也可用记事本打开 `OutfitSystemData.json`，复制其中一个 outfit 对象（或整个 `{"outfits":[...]}`）到剪贴板再 Paste，应同样能建出套装。见 §3.29。
41. **SLIF NG 膨胀跟随（1.5.17 重点）**：装 SLIF NG 0.4.18+，幻化一套带 `.tri` 的衣服（例如 SLIF 的 belly 目标经 body profile 映射到 BodySlide 滑块的那种），用 SLIF 的调试法术/控制台给该 actor 加膨胀 → 幻化件应随身体一起鼓起来，不再穿模；停止膨胀/瘦身后同样跟随。日志先确认 `MorphRefresh: SLIF NG query interface v2 acquired`，再看每次变化成对出现 `taking the illusion ... off for one pass` 与 `illusion of ... restored with a fresh morph target`；连续 ramp 时最多每 600 ms 重建一次，不应刷屏。见 §3.30。
42. **幻化件跟随身形（1.5.20 重点，对应 m00002 / m00270）**：幻化一套带 `.tri` 的护甲（BodySlide 生成过 `.tri` 的紧身衣/裙装），然后三种情况各试一次 —— ①开 RaceMenu（`showracemenu`）在 Body Morphs 页签拖体重/胸/腿滑块；②用 OBody NG 给该 actor 换一个身形预设；③用 SLIF NG 加膨胀。三种情况下幻化件身形都应实时跟着变，**中途不得闪出原版护甲**、不需要重穿或先取消幻化。日志先确认 `MorphRefresh: RaceMenu BodyMorph v5 acquired through skee64` 与 `hands out MorphShapeCallback`，再看到 `re-morphed <FormID> on N node(s), M new .tri target(s) (...); the vertices moved.`（每角色 5 秒最多一条，debug 级）。若只有 `has no MorphShapeCallback (v5 added it)`，说明 skee 版本低于 5，退回 250 ms 轮询兜底。见 §4.49 与 §3.33。

43. **幻化不再全裸（1.5.21 重点，对应 m00746）**：装 1.5.21 后先看日志开头两行 —— `Skinning hooks: SkyrimSE.exe 1-5-97-0, … bytes, written …, base 0x…, .text rva 0x… + 0x…` 与 `Illusion display is active: the player skinning hooks are installed.`。若出现 `CustomSkinPlayer: the call to 0x… sits at rva 0x…` 或 `.text holds exactly one call … hooking that one`，说明自定位生效，幻化应正常显示。然后随便切一套幻化：应看到幻化外观（**不是裸体**）。若日志是 `Illusion display is off for this session: … (vanilla pass true, display pass false)`，说明钩子仍没装上 —— 这时**必须看到角色穿着真实护甲**（不是裸体），其它功能正常，把该行连同上面的 `candidate at rva` / `nearest call at rva` 行一起发回。见 §4.50 与 TESTING §3.34。

44. **幻化恢复与钩子自解析（1.5.22 重点，对应 m01170）**：装 1.5.22 后先看日志开头：`Skinning hooks: SkyrimSE.exe <版本>, …` 之后每条钩子都应有一行 `…: the call to 0x… sits at rva 0x… (N byte(s) into the function at rva 0x…)`（debug 级，需开 Verbose logging），最后是 `Illusion display is active: the player skinning hooks are installed.` 或 `Illusion display is active: the hiding hook is installed and the display set is attached from the deferred pass.` —— **两者都算正常**。然后随便切一套幻化，应看到幻化外观（不是裸体）。若出现 `… does not call 0x… within 0x800 bytes; the hook stays disabled`，把紧接的 `the function at rva` / `.text holds N call(s)` / `call at rva` 行发回；若出现 `… calls 0x… N times; hooking the first one at rva …`，也发回（说明该构建里有多条路径，可能只覆盖了第一条）。见 §4.51 与 TESTING §3.35。

45. **1.5.23 恢复 1.5.17 的贴件站点（对应 m01836 / m01837）**：装 1.5.23 后先看日志开头 —— `CustomSkinPlayer: the function at rva 0x364280 has no call to 0x… within 0x800 bytes, but the site this hook has used since 1.0.0 - rva 0x364301 (+0x81 into the function) - is still a 5-byte call; the hook is installed there.` 与 `Illusion display is active: the player skinning hooks are installed.` 两条都在 ⇒ 幻化应恢复到 1.5.17 的行为（切套装能看到幻化外观，不是裸体，也不再出现 1.5.20/1.5.21 的「整体关闭」）。若看到 `… and the legacy site rva 0x… is not a 5-byte call either; the hook stays disabled`，说明这份 exe 连历史站点都不是 `E8`，把该行与 `the function at rva` / `.text holds N call(s)` / `call at rva` 附近五行发回。确认幻化正常后，再验 §4.49 的 `.tri` morph 跟随（RaceMenu 拖滑块 / OBody 换预设 / SLIF 膨胀）。见 §4.52 与 TESTING §3.36。
46. **1.5.24 贴件回到引擎的 ActorWeightModel（对应 m02035）**：装 1.5.24 后，日志里每次贴件应以 `Custom: attaching N outfit-only piece(s) of M displayed (mask 0x…, outfit revision …, body morph …)` 开头，紧跟着的 `Custom: … slot NN clone 0x…` **必须是非 0 指针**；出现 `Custom: ApplyArmorAddon refused …` 或仍是 `0x0` 就把该行连同游戏版本发回。切换套装时不应再出现 `restored N real piece(s)`（1.5.24 已删除该行为）。
48. **1.5.26 幻化件跟随身形（对应 m00002 / m00270 / m02536）**：幻化一套**带 `.tri`** 的护甲（BodySlide 生成过 `.tri` 的紧身衣/裙装），三种情况各试一次 —— ①`showracemenu` 在 Body Morphs 页签拖体重/胸/腿滑块；②OBody NG 换一个身形预设；③SLIF NG 加膨胀。三种情况下幻化件都应**实时**跟着变，中途不得闪出原版护甲，不需要重穿或先取消幻化。日志判定：贴件后出现 `MorphRefresh: <FormID> carries a .tri morph target on N of M cloned slot(s) (…); … no interface needed.`（每角色一次，info）；`N < M` 说明有幻化件的 `.tri` 推导不到。开 Verbose 后每 5 秒最多一条 `stay armed for RaceMenu's own pass`（debug）——**这条是正常的**，说明没拿到接口但目标已就位。若拿到接口还会看到 `re-morphed … the vertices moved`。见 §4.55 与 TESTING §3.39。
47. **1.5.25 身体不再变空模型（对应 m02242）**：装 1.5.25 后，切套装 / 在物品栏脱掉 32 槽护甲 / 切到空幻化套装时，日志里应只出现 `Custom: N slot(s) left the display set (mask 0x…); left to the engine for …`，**不应再有 `cleared for …`**；身体（CK 槽 32，掩码 `0x00000004`）必须始终是原版皮肤/身体。开 Verbose logging 还能看到每个贴过的槽 `Custom: … slot NN item … addon … part … clone …`：`addon` 非 0、`part true` 才算真正建出几何。
49. **1.5.27 `.tri` 目标终于贴上了（对应 m02992）**：装 1.5.27 后开 Verbose，切一次幻化套装，日志里每个幻化件应出现一条 `MorphRefresh: <armor> slot model <path> (from the armour addon the engine attached) uses .tri <file>.`；随后该角色第一次的 `carries a .tri morph target on N of M cloned slot(s)` 里 **N 应等于幻化覆盖的槽数**（0x00010086 那套是 4）。若出现 `has no .tri beside it … Probed: a, b, c, d.`，说明该件四个候选名都不存在 —— 把这行发回（里面有真实模型路径），那是这套衣服本身没有 `.tri`，不是代码问题。拖 Body Morphs 滑块 / 换 OBody 预设 / SLIF 膨胀时幻化件应实时跟随，并且**不应再出现** `taking the illusion off for one engine pass`。见 §4.56 与 TESTING §3.40。

50. **1.5.29 场景结束后的残留幻化与「再按一次键」（对应 m05502）**：复现 B 组 —— OStim 场景中用 Tab/Z 打开快速换装窗切到 `_sex` 套装，结束场景回普通幻化后**不要**再按键，看模型是否已经变回普通套装。日志判定：应出现 `Custom: detached N illusion clone(s) whose slot left the display set (mask 0x…)`（1.5.29 新增）或 `MorphRefresh: … lost the illusion in slot mask 0x…; rebuilding the actor's equipment.`（debug），随后是普通套装的 `attaching N outfit-only piece(s)`；**不应**再需要第二次 Tab。若三次重建后仍看到 `still holds no illusion geometry … after 3 rebuilds`，把该行连同前 40 行发回（多半是件数/ARMA 问题，不是残留）。A 组的崩溃（`EXCEPTION_ACCESS_VIOLATION` 在渲染/剔除阶段读 `[rax+0xF4]`）仍与本插件关系未证，若重装后仍崩，请把 `crash-*.log` 与同一次会话的 `SkyrimOutfitSystemNG.log` 一起发回。见 §4.57 与 TESTING §3.41。

51. **1.5.30 场景切换不用再按一次键（对应 m06712）+ DLL 改名（对应 m06781）**：①先确认装的是 `Data/SKSE/Plugins/OutfitSystemNG.dll`，并且旧名 `SkyrimOutfitSystemNG.dll` 已删除（两个都在会双加载）；②复现 OStim 场景开始 → 看 `_sex` 套装是否**直接**显示（不要按 Tab）；③场景结束回普通幻化，看模型是否**直接**变回（不要按 Tab）。日志判定（需开 Verbose）：每次切换后应出现最多 3 条 `MorphRefresh: repeating the equip pass for <FormID> (follow-up N of 3), because one pass alone does not materialise the illusion.`，紧邻前后能看到 `Armor Refresh for …` 与 `Custom: attaching N outfit-only piece(s) …`。若 3 条之后模型仍不更新，把这行连同前后 20 行发回。见 §4.58 与 TESTING §3.42。
52. **1.5.31 场景切换不再需要弹窗（对应 m07050）**：①装 `Data/SKSE/Plugins/OutfitSystemNG.dll`，确认旧名 `SkyrimOutfitSystemNG.dll` 已删（两个都在会双加载）；②进 OStim 场景，**不要按 Tab / Z**，看 `_sex` 套装是否直接出现；③场景结束回普通幻化，同样不要按任何窗口，看模型是否直接变回。日志判定（需开 Verbose）：每次切换后应依次看到 `Armor Refresh for …` → `Custom: attaching N outfit-only piece(s) …` → `MorphRefresh: asking the game to rebuild the model of <FormID> so the illusion reaches the drawn 3D (follow-up 2 of 3).` → `Model Rebuild for …`。若重建之后仍不出现，把这几行连同前后 20 行发回。见 §4.59 与 TESTING §3.43。

53. **1.5.32 场景套装只做切换（对应 m07382 / m07424）**：①装 `Data/SKSE/Plugins/OutfitSystemNG.dll`，确认旧名 `SkyrimOutfitSystemNG.dll` 已删；②进 OStim 场景，**不要按 Tab / Z**，看 `_sex` 套装是否直接出现；③场景结束，同样不要按任何窗口，看是否直接变回普通幻化。日志判定（需开 Verbose）：只应有 `SceneOutfits: OStim scene started; '…' -> '…_sex'` 与其后的 `Armor Refresh for …` + `Custom: attaching N outfit-only piece(s) …`，**不应再有** `repeating the equip pass for` / `follow-up N of 3` / `Model Rebuild for`。若套装名对不上，会看到 `no '…' outfit for the … scene`（`_sex` 套装名必须与当前套装名严格对应，大小写不敏感）。见 §4.60 与 TESTING §3.44。
54. **1.5.33 场景套装换装改为「状态切换那套方法」（对应 m07666）**：①确认装的是 `Data/SKSE/Plugins/OutfitSystemNG.dll`，旧名 `SkyrimOutfitSystemNG.dll` 已删；②进 OStim / SexLab 场景，**不要按 Tab / Z**，看 `<当前套装>_sex` 是否直接出现；③场景结束，同样不要按任何窗口，看是否直接变回普通幻化。日志判定：应看到 `SceneOutfits: OStim scene started; 'A' -> 'A_sex'.`（info，无需 Verbose）紧跟同一瞬的 `Armor Refresh for …` + `Custom: attaching N outfit-only piece(s) …`；`SceneOutfits: OStim scene ended; 'A' restored.` 同理。**不应再出现** 1.5.30/1.5.31 的 `repeating the equip pass for` / `follow-up N of 3` / `Model Rebuild for`。若模型仍不出现，把这 20 行一起发回。见 §4.61 与 TESTING §3.45。

55. **1.5.34 场景事件不再被线程门禁丢掉（对应 m07842）**：①确认装的是 `Data/SKSE/Plugins/OutfitSystemNG.dll`（1.5.34.0），旧名 `SkyrimOutfitSystemNG.dll` 已删；②进 OStim / SexLab 场景，**不要按 Tab / Z**，看 `<当前套装>_sex` 是否切过去；③场景结束，同样不按键，看是否切回普通幻化。日志判定（要开 Verbose 才看得到交接行）：应先见 `SceneOutfits: 'ostim_start' came from thread <工作线程> (game thread <主线程>); the switch is handed to the game thread.`，紧跟（下一任务周期）`SceneOutfits: OStim scene started; 'A' -> 'A_sex'.` → `Armor Refresh for …` → `Custom: attaching N outfit-only piece(s) …`。**不应再出现** 1.5.33 的 `arrived off the game thread; the scene switch is skipped.`（出现即说明装的不是 1.5.34）。若 `scene started` 有了但模型还不出现，把那 20 行发回 —— 那是「切了但 3D 不刷新」另一层问题，不在本版范围内。见 §4.62 与 TESTING §3.46。56. **1.5.39 菜单三件（逐槽策略按套装独立 / 从 ESP 生成套装 / 金件页，对应 m09191、m09426）**：①幻化栏放几套不同套装，选中 A 勾「逐槽策略编辑器」，再选中 B —— **B 的策略编辑器应是收起的**（旧版会一起展开）；把 A 勾上后打 JSON，`slotPolicy` 只应出现在 A 的记录里。②幻化页底部「Choose a plugin...」→ 列表应按加载顺序倒序排列（最后加载的在最上面）、右侧显示 `%s  (%zu piece(s), %zu skipped as not clothing)`；选一个含 3 件上衣 + 2 条裤子 + 1 个 FX 特效的 ESP，应生成 `<ESP名称>` 与 `<ESP名称>_1` 两套，**FX 那条不进任何一套**，日志 `ESP outfits: …; N wearable of M slot record(s)` 里 M > N。③金件页：同一 ESP 里那条 FX 记录**可以**被「Add every piece of a plugin」加进来（金件不做服装过滤），但「Add piece」手输不存在的 FormID 应提示 `That is not a hexadecimal form id.` 或找不到记录。④设置页「套装生成」段的「生成的套装自动收藏」勾上后生成的套装应带收藏星标。若 ESP 列表为空，说明该 ESP 没有通过 `IsOutfitArmor()` 三条判据的记录（没有 EditorID / 只有 FX 槽 / armor addon 没模型），把 `PluginScan:` 那行发回。见 §4.63 与 TESTING §3.47。

## 7. 已知限制
- **1.5.39 的三项新功能（逐槽策略按套装独立 / 从 ESP 生成套装 / 金件页）全部未实机验证**：本机没有游戏，静态侧只有 `BUILD_EXIT=0`（0 error / 0 warning）与守卫测试 `tests/esp_outfits_filter.ps1` 等 7 项通过。菜单交互、ESP 扫描结果、生成出的套装外观都还没人看过。
- **从 ESP 生成的套装只按「每槽第 i 件配成第 i 套」拼装**：同一个 ESP 里某槽有 4 件、另一槽只有 1 件时，会生成 4 套且后 3 套复用那 1 件；变体上限 32 是硬编码。想要精确搭配需要手动改套装。
- **服装过滤是三条硬判据**（有 EditorID + 至少一个非 FX 槽位 + 至少一个 armor addon 有模型），仍可能漏掉特例：只挂了 `bipedModels[kFemale]` 而男性槽为空的 addon（判据里同时看 `bipedModels[kMale]` 与 `bipedModel1stPersons[kMale]`，通常能覆盖）；用脚本在运行时动态生成、没有 EditorID 的护甲会被当成「非服装」跳过。**金件不受此限**。
- **金件数据换了存储位置**（`Data\SKSE\Plugins\OutfitSystemGoldenItems.json`）：旧 `SkyrimOutfitSystemSE.ini` 的 `goldenItems` 只在首次加载时迁移一次并写 `[Compatibility] GoldenItemsMigrated`；若之后又手工往 ini 里加金件，不会再被读。
- SMP 重置依赖 Faster HDT-SMP 的 Papyrus 静态函数 `DynamicHDT.ResetPhysics`；没装 FSMP 时开关无效但不报错。
- SexLab / OStim 场景套装只对玩家生效（事件不带参与者列表；OStim 的 NPC 子线程不处理）。

- 本机没有 Skyrim；除用户已实测的 PAD、快速选择窗口位置记忆、**SMF 3.9→3.18/3.23 手柄恢复**外，其余游戏内行为未经实机验证（静态层面只证明：DLL 能被 SKSE 加载、无外部 DLL 依赖、钩子站点能找到、脚本能编译）。
- 快速选择窗口的手柄依赖 SKSE Menu Framework ≥ 3.18（3.18 与 3.23 均已实测）；3.9 可用 1.5.6 的 raw fallback，但该路径未实机验证，优先升级框架。
- AND 兼容是**近似判定**：只看槽位掩码，不做关键词级精判；只写裸体/上身/下身三个阵营，不写 `AND_Showing*` 与谦逊度（Modesty）系列。
- AND 会周期性重扫真实装备覆盖等级，我们靠“刷新即同步 + 1 秒心跳”追平；极端情况下（AND 在同一帧内重扫且玩家不再有任何输入）可能出现短暂不一致。
- PAD 仍无法撕“幻化件下**完全没有**真实护甲”的槽位（PAD 只认背包里真实穿着且场景里存在的护甲）；1.4.0 已修复有真实护甲打底的那种情况（§4.10）。
- 引擎内部对话 / 台词条件（TESCondition）读真实装备的 `ExtraWorn` 标记，不受幻化影响；想影响它们只能针对具体 mod 的公开接口做兼容（AND 就是这样做的）。
- 直接调用 `Actor.GetWornArmor` / `GetInventory` 的第三方 DLL 仍看到真实护甲（CommonLibSSE-NG 里是内联实现，无单一入口）。
- 随机换装只有键盘组合键；全局手柄组合键未实现（快速选择窗口内部已可全手柄操作）。
- 翻译文件只覆盖界面文案与 HUD；左侧树节点标题由菜单框架缓存，改语言需重启游戏。
- VR 未构建。
- 1.5.7 的 `.tri` 实时刷新与快选窗 X 键未实机验证；RaceMenu `BodyMorph` 接口为手写最小 ABI 声明。
- 1.5.8 的身体槽位行内移除确认尚未实机验证。
- 1.5.9 的 OBody 多件刷新尚未实机验证；1.5.10 起不再要求 RaceMenu，但需要 **OBody 4.4.0+** 的插件 API，且该 API 为按头文件 ABI 手写的最小声明。
- 1.5.10 的 OBody 预设轮询尚未实机验证；OBody-only 路径下 `ApplyOBodyMorphsToActor` 是否同步应用形态未知。
- 1.5.22 的钩子自解析未实机验证：站点靠「函数体内扫 `E8`」确定，窗口固定 `0x800`（已知最深 `0x373`）。若某运行时把该调用点挪出窗口、或编译器把它改成跳转/内联，该钩子会放弃并打印全 `.text` 调用统计（`CustomSkinPlayer` 放弃时幻化改走 100 ms 节流的延迟附加，不裸体，但每次皮肤更新后会晚约一帧才贴件）。多候选时只钩第一条，可能漏掉同一函数的其它路径（第一人称 / 狼人 / 吸血鬼）。`DontVanillaSkinPlayer` 与 `FixEquipConflictCheck` 仍分别假定 actor 在 `r13` / `rdi`+`rbx`（调用约定固定，不随版本变）。
- 1.5.21 的钩子自定位未实机验证：只在「窗口内唯一候选」或「全 `.text` 唯一调用点」时才采用，多候选直接关闭幻化（显示真实护甲）；用户那份 exe 的该函数布局与记录偏移不同，若附近存在多条到 `ExecuteVisitorOnWorn` 的调用，需要拿日志里的 `candidate at rva` 行手工补偏移。
- 1.5.33 的场景套装换装未实机验证：场景开始/结束现在与「按地点/状态切换」完全同两步（`setOutfitByName()` + `refreshArmorFor()`），但 1.5.29–1.5.32 的日志已证明装备 pass 在场景开始时确实跑过（`Armor Refresh for …` + `Custom: attaching 5 outfit-only piece(s) …`）。若本版模型仍不出现，说明差异不在这两次调用（换地点/快速旅行会让引擎重建被绘制的 3D），需要按新日志另找方向。
- 1.5.11 的 OBody 换预设后身体/衣服消失修复尚未实机验证；修复前提仍是 OBody 4.4.0+ 的插件 API。
- 1.5.12 的 500 ms 延迟二次重建与 RaceMenu 重试尚未实机验证；OBody 端 `ApplyOBodyMorphsToActor` 仍是手写 ABI 声明。
- 1.5.14 的 revision 触发全量重建、400/1400 ms 两段延迟重建与 clone 指针诊断均未实机验证。
- 1.5.15 的同帧循环未实机验证；若某些机型上同帧刷新仍拿到旧形变，需把 `ReillusionGapMs` 调到 20–50（代价是短暂露出原版装备）。
- 1.5.17 的 SLIF NG 兼容未实机验证；SLIF 查询接口是手写最小 ABI（按 `SLIFNG_API.h` 抄的 vtable 顺序），广播兜底与签名去抖也未实机验证。SLIF NG 0.4.17 及更老版本只有消息握手、没有 `SLIFNG_GetQueryInterface` 导出。
- 1.5.24 的「用 `rdi` 传进来的 `ActorWeightModel` 贴件」未实机验证：寄存器契约来自上游 1.5.73 的 SE/AE 两份实现（`rbx`/`rdi`），而 1.5.17 在用户 1.5.97 上实测可用，所以语义有据；但本版改动了 `CustomOne` 的清空策略（不再 `ClearSlots(displayMask)`、不再还原真甲），切换套装时同一槽位换件是否每次都能重建克隆，需要实机确认 `clone 0x…` 指针在换套装后发生变化。
- 1.5.25 的「离开幻化集不清空」未实机验证：依据是上游 1.5.73 从不清理任何槽、且 1.5.17 在用户机器上正常。副作用是切到更小的套装后，不再被接管的那几个槽会短暂保留上一套的克隆，直到引擎因换装/脱装重贴该槽；若长期不刷新，可能看到旧件残留（不会空模型）。
- 1.5.23 的「历史站点回退」未实机验证：`CustomSkinPlayer` 扫不到调用点时会回退到 `entry + 0x81`（AE `+0x1EF`），只校验那里是 5 字节 `E8`，**不校验目标**（这正是 1.5.17 的行为）。语义能成立的依据是 1.5.17 在用户 1.5.97 上全功能正常；若某个未来运行时把该位置改成别的 call 或非 call，日志会打 `is not a 5-byte call either` 并关闭幻化（显示真实护甲，不裸体）。
- 1.5.32 的场景套装「只切套装」未实机验证：本版删掉了 1.5.30 的补跑 pass 与 1.5.31 的 `Update3DModel()`，恢复成菜单点击那条路径。若实机后场景里身体/手/脚槽仍为空模型，则问题不在切换路径或 pass 次数，需要单独定位「引擎 Skin 覆盖槽与幻化克隆争同一槽」（3P/1P 的 `HairTintDiagnostics` 对比是关键证据）。另：场景事件改为游戏线程同步执行后，若某个 mod 在非游戏线程派发 `ostim_start/end`，仍会走任务队列，切换时机比同步路径晚一帧。
- 1.5.18 的几何签名（partClone 顶点缓冲采样）与 2.5 秒 quiet 窗口未实机验证；采样参数（`dataSize >= 64`、头 1024 字节、步长 251）按顶点量级估算，若同一身形两次采样哈希不稳定会表现为「换完身形后再多触发一两轮重建」，届时调大 `kPostCycleQuiet` 或缩小采样范围。SLIF 的 10 个滑块名按 SLIF NG 默认配置抄写，用户改滑块名时靠几何签名兜底。
- 1.5.16 的复制 / 粘贴未实机验证；剪贴板读写是 Win32 `CF_UNICODETEXT`，其它程序正占用剪贴板时会提示「请重试」。粘贴解析的是原版 JSON 结构，`modName` + `localFormId` 解析不到时退回 `rawFormId`（跨 load order 可能指向别的护甲）。
- 1.5.20 的 `.tri` 跟随未实机验证；`BODYTRI` 由我们按 ARMA 模型路径推导后手工挂到幻化 clone 上，若某个 mod 的 `.tri` 命名不符合「模型名去 `_0`/`_1` + `.tri`」的约定（例如 `.tri` 放在别的目录或名字完全不同），该件仍不会跟随，日志里表现为 `0 new .tri target(s)` 且 `the vertices were already current`。第一人称单独配 `.tri` 的 mod 也未覆盖（1P 与 3P 用同一份推导路径）。

- 1.5.26 的「无接口也贴 `.tri`」未实机验证：依据是 skee 源码里 `ApplyBodyMorphs` → `UpdateMorphs` 会遍历每个 biped 槽的 `data.object` 并在子树里找 `BODYTRI`（`BodyMorphInterface.cpp` 690–741 行）；若某个 skee 版本改成只认引擎自己挂的目标、或把 `data.object` 换成别的指针，幻化件仍不会跟随（症状与修前相同，日志里 `N of M` 会显示目标已贴）。另外 `BODYTRI` 的字符串必须由引擎堆分配（插件用静态 CRT，`new[]` 会被别的分配器释放），这一点沿用 1.5.20 的实现。
- 1.5.26 的蒙皮网格几何探测未实机验证：`rawVertexData` 是 GPU 上传前的绑定姿势顶点，理论上动画不影响；若某些驱动/ENB 组合下该缓冲被清空，探测会退化成「读不到数据」（返回 0，跳过该形状），不影响贴件与 `.tri` 跟随。
- 1.5.27 的 `.tri` 目标推导未实机验证：模型路径改问 `biped->objects[i].addon`（引擎解析结果），候选名扩到四个。若某套幻化件的 `.tri` 目录/名字完全不符合这四个候选（例如 `.tri` 与 `.nif` 不同目录、或叫 `body_1_morph.tri`），该件仍不会跟随，日志里是 `has no .tri beside it … Probed: …`（每个护甲/性别一次）。另外 `BODYTRI` 的 value 必须是引擎堆内存（插件静态 CRT），沿用 1.5.20 的实现。

- 1.5.29 的「残留克隆窄清 + 幻化几何对账」未实机验证，两条判据都基于源码推理：①拆克隆的前提是 `ownedMask` 只记录「贴进去时没有真实穿戴物打底」的槽，若某套幻化的件恰好与真实穿戴件同槽（引擎通道先贴真甲、我们再覆盖），该槽不会被计入 `ownedMask`，退出时不会拆 —— 表现是仍残留旧件（下一轮的 `attaching` 会覆盖回来，但若两套件数不同就会看到旧的那件）；②对账依赖 `BipedAnim::objects[bit].partClone` 非空来判断「幻化在不在」，若引擎在某些情况下保留 `partClone` 但几何已失效（顶点缓冲被清），对账不会触发。两种情况的日志分别是「有 `left to the engine` 但没有 `detached`」与「完全没有对账行」，可据此区分。
- 1.5.30 的「场景切换补跑 pass」未实机验证：前提是「所有切换入口都经过 `Features::RefreshActor()`」；若某条路径（例如第三方 mod 自己改装备、或 OStim 内部直接换装）没有经过它，表现仍是「要按一次键」。随访次数与延迟（120 / 400 / 900 ms）按菜单路径的观察估定，不保证每个 load order 都够；不够时表现为「慢一瞬才更新」，而不是不更新。
- 1.5.31 的「重建模型让幻化上屏」未实机验证：前提是 `Actor::Update3DModel()` 真的会让引擎重走 biped→3D 并采用脚本刚写进去的克隆（依据是菜单开合时引擎自己做的正是这件事）。若某个 load order 里重建之后仍不显示，说明还有一层缓存（例如 ENB / 框架缓存了节点），需要按新日志再定位；重建每次切换只发生一次，不会循环。

- 1.5.34 的「离线程事件用 task 交接」未实机验证：本版只保证事件不再被 `OnGameThread()` 丢掉、换装会在游戏线程执行。若日志出现 `scene started; 'A' -> 'A_sex'.` 与随后的 `attaching …` 而模型仍不出现，那属于「切了但被绘制的 3D 不更新」的另一层（1.5.30/1.5.31 试过的两个方向都被实机否掉），本版不声称已解决。

## 8. 排查速查

| 日志 | 含义 / 处置 |
| --- | --- |
| `PluginScan: N plugins loaded, M slot armour record(s) indexed.`（debug） | 加载顺序视图建好。N 应≈你启用的插件数；插件列表为空或数量异常说明 esm/esp/esl 都没读到。 |
| `ESP outfits: … ; N wearable of M slot record(s)).` | 生成套装的权威行：M 是该 ESP 的槽位护甲总数、N 是通过服装过滤的数量。N=0 时会改打 `ESP outfits: That plugin holds no wearable clothing: …`。 |
| `ESP outfits: That plugin holds no wearable clothing: every slot record is an FX effect, has no editor id or has no model.` | 这个 ESP 一件正常服装都没有：没有 EditorID、只覆盖 FX 槽（CK 槽 61 / 掩码第 31 位）、或 armor addon 没模型。换一个 ESP，或直接做成金件。 |
| `Golden items: N piece(s) of <plugin> added.` | 金件「添加某个 ESP 的全部装备」的结果 —— 这条**不过滤**，FX 件与无 EditorID 的件都会进来。 |
| `Golden items: <plugin>:<id> was added as a golden item.` / `Golden items: … could not be resolved` | 金件单件添加：前者成功，后者说明 `plugin:localid` 解析不到（FormID 写错、插件名不对、或该记录没有槽位掩码）。 |
| `Language: Chinese was requested but` | 想用中文但菜单框架没有 CJK 字形：在 `Data/SKSE/Plugins/SKSEMenuFramework.ini` 开 `[Fonts] EnableChinese = true` 并放入 `Fonts/CN.ttf`（或把 `PrimaryFont` 指向已有的中文字体）。插件不会注入字体。 |
| `PAD slot report: slot` | Log slot report 的输出。`real '<none>'` = 该槽没有真实护甲，PAD 无处记损伤。 |
| `Compatibility: Advanced Nudity Detection ... detected; nudity, topless and bottomless ranks now follow the displayed outfit.` | AND 兼容已生效 |
| `... was found, but its nudity factions are missing` | AND 装了但阵营 EditorID 找不到（可能是 AND 改版），兼容自动退回原版行为 |
| `PAD compatibility: ... without a real armour underneath` | 该幻化件下面**没有**真实护甲，PAD 撕不到（设计如此，只能靠真实护甲打底） |
| `Language: wrote the editable translation files to` | 首次运行（目录里一个语言文件都没有）自动写出 english / chinese 两份模板到 `Data\interface\translations` |
| `Language: N language file(s), M lines in total, from ...` | 找到 N 个语言文件、合计 M 条有效翻译行；语言下拉里就会列 N 种 |
| `Language: interface language 'xx' (mode '...'), N translated lines.` | 最终生效的语言与实际覆盖条数 |
| `Imported N outfits and M actor assignments from OutfitSystemData.json.` | JSON 导入成功（N 套装、M 个 actor 赋值）；用 `兼容参考/OutfitSystemData.json` 应得到 `Imported 530 outfits and 2 actor assignments` |
| `OutfitSystemData.json: the policy 'X' is unknown; it is ignored.` | 文件里出现 0–11 / 128+ 之外的策略值：该字段被忽略（槽位回落基础策略），套装不受影响 |
| `Menu: the framework did not create a standalone window` | 菜单框架没给快速选择窗口开独立窗口（版本过旧或 `AddWindow` 失败），快捷窗退化为菜单内一页 |
| `ImGuiApi: N/M optional helpers resolved ... window geometry tracked/not tracked` | 逐符号绑定的结果。小于 M/M 时上面几行会各有一条 `optional entry point ... is missing`，只影响对应小功能 |
| `…: the function at rva 0x… has no call to 0x… within 0x800 bytes, but the site this hook has used since 1.0.0 - rva 0x… (+0x… into the function) - is still a 5-byte call; the hook is installed there.` | 1.5.23 的历史站点回退生效（正常路径：1.5.17 起一直可用的站点在新运行时里目标不同，但仍是成员调用）。幻化应正常显示 |
| `… and the legacy site rva 0x… is not a 5-byte call either; the hook stays disabled` | 连历史站点都不是 `E8`：该钩子放弃，幻化整条关闭（显示真实护甲，不裸体）。把该行与随后的 `the function at rva` / `.text holds N call(s)` / `call at rva` 行发回 |
| `Custom: attaching N outfit-only piece(s) of M displayed (mask 0x…, outfit revision …, body morph …)` | 1.5.24 起每次贴件的开启行：N = 我们要贴的件数，M = 幻化集合总件数 |
| `Custom: … slot NN clone 0x…` | 该槽克隆节点指针；**非 0 才算贴上**，`0x0` 说明 `ApplyArmorAddon` 拒绝 |
| `Custom: ApplyArmorAddon refused …` | 引擎拒绝为这件建 addon，该槽会空着（5 秒节流） |
| `Custom: N slot(s) left the display set (mask 0x…); left to the engine for …` | 1.5.25 起：这些槽不再由幻化接管，原样交回引擎（不再清空；1.5.24 的 `cleared for …` 已删除，更早的 `restored N real piece(s)` 也已删除） |
| `Custom: … slot NN item … addon … part … clone …` | 贴件后的槽状态（debug）：`addon 0` = ARMA 没解析出来；`part true` = 几何已建；`clone 0x0` 本身不算失败证据 |
| `Custom: detached N illusion clone(s) whose slot left the display set (mask 0x…) for …` | 1.5.29 起：这些槽里是我们上一套的幻化克隆、且没有真实穿戴物打底，已拆掉（debug） |
| `MorphRefresh: … lost the illusion in slot mask 0x…; rebuilding the actor's equipment.` | 1.5.29 起：主线程对账发现幻化槽里没有几何，用普通装备刷新重建（debug，同角色 300 ms 退避、最多 3 次） |
| `SceneOutfits: 'ostim_start' came from thread 39516 (game thread 34628); the switch is handed to the game thread.` | 1.5.34 起（debug）：事件来自工作线程，已用一个 task 交接到游戏线程；后面的线程号在排查「事件到底从哪来」时最有用 |
| `SceneOutfits: … came from another thread and there is no task interface, so the switch cannot be handed over; the scene outfit is left as it is.` | SKSE 任务接口不可用（极端情况）：本版不换装也不崩，其它功能不受影响 |
| `… arrived off the game thread; the scene switch is skipped.` | **1.5.33 的丢事件行，1.5.34 起不应出现**；看到它说明装的不是 1.5.34 || `MorphRefresh: … still holds no illusion geometry in slot mask 0x… after 3 rebuilds; it is left alone from now on.` | 三次重建仍空，本版不再尝试；多半是该件 `ApplyArmorAddon` 被拒或无可用 ARMA（warn，每角色一次） |
| `Custom: the weight model 0x… does not point at a biped model of …` | `ModelBiped` 校验没通过（模型指针不是该 actor 的两套 biped 之一），该次不清理遗留槽，只打一次 |
| `Illusion display is active: the player skinning hooks are installed.` | 1.5.23 起唯一的“幻化可用”判定行：藏真甲与贴件两个钩子都装上了 |
| `address library id N is missing` | 该运行时缺 ID，对应钩子被跳过；查 Hooks.cpp 的 ID 对 |
| `unexpected opcode 0xXX at +0xNN` | 站点被别的 mod 改过，钩子跳过（不崩） |
| `Trampoline ready ... (0 bytes free)` | 空间耗尽，后面的钩子会跳过 |
| `MorphRefresh: RaceMenu BodyMorph vN acquired` | 已接上 RaceMenu BodyMorph v4+，`.tri` 形变会跟随刷新 |
| `MorphRefresh: OBody plugin API v1 acquired` | 已接上 OBody 4.4.0+ 插件 API；换预设会触发整套幻化重建 |
| `MorphRefresh: SLIF NG query interface v2 acquired` | 已接上 SLIF NG（0.4.18+）的只读查询接口；膨胀/瘦身会触发幻化重建 |
| `MorphRefresh: SLIF NG is not installed` | 没装 SLIF NG，或版本老到既没有该导出、消息握手也没应答；1.5.18 起膨胀/身形变化仍由几何签名跟随 |
| `Failed to dispatch message to skee` | 命名通道没找到 skee 的监听器（SKSE 名字错位或 skee 注册晚）。1.5.17 起失败后会立刻再发一次无名广播；随后若出现 `RaceMenu BodyMorph vN acquired` 说明兜底成功 |
| `MorphRefresh: OBody is ready; preset changes will rebuild the illusion.` | OBody 已就绪，预设轮询开始工作 |
| `MorphRefresh: no body morph provider answered; morph changes follow the geometry probe only.` | 1.5.18 起的新文案（原先叫 `no body morph provider is available`）：RaceMenu / OBody / SLIF 三路接口都没拿到，此时靠几何签名跟随，`.tri` 形变仍会刷新 |
| `MorphRefresh: the body morphs of <FormID> changed; following them.`（debug） | 1.5.19 起：检测到 morph 签名变化，先把变化交给 RaceMenu / OBody 直接刷（provider-first），不再立刻摘幻化 |
| `MorphRefresh: the provider pass moved the meshes of <FormID>; the illusion followed without a cycle.`（trace） | 1.5.19：provider 那一次刷新就把克隆带上了，没有幻化循环 —— 不露真甲、不卡 |
| `MorphRefresh: the provider pass left the meshes of <FormID> alone; taking the illusion off for one engine pass.`（debug） | 1.5.19：等了 4 × 350 ms 克隆仍没动，回退幻化循环（唯一能让引擎重载 `.tri` 目标的路径） |
| `MorphRefresh: rebuilding the illusion of <FormID> after the body morph provider settled.`（debug） | 仅存在于 1.5.18（单次 700 ms 补建）。**1.5.19 已删除**，看到这行说明跑的还是旧版 DLL。 |
| `MorphRefresh: queued the body morph pass for <FormID>.`（trace） | 1.5.12 起重建后不再只对 RaceMenu 排 morph pass；OBody-only 也会排。 |
| `RaceMenu did not answer the interface exchange` | info。1.5.12 是「2 秒后最多 30 次」，1.5.18 是「12 次 × 5 秒」，两者都会在进游戏前就把窗口耗尽；**1.5.19 起不再放弃**：前 8 次每 5 秒、之后每 30 秒一直试到会话结束，另在 `kPostLoadGame` 与每次真正要用接口前再试。接上会打 `RaceMenu BodyMorph vN acquired`。 |
| `MorphRefresh: <dll> is in the process but RaceMenu still has not handed out the BodyMorph interface; kept asking (N tries).`（info） | 1.5.19：skee 在进程里但不答（每 5 分钟最多一条、上限 8 条）。说明 RaceMenu 装了但握手没成；此时靠几何探针 + OBody/SLIF |
| `MorphRefresh: no skee module in the process - RaceMenu is not installed.`（info） | 1.5.19：进程里根本没有 skee64/skee/skeevr，RaceMenu 没装 |
| `MorphRefresh: RaceMenu BodyMorph ...`（info/warn，未取得） | 没装 RaceMenu 或版本低于 4；若同时装了 OBody 4.4.0+，仍会使用 OBody 后端 |
| `MorphRefresh: RaceMenu BodyMorph v5 acquired through skee64` | 1.5.20：握手成功且记下了应答通道（`skee64` / `skee` / `(broadcast)`）。v5 意味着可以用 `MorphShapeCallback` 事件驱动 |
| `MorphRefresh: RaceMenu BodyMorph vN has no MorphShapeCallback (v5 added it); the .tri morph of the illusion follows the poll instead.` | 1.5.20：skee 版本低于 5（老 RaceMenu），退回 250 ms 轮询 + provider-first；功能可用但滑块拖动时略有滞后 |
| `MorphRefresh: re-morphed <FormID> on N node(s), M new .tri target(s) (...); the vertices moved.`（debug） | 1.5.20：幻化槽补上 `.tri` 目标并重放 morph 成功，顶点确实变了。`M new .tri target(s)` 表示本次新挂了几个 `BODYTRI` |
| `… the vertices were already current.`（debug） | 1.5.20：重放后顶点没变 —— 要么该件本来没有 `.tri`（`0 new .tri target(s)`），要么 morph 值与当前一致 |
| `.tri morph target support inactive (no RaceMenu BodyMorph interface yet)` | 1.5.20：还没拿到 BodyMorph 接口，补 `.tri` 与重放都不做；等 `RaceMenu BodyMorph vN acquired` 出现后自动转为 active |
| `Menu: SKSE Menu Framework <版本> (api <n>)` | 框架版本；低于 3.18 时手柄可能无法进入自定义窗口，优先升级框架 |
| `Menu: raw gamepad input enabled for the quick picker (framework < 3.18).` | 旧框架下已启用 1.5.6 的原始手柄兜底 |
| `Menu: SKSE Menu Framework <版本> handles gamepad input for the quick picker.` | 3.18+ 使用框架自身的 gamepad 翻译，未注册 raw 回调 |
| `Menu: SKSE Menu Framework not found` | 没装菜单框架，其它功能正常，但没有菜单与快速选择窗口 |
| Papyrus.0.log: `Cannot open store for class "SkyrimOutfitSystemNativeFuncs"` | Scripts 目录缺 pex，重装整包 |
| `The player never finished loading its 3D` | 600 帧内玩家 3D 未就绪，刷新被跳过；手动刷新或重进 |
| `ImGuiApi: optional entry point ... is missing` | 菜单框架没导出某个可选 ImGui 函数：只影响对应小功能（如滚动定位），窗口照常打开；把日志里的符号名发来即可加备选名。 |
| `ImGuiApi: the window entry points are incomplete` | 四个开窗函数缺失，快速选择窗口无法自绘：属于框架版本不匹配，装 SKSE Menu Framework 3.x。 |
| `Quick picker: opening at ... (centred)` | 当前 ini 里 `QuickPickerX/Y` 还是 `-1`（首次或从未拖动过）。 |
| `Outfits: deleted 'X': yes.` | 删除按钮生效（1.4.4）。`no` 说明名字已不存在，可能被其它途径删掉了。 |
| `Outfit editor: removed 'X' from 'Y'.` | 身体槽位列表的行内“移除”确认成功（1.5.8）；没有这行说明只点了第一次、还在确认状态。 |
| `Outfits: copied 'X' to the clipboard (N bytes of JSON).` | Copy 按钮成功（1.5.16）；HUD 同时提示。 |
| `Outfits: could not write 'X' to the clipboard.` | 剪贴板被别的程序占用或打开失败；稍后重试即可，不影响其它功能。 |
| `Outfits: pasted 'A' as 'A_copy' (N pieces, blanket XXOO, M slot overrides).` | Paste 成功（1.5.16）；名字带 `_copy` 说明原名已被占用。部件数与策略覆盖数可用来核对是否与源套装一致。 |
| `Custom: force-rebuilding N display pieces (mask 0x........) for ........ after a body morph change.`（debug） | 1.5.9/1.5.10 的整套幻化重建已触发；默认日志看不到，需在 Settings 勾选 Verbose logging。 |
| `AutoSwitch: native location and fast travel detection installed.` | 原生自动切换已注册（1.4.4）；没这行说明 `ScriptEventSourceHolder` 取不到，地点切换不会自动发生。 |
| `AutoSwitch: 00000014 switched to state 3.`（debug） | 状态真的变了才打；默认级别看不到，开 Verbose logging 才有。 |
| `State random: actor 00000014 entered state 3 and rolls 'X'.`（debug） | `<random>` 抽中了哪一套；同状态内重复触发战斗不应重复出现。 |

| `SceneOutfits: SexLab / OStim scene detection installed.` | 场景套装监听已注册（1.5.0）；没有这行说明 `ModCallbackEvent` 源取不到。 |
| `SceneOutfits: ... scene started; 'A' -> 'A_sex'.` | 场景开始并已换上 `_sex` 套装（debug，需 Verbose logging）。 |
| `SceneOutfits: no 'A_sex' outfit for the ... scene; the current outfit is kept.` | 没有同名 `_sex` 套装，按设计保持当前幻化。 |
| `SceneOutfits: OStim scene started; 'A' -> 'A_sex'.`（info，1.5.33 起） | 场景套装已按名字指派完成，紧接其后应有 `Armor Refresh for …`。不再需要 Verbose。 |
| `SceneOutfits: OStim scene ended; 'A' restored.`（info，1.5.33 起） | 场景结束已恢复原套装并紧随一次装备 pass。 |
| `SceneOutfits: no 'A_sex' outfit for the … scene; create an outfit with exactly that name to use scene switching. The current outfit is kept.`（info，1.5.33 起） | 名字对不上；建一套与当前套装同名的 `_sex` 套装即可。 |
| `SceneOutfits: 'A_sex' could not be assigned for the … scene; the current outfit is kept.`（warn，1.5.33 起） | `setOutfitByName()` 返回失败（套装已删除等），幻化保持不变。 |
| `SceneOutfits: '<event>' arrived off the game thread; the scene switch is skipped.`（warn，1.5.33 起） | 事件不在游戏主线程；正常情况下不应出现，出现说明有 mod 在别的线程发场景事件。 |
| `Model Rebuild for <name>`（debug） | 1.5.31：已请求引擎重建该角色的 3D 模型（`Actor::Update3DModel()`）。这是让脚本换装产生的克隆真正上屏的那一步。 |
| `Skipping model rebuild for <name>: no 3D yet.`（debug） | 1.5.31：3D 还没加载，本轮跳过；下一次随访会再试。 |
| `MorphRefresh: asking the game to rebuild the model of <FormID> so the illusion reaches the drawn 3D (follow-up N of 3).`（debug） | 1.5.31：随访链第 2 步就是重建模型；这条之后模型应出现/变回。没有这条说明随访没跑到第 2 步。 |
| `SceneOutfits: '<outfit>' could not be assigned for the <source> scene; the current outfit is kept.`（warn） | 1.5.31：场景套装赋值失败（套装已删除等），幻化保持原样。 |
| `SceneOutfits: '<outfit>' could not be restored after the <source> scene.`（warn） | 1.5.31：场景结束恢复失败，需要手动切回。 |
| `SMP: Faster HDT-SMP (DynamicHDT) is not installed; the physics reset is skipped.` | 开关打开但没装 FSMP；只提示一次，不影响换装（debug）。 |
| `SMP: DynamicHDT.ResetPhysics could not be dispatched for ...` | FSMP 存在但静态调用失败，检查 FSMP 版本/ Papyrus 注册（debug）。 |
| `Custom: rebuilding <n> display pieces (mask 0x…, outfit revision …, body morph …) for <FormID>.`（debug） | 1.5.14 起每次全量重建都打印；`outfit revision true` 表示这次由「套装被重新指派/编辑」触发（Z 键、快选窗 E、Papyrus SetOutfit），`body morph true` 表示由身形变化触发。 |
| `Custom: <ArmorFormID> slot <nn> clone 0x…`（debug） | 1.5.14 诊断：同一槽位两次重建后 clone 指针应变化；不变说明 `ApplyArmorAddon` 复用了旧几何，问题不在 morph 时序。 |
| `MorphRefresh: taking the illusion of <FormID> off for one pass (<原因>)`（debug） | 1.5.15 起循环两阶段在同一帧完成；<原因> 为 `body morph changed`（身形变化）或 `the outfit was re-applied`（Z / 快选窗 E）。 |
| `MorphRefresh: illusion of <FormID> restored with a fresh morph target.`（debug） | 紧随上一条；两条之间不应大量出现其它日志（说明没有跨帧）。 |
| `[Compatibility] ReillusionGapMs`（ini） | 默认 0 = 同帧循环；>0 时退化为两段式，会短暂露出原版装备。 |
| `MorphRefresh: final rebuild of the illusion of <FormID> a second after the body morph change.`（debug） | 1.5.14–1.5.17 的 1400 ms 第二段重建；**1.5.18 起已删除**，看到这行说明跑的还是旧版 DLL。 |

| `Skinning hooks: SkyrimSE.exe 1-5-97-0, N bytes, written … , base 0x…, .text rva 0x… + 0x…` | 1.5.21：装钩子前记下跑的是哪个 exe。反馈钩子问题时连这一行一起发（版本/大小/时间能判断 exe 是否被改过）。 |
| `CustomSkinPlayer: the call to 0x… sits at rva 0x… instead of 0x… (moved by N bytes); hooking the relocated site.`（warn，**1.5.21 专有，1.5.22 起不再出现**） | 1.5.21：偏移对不上但在 ±0x800 内找到唯一那条调用，已钩到自定位站点 —— 幻化正常，把这行记下即可。 |
| `… .text holds exactly one call to 0x…, at rva 0x… (N bytes from the nominal site); hooking that one.`（warn，**1.5.21 专有**） | 1.5.21：附近没找到，但全 `.text` 只有这一处调用，采用它 —— 幻化正常。 |
| `… N calls to 0x… sit inside the +/-0x800 window; refusing to guess…` + `candidate at rva 0x…`（**1.5.21 专有**） | 1.5.21：多候选无法判断，钩子被放弃、幻化本局关闭（真实护甲可见）。把这些行发来补偏移。 |
| `Illusion display is off for this session: the player skinning hooks are not both installed (vanilla pass missing|hooked, display pass missing|hooked). The real equipped armour stays visible.`（critical） | 1.5.21 起：成对钩子缺一个，幻化关闭以免裸体。1.5.22 起只有 `vanilla pass missing` 才会走到这里（`CustomSkinPlayer` 缺失改走延迟附加）；1.5.21 时 `display pass false` = `CustomSkinPlayer` 没装上（看上面的 CustomSkinPlayer 行）。 |
| `ShouldOverrideSkinning: 0x… is not a readable object; ignoring the call.`（warn） | 1.5.21：钩子位置传进来的不是有效指针（防御）。偶发可忽略；持续出现说明钩错位置，反馈日志。 |

| `…: the call to 0x… sits at rva 0x… (N byte(s) into the function at rva 0x…).`（debug） | 1.5.22：站点由「地址库定函数 + 函数体内扫 `E8`」决定，这行告诉你钩在哪儿。四个钩子各一条，需要 Verbose logging。 |
| `…: the function at rva 0x… does not call 0x… within 0x800 bytes; the hook stays disabled so the game keeps running.`（error） | 1.5.22：该运行时这个函数体里没有到目标函数的调用（被内联或改写）。紧跟的 `the function at rva` / `.text holds N call(s)` / `call at rva` 行是证据，请发回。`CustomSkinPlayer` 缺这条时幻化仍可用（走延迟附加）；`DontVanillaSkinPlayer` 缺这条则幻化关闭（显示真实护甲）。 |
| `…: the function at rva 0x… calls 0x… N times; hooking the first one at rva 0x…. If the illusion skips a path in this game build, report this line.`（warn） | 1.5.22：多条调用只钩第一条。幻化正常就不用管；若某些路径（第一人称 / 狼人 / 吸血鬼）不刷新，把这行发回。 |
| `Illusion display is active: the hiding hook is installed and the display set is attached from the deferred pass.`（info） | 1.5.22：`CustomSkinPlayer` 的壳钩子没装上，幻化改由主线程延迟任务贴件（每次皮肤更新后约一帧）。功能正常，属预期降级。 |
| `CustomSkinPlayer: the display set is attached from inside the skinning routine.`（info） | 1.5.22：壳钩子装上了，走快路径（与 1.5.20 之前一致）。 |
| `Custom: the skinning routine of this game build has no call site to hook; attaching the display set from the deferred pass.`（debug） | 1.5.22：第一次走延迟路径时打一条，说明这条路真的在用（每批一次）。 |

| `MorphRefresh: <FormID> carries a .tri morph target on N of M cloned slot(s) (…); … no interface needed.`（info，每角色一次） | 1.5.26：幻化克隆件已带上 `.tri` 目标，RaceMenu/OBody 自己的 morph 遍历能走到它们。这条是**功能就位**的证据；`N < M` = 有幻化件的 `.tri` 推导不到（看该件 ARMA 模型名/目录） |
| `MorphRefresh: .tri morph targets are attached to every illusion clone; … The provider interface only adds the immediate re-morph.`（info） | 1.5.26：模块已装好。**不再出现** `.tri morph target support active/inactive`（1.5.20 的措辞） |
| `MorphRefresh: … has no BodyMorph interface; N .tri target(s) of M slot(s) stay armed for RaceMenu's own pass (…)`（debug） | 1.5.26：没拿到接口，但目标已就位 —— 正常，不是错误。拖滑块时由 RaceMenu 自己的 `ApplyBodyMorphs` 变形 |
| `MorphRefresh: RaceMenu did not answer the interface exchange on any channel; the .tri targets stay armed, only the immediate re-morph and the morph callback are off.`（info） | 1.5.26：接口没拿到（1.5.97 上常见）。功能仍应正常，只是少了「立刻重放」与 `MorphShapeCallback` 两条加速项 |
| `MorphRefresh: re-morphed … on N node(s), M .tri target(s) of K slot(s) (…); the vertices moved.`（debug） | 1.5.26：拿到了接口并立刻重放了 morph，顶点确实变了 |
| `MorphRefresh: <armor> slot model <path> (from the armour addon the engine attached) uses .tri <file>.`（debug） | 1.5.27：该幻化件的 `.tri` 目标找到了。若拖滑块仍不动，先确认这行里的 `<file>` 在 `Data\meshes\` 下真的存在 |
| `MorphRefresh: <armor> slot model <path> (from …) has no .tri beside it, so RaceMenu cannot morph that slot. Probed: …`（debug） | 1.5.27：四个候选名都不存在，该槽无法被 morph。把这行发回（含模型路径与试过的名字） |
| `MorphRefresh: <armor> has no armour addon yet; no .tri target can be derived.`（debug） | 1.5.27：引擎还没解析该槽的 ARMA；每次重建都会重试（不缓存空值） |
| `MorphRefresh: the illusion of <FormID> carries .tri targets; leaving the clones to the next morph pass instead of cycling.`（debug） | 1.5.27：已武装，不再摘幻化。**这是正常状态** |
| `MorphRefresh: the provider pass left the meshes of <FormID> alone, but the illusion carries .tri targets; leaving it to the next morph pass.`（debug） | 1.5.27：provider 没带动网格但目标已就位，等 RaceMenu 下一次 pass。不再回退幻化循环 |
| `MorphRefresh: repeating the equip pass for <FormID> (follow-up N of 3), because one pass alone does not materialise the illusion.`（debug） | 1.5.30：场景开始/结束等脚本切换之后的补跑 pass 在跑，属正常；玩家不应再需要按 Tab。若 3 条都过了模型仍不更新，把这行连同前后 20 行发回 |
| `SceneOutfits: no 'X' outfit for the SexLab scene; create an outfit with exactly that name to use it here.` | 场景套装名对不上：`<当前套装>_sex` 不存在（大小写不敏感匹配后仍没有）。建一套同名 `_sex` 套装即可。 |
| `SceneOutfits: 'X' could not be assigned for the OStim scene; the current outfit is kept.` | 切换调用失败（套装已删除/数据层拒绝），当前幻化保持不变，不会残留半套。 |
| （不再出现）`MorphRefresh: repeating the equip pass for … (follow-up N of 3)` / `Model Rebuild for …` | 1.5.32 已删除补跑与重建机件；若日志里还有这两行，说明装的不是 1.5.32。 |

## 9. 维护提示
- **「能不能收」的规则分两条路，不许合并**（m09426）：幻化用的套装生成只收正常服装 —— 走 `PluginScan::IsOutfitArmor()`（有 EditorID + 至少一个非 FX 槽位 + 至少一个 armor addon 有模型），并且分桶时用 `mask & ~kFxBit`；金件是用户自愿指定的任何记录，`Golden::AddAllFromPlugin()` / `AddLocal()` **不调用** `IsOutfitArmor()`。以后新增任何「按 ESP 批量收件」的入口，先想清楚它属于哪条路，不要图省事复用同一个过滤函数。
- 逐槽策略编辑器的展开状态是 **UI 状态、按套装名分开存**（`MenuOutfitEditor.cpp` 的 `g_slotPolicyOpen`），不是套装数据；它不进 JSON、不能影响别的套装，也不能拿它当「这套装了逐槽策略」的判据（判据是 `slotPolicies` 非空）。
- 模块文件名（1.5.30 起 `OutfitSystemNG.dll`）**只影响** SKSE 的加载与消息路由顺序；插件身份不许跟着改：`SOS_PCH.h` 的 `Plugin::NAME`、日志文件名 `SkyrimOutfitSystemNG.log`、Papyrus 翻译前缀 `SkyrimOutfitSystemNG_<lang>.txt`、存档标识一律保持原值。改名时必须确认 `Data/SKSE/Plugins` 最终 VFS 里只有一份，绝不能双加载。
- 「切换了套装」不等于「模型已更新」：切换入口必须走 `Features::AssignOutfit()`（点套装列表那条路）。**不要**再往切换路径里塞「补跑几次装备 pass」（1.5.30，实测无效）或「重建 3D」（1.5.31，实测无效，且被用户明确否掉）。
- 「模型没更新」与「幻化没切换」是两件事：`BipedAnim` 里已经有克隆、`HairTintDiagnostics` dump 里也有几何，画面仍可能停在旧模型（1.5.31 日志即如此）。判定问题必须先看 dump 里那几个槽（3P slot 1/2/4/7/13）是否都有 addon 与几何 —— 有，就说明「切换」没问题，不要再改切换代码。1.5.30 的补跑与 1.5.31 的 `Update3DModel()` 两个方向已被实机否掉，不要换名字复活。

- 游戏版本升级：只改 `src/cpp/hooking/Hooks.cpp` 里各 `REL::RelocationID(seID, aeID)` 的两个 ID（宿主与目标函数），站点由 `FindCallInFunction` 自动重扫，**不要再加任何字节偏移**。唯一例外是 `CustomSkinPlayer`：它走 `FindCallInFunctionOrOffset`，扫不到时才回退到 1.0.0 起的历史站点（SE `+0x81` / AE `+0x1EF`，1.5.17 在 1.5.97 上实测可用）；换运行时如果这行 warn 变成 error，说明连历史站点也不是 `E8`，此时才需要按真实反汇编重新定位。
- `.tri` 跟随**不得**以「拿到 skee 接口」为前提：`BakeIllusion()` 只在重放那一小段用 `g_api`，贴 `BODYTRI` 必须无条件执行（1.5.26 的教训；1.5.20–1.5.25 就是因为门禁把整个机制关掉了）。
- `.tri` 目标的模型路径**必须**用 `biped->objects[slot].addon`（引擎已解析的 ARMA）取，不要再用 `TESObjectARMO::GetArmorAddon(race)` 当主来源 —— 后者在自定义种族 / 未列种族 / 默认种族回退时返回 null，1.5.20–1.5.26 因此一个 `BODYTRI` 都没贴上（1.5.27）。
- 幻化槽「交回引擎」与「拆掉自己的克隆」是两件不同的事（1.5.29 的教训）：1.5.25 为了不再把身体槽清空，把所有离开幻化集的槽都只打 `left to the engine`，结果引擎那边没有真甲可重建时（场景套装、非穿戴件）旧克隆永久残留，用户表现为「要再按一次键才刷新」。今后新增任何「交回引擎」的分支，都必须同时回答「这个槽里是不是我们自己放的东西」——用 `AppliedState::ownedMask` 这类记录，而不是靠猜。
- 新增 Papyrus 函数：`OutfitSystem.cpp` 注册 + `src/papyrus/skyrimoutfitsystemnativefuncs.psc` 声明 + 重新编译 pex 入包 + `docs/PAPYRUS_API.md` 表格。
- **兼容第三方 mod 一律走数据契约**：优先找对方“已经对外的结果”（阵营、全局变量、Papyrus getter、JSON 配置），不要去 hook 对方函数或写它的内存。这样对方升级不换契约就仍然兼容。
- 新增 Papyrus 脚本时记得同步 dist/Scripts：漏一个 pex 就会让整类原生函数失效（1.0.1 的教训）。
- 任何跨线程访问 OutfitService 都必须先取守卫；守卫的析构必须解锁。
- 新增界面文案：在 `I18n.cpp` 的 `kDictionary` 加一条（并重跑生成脚本，更新 `Data/interface/translations/SkyrimOutfitSystemNG_english.txt` 与 `_chinese.txt` 两份 233 条模板），否则外置文件里缺少该键、只能回退英文。
- 翻译文件解析规则：**TAB 优先、`=` 兜底**（键里可以带 `=`），所以生成脚本必须写 TAB 而不是 `=`。
- 槽位编号：菜单里显示的是 CK 号 = `BIPED_OBJECT` 索引 + `kDisplaySlotOffset`（30）；日志与内部数据仍用 0 基索引，改文案时不要把两者混用。
- 策略序列化：`OutfitSystemData.json` 必须写数字 id（`PolicyToInt`），字符串码只用于导入兼容；新增策略时同时更新 `kPolicies`、`PolicyFromInt` 的逆映射与词典文案。
- `MenuUI` 里调用新增的 `ig*` 之前先看 `ImGuiApi::Api::Extras` 有没有：可选绑定缺失时不能让菜单整块失效。
- 场景套装（`SceneOutfits.cpp`）只负责「找对名字 → `OutfitSystem::setOutfitByName()` + `OutfitSystem::refreshArmorFor()`」，与「按地点/状态切换」（`AutoSwitch::Evaluate()`）共用同一个两步序列。任何与某个 mod 场景相关的槽位特判、时机特判、补跑、重建、SKSE 任务队列、`Tick()` 心跳都不许加回这里：`_sex` 套装里的规则全部由用户自己在套装编辑器里配置（m07424 / m07666 原话）。
- 场景事件的线程门禁只能「交接」不能「丢弃」：OStim / SexLab 的事件来自它们自己的工作线程，`if (!OnGameThread()) return;` 会把整条场景换装丢掉（1.5.33 → m07842）。正确写法是 `SKSE::GetTaskInterface()->AddTask([eventName]{ Dispatch(eventName); })` 交接一次，游戏线程上仍走内联快路径。
- 改「场景套装」时如果发现自己在写新的调度/刷新机制，先去看 `AutoSwitch::Evaluate()` 怎么做的：用户明确要求照抄那条路（m07666）——「参照按地点/状态自动切换这里的换装方法去换装」。
- 「按地点/状态切换」（`AutoSwitch.cpp`）与「场景套装」（`SceneOutfits.cpp`）是两个不同模块，用户报告场景问题时不要改错文件；核对办法是 `git diff <上个 tag> HEAD -- src/cpp/AutoSwitch.*` 是否为空。
