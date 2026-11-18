# Skyrim Outfit System — NG

把任意已穿护甲的外观替换成任意套装（“幻化”），战斗数值、护甲评级、perk 与套装加成仍然读取真实护甲。

本仓库是 Skyrim Outfit System SE 0.7.0 的 **CommonLibSSE-NG 移植版**：原 C++ 幻化实现（xbyak 钩子、Papyrus 层、策略系统、ESP/脚本/翻译）全部保留，只把随包的 Rust 数据层等价改写为 C++，并在此基础上增加/替换若干功能。没有任何 fitting-room 代码。

## 1.5.34 最新说明（2026-11-18）

修「OStim 场景开始后幻化压根没切换」（m07842）。1.5.33 把「按地点/状态切换」的线程门禁 `OnGameThread()` 一起抄进了场景模块，而 OStim / SexLab 的事件来自它们自己的工作线程 → 事件在查名字之前就被丢掉。

- **改了**：`SceneOutfits.cpp` 里离线程事件改走一次 `SKSE::GetTaskInterface()->AddTask([eventName]{ Dispatch(eventName); })` 交接到游戏线程；新增单一入口 `void Dispatch(const std::string&)`；游戏线程上的事件仍内联执行。不建队列、不加 `Tick()`。
- **没改**：换装调用序列仍是 `setOutfitByName()` + `refreshArmorFor()`（与按地点/状态切换同两步）；钩子层、注入壳、贴件语义、槽清理、`.tri` 推导、DLL 文件名（`OutfitSystemNG.dll`）零改动。
- **未验证**：尚未实机验证。若日志出现 `scene started; 'A' -> 'A_sex'.` 而模型仍不出现，那是「切了但 3D 不刷新」的另一层问题。
## 1.5.33 最新说明（2026-11-18，历史，其 `OnGameThread()` 门禁把工作线程场景事件整条丢弃）

**验证状态（1.5.33）**：尚未实机验证。Release 全量重编 `BUILD_EXIT=0`，0 error / 0 warning；DLL `OutfitSystemNG.dll` 1,934,848 B，FileVersion `1.5.33.0`，SHA256 `8c23e5ee…`（`build/Release` 与 `dist` 一致）。

- **按你的要求回退到 1.5.29 再改**（m07666）：源码基线 `git checkout v1.5.29 -- src`，撤掉 1.5.30 的「补跑装备 pass」、1.5.31 的「重建 3D 模型」、1.5.32 的场景入口写法，只留「场景套装」这一件事。
- **改的是 SexLab / OStim 的 `_sex` 模块，不是「按地点/状态自动切换」**：`AutoSwitch.cpp` / `AutoSwitch.h` 从 1.5.29 到本版**一行未改**（`git diff v1.5.29 HEAD -- src/cpp/AutoSwitch.*` 为空）；`_sex` 换装一直在 `SceneOutfits.cpp`。
- **换装方法照抄「按地点/状态自动切换」**：新增 `OutfitSystem::setOutfitByName()`，与 `setOutfitUsingState()` 并排；场景开始/结束都走同两步 —— 锁内指派名字、锁外 `OutfitSystem::refreshArmorFor()`，与 `AutoSwitch::Evaluate()` 完全同构。
- **场景专用机件全部删除**：SKSE 任务队列（`QueueSceneEvent` / `DrainSceneEvents` / `ScheduleDrain`）、`Tick()` 心跳及 `main.cpp` / `NGHotkeys.cpp` 两处调用都不在了；事件在 `ModCallbackEvent` sink 里同步执行。开始/结束日志升到 info（`SceneOutfits: OStim scene started; 'A' -> 'A_sex'.` / `SceneOutfits: OStim scene ended; 'A' restored.`），不用开 Verbose 就能判定。
- **未改动**：幻化钩子、注入壳、贴件语义、槽清理、`.tri` 目标推导、DLL 文件名 `OutfitSystemNG.dll`；1.5.25 已实测确认的基础幻化与空幻化不受影响。
- **未实机验证**：1.5.29–1.5.32 的日志都显示场景开始时装备 pass 其实跑过（`Armor Refresh for …` + `Custom: attaching 5 outfit-only piece(s) …`）。若本版模型仍不出现，说明差异不在这两步调用，需要按新日志另找方向，届时请把 `SceneOutfits:` 前后 20 行一起发回。

## 1.5.32 最新说明（2026-11-18，历史，其场景入口写法被 1.5.33 取代）

**验证状态（1.5.32）**：尚未实机验证。Release 重编 `BUILD_EXIT=0`，0 error / 0 warning；DLL `OutfitSystemNG.dll` 1,937,920 B，FileVersion `1.5.32.0`，SHA256 `da9ec899…`（`build/Release` 与 `dist` 一致）。

- **按用户要求删掉「_sex 套装专属逻辑」**（m07382 / m07424）：场景套装现在**只做两件事** —— 找对名字、切套装。1.5.30 的「补跑几次装备 pass」与 1.5.31 的「重建 3D 模型」全部删除：用户实机证明两者都无效（`attaching …` 每次都执行、`Model Rebuild for` 也跑了，模型仍不出现）。
- **切换走的就是「点套装列表」那条函数**：`SceneOutfits` 的开始/结束一律调 `Features::AssignOutfit()`，与菜单里点 Wear、快速选择窗口按 E 完全同一条路；参考了按地点/战斗切换的实现方式，场景事件在**游戏线程上同步**执行（不再排进 SKSE 任务队列）。
- **名字查找更宽容**：先试 `<当前套装>_sex`，找不到就在全部套装名里做**忽略大小写**匹配；仍没有就打一行 info 提示建一套同名套装，幻化保持不变。`_sex` 套装里的槽位规则完全由你自己在套装编辑器里配置，代码不再有任何特判。
- **未改动**：幻化钩子、注入壳、贴件语义、槽清理、`.tri` 目标推导、DLL 文件名 `OutfitSystemNG.dll`；1.5.25 已实测确认的基础幻化与空幻化不受影响。
- **未实机验证**：场景里身体/手/脚槽若仍为空，问题不在切换路径（1.5.31 日志里的 `HairTintDiagnostics` dump 显示这些槽其实有几何），需要另找方向；届时请连同日志与 3P/1P 段一起发回。

## 1.5.31 最新说明（2026-11-18，历史，其修法已被用户否决）

**验证状态（1.5.31）**：尚未实机验证。Release 重编 `BUILD_EXIT=0`，0 error / 0 warning；DLL `OutfitSystemNG.dll` 1,941,504 B，FileVersion `1.5.31.0`，SHA256 `c8c8ae5d…`（`build/Release` 与 `dist` 一致）。

- **修「场景开始/结束都必须按一次 Tab/Z 才显示」（m07050）**：1.5.30 的三次自动补跑 in 场景切换时确实全部执行、每次都有 `Custom: attaching N outfit-only piece(s)`，但模型仍不出现 —— 同帧诊断 dump 显示 biped 槽里**已经有几何**。结论：装备通道（biped 条目）→ 几何物化 → **上屏（引擎重建 3D 模型）** 是三层，缺的是第三层；开/关一次菜单之所以灵，就是因为它让引擎重建了模型。
- **新增 `OutfitSystem::rebuildActorModel()`**：`Actor::Update3DModel()` + `Is3DLoaded()` 守卫，在 `MorphRefresh` 随访链第 2 步（120 / 400 / 900 ms 中的 400 ms 那次）之前调用一次，日志 `Model Rebuild for …` / `asking the game to rebuild the model of … (follow-up 2 of 3)`。
- **场景入口照用户要求改写**：`SceneOutfits` 删掉自己拼的 `set_outfit_c` + `RefreshActor`，场景开始/结束直接调 `Features::AssignOutfit()` —— 与「点套装列表」完全同一条函数；失败时打 warn 并保持原幻化。
- **未改动**：幻化钩子、注入壳、贴件语义、槽清理、`.tri` 目标推导、DLL 文件名 `OutfitSystemNG.dll` 全部保持 1.5.30 状态；1.5.25 已实测确认的基础幻化与空幻化不受影响。
- **未实机验证项**：`Update3DModel()` 是否足够让引擎采用刚写入的克隆（依据是菜单开合时引擎自己做的正是这件事）；若重建后仍不显示，需要按新日志再定位缓存层。

## 1.5.30 说明（历史，已被 1.5.31 取代）
**验证状态（1.5.30）**：尚未实机验证。Release 重编 `BUILD_EXIT=0`，0 error / 0 warning；DLL 1,939,456 B，FileVersion `1.5.30.0`，SHA256 `2d0d968953abe0c3715cf1e2e8b882b2d410d937fc30e0a8659dda451d79cfda`，`build/Release` 与 `dist` 一致。

- **模块改名（重要）**：从 1.5.30 起插件文件名是 **`OutfitSystemNG.dll`**。安装前请删除旧的 `Data/SKSE/Plugins/SkyrimOutfitSystemNG.dll`，两个名字同时存在会双加载。插件内部身份不变：日志仍写 `SkyrimOutfitSystemNG.log`，Papyrus 名、翻译文件前缀、存档标识都不变，存档可继续用。
- **修「场景里切 `_sex` 不显示、必须按一次 Tab/Z 才更新」（m06712）**：真因不是调度器坏了，而是「幻化集已切换」不等于「渲染模型已更新」——一次装备 pass 只把 `addon`/`part` 写进 biped 槽，真正被渲染的克隆几何要由后续 pass 物化。快速换装窗之所以「一按就出来」，是因为开关菜单让引擎自己连跑十几轮 pass；脚本路径（场景开始/结束）一次 pass 之后没有任何后续 pass。现在每次切换后由 `MorphRefresh::RequestOutfitPasses()` 补跑 120 / 400 / 900 ms 三次装备 pass，日志会看到 `MorphRefresh: repeating the equip pass for <FormID> (follow-up N of 3), because one pass alone does not materialise the illusion.`
- **统一入口**：`Features::RefreshActor()` 末尾调用上面的补跑；`SceneOutfits` 的两处场景切换改为走 `Features::RefreshActor(player)`，与「点套装列表」共用同一条序列。新增的切换入口也必须走它。
- **未改动**：幻化钩子（`Hooks.cpp` 注入壳、站点选择）、贴件语义、槽清理策略、`.tri` 目标附加逻辑全部保持 1.5.29 状态；1.5.25 已实测确认的基础幻化与空幻化不受影响。
- **未实机验证项**：三次 follow-up 是否能覆盖所有套装的物化时机；某件装备若 900 ms 后仍未物化，可再加一档延迟（结构无需改动）。

## 1.5.28 说明（历史）

**验证状态（1.5.28）**：尚未实机验证，不能声称已实机修好。生产函数路径回归 6/6 通过；最终 Release BUILD_EXIT=0，DLL 版本/哈希与交付副本一致。包内逐文件清单见 SHA256SUMS.txt；最终 zip 大小、哈希及解包复核记录见包外 VERSIONS.md 与同名 .zip.sha256，避免自引用。

- **修正资源路径**：1.5.27 日志里的模型路径已经带 `meshes` 前缀，`TriFileExists()` 又加一次，实际探测成 `meshes\meshes\...`；1.5.28 规范化模型/TRI 路径，资源打开路径只保留一次 `meshes` 前缀。
- **收紧 BODYTRI 诊断**：不再只按 extra data 名称匹配；必须是 `NiStringExtraData`、路径非空且对应资源可打开才计数。日志改为 `has readable BODYTRI resources on N of M...`，并明确 `paths only, not proof of TRI shape matching or morph movement`。N/M 只是路径资源可读性，不是 TRI shape 名匹配、顶点移动或实时跟随的成功证据；即使 N=M 也不能宣告修好。
- **分离模型缓存**：以实际 addon（ARMA）+ sex + 1P/3P 区分缓存；1P 选择 `bipedModel1stPersons`，3P 使用第三人称模型，避免多 ARMA、性别与视角之间串用路径。
- **保留有效回调**：去掉会误吞所有 biped 回调的 `IsIllusionClone` 过滤，只用 `selfWrite` 防止自家写入重入。provider 调用前采集几何签名，再作调用后比较，避免把已更新的状态当基线。
- **自动刷新不摘幻化**：自动 morph 刷新不再 `CycleIllusion` 摘掉再穿回，而排队 `RequestBake`；不改基础幻化钩子、不清身体槽，也不以重新穿脱作为自动兜底。
- **共享 extra data 安全性**：规范化已有 BODYTRI 时，不原位改写共享字符串；为该节点创建新的 `NiStringExtraData` 替换，持有 `NiPointer` 保留原对象，替换失败时恢复。`GetBodyMorphs` 等未使用虚槽的声明签名纠正，虚表槽顺序不变。
- **能力边界**：主机制仍依赖 RaceMenu 自己的 morph 遍历能访问克隆件、TRI shape 能匹配。拿到且通过版本门禁的接口时才可额外重放；无接口重放不能保证实时，轮询/路径可读日志都不能替代实机顶点与外观验证。

### RaceMenu 接口拿不到时

SKSE64 2.0.20 的 `RegisterListener(nullptr)` 只订阅注册当时已有的 sender 列表（含当时预留槽），不是对未来 sender 的持续订阅；skee 的 `[Features] bEnableEarlyRegistration=1` 若使其过早注册，可能漏掉后加载的 SOS。`QueueProviderRetry()` 只重发消息，不能补建 skee 对 SOS 的订阅关系。公开配置修法：检查**实际游戏生效的** `Data/SKSE/Plugins/skee64.ini`，若 `[Features] bEnableEarlyRegistration=1`，由用户改为 `0` 并完全退出、重启游戏；不自动修改，也不打包用户 ini。当前没有本次 `skse64.log`，不能断言本次配置必为 `1`。协议依据：[SKSE v2.0.20 PluginManager.cpp L488–616](https://github.com/ianpatt/skse64/blob/v2.0.20/skse64/PluginManager.cpp#L488-L616)。

> **1.5.28 纠正（历史说明，不作当前验收依据）**：旧文档将握手归因于“仅消息嵌套”/receiver 名字错位的解释已纠正，实际是注册时机导致 sender 订阅遗漏；主线程 Queue 重试并非完整修法。旧 `carries a .tri morph target on N of M`、`N > 0` 或 `N = M` 判定均不充分，不能证明 TRI shape 匹配或 morph 顶点移动；旧实时成功措辞是当时目标/假设而非实机结论。当前以 1.5.28 最新章节为准。

## 支持版本

| 运行时 | 状态 |
| --- | --- |
| Skyrim SE 1.5.97 + SKSE64 2.0.20 | 支持（SE 侧 Address Library ID） |
| Skyrim AE 1.6.1170 / 1.6.1179 / 1.6.640 等 1.6.1130+ | 支持（AE 侧 ID） |
| Skyrim VR | 不支持 |

要求 Address Library for SKSE Plugins。

## 安装

1. 用 mod 管理器安装本包（SKSE/Plugins/OutfitSystemNG.dll、Scripts/*.pex、SkyrimOutfitSystem.esp、翻译）；若旧版本留下 `SkyrimOutfitSystemNG.dll`，**必须删掉**（两个名字同时存在会双加载）。
2. 可选安装 **SKSE Menu Framework 3.18 或更新版本**：没有它插件照常工作（幻化、热键、Papyrus API 可用），只是没有设置菜单。3.18 与 3.23 均已实测手柄正常；3.9 等旧版在自定义窗口里收不到手柄，升级框架即可，不需要换 DLL。
3. 进游戏打开菜单框架（默认 F1），选择 Skyrim Outfit System 分类。

## 功能

### 保留自原版
- 套装由任意护甲组合而成，可收藏、重命名、导出/导入 JSON（**结构与原版 `SkyrimOutfitSystem\OutfitSystemData.json` 完全一致**，两版之间可直接互导）。
- 逐槽位策略（12 个内建 4 字符策略 + blanket 策略）。
- 13 种状态自动换装（战斗、城市、城镇、地下城、世界，各含晴/雨/雪）。
- “黄金件”例外（默认龙裔 DLC 龙鳞护甲）永远显示真实外观。
- Papyrus API（SkyrimOutfitSystemNativeFuncs）与原版命名一一对应。

### 本移植新增
- **设置菜单改用 SKSE Menu Framework**（Quick picker / Outfits / Actors / Settings 四页），不再依赖 SkyUI MCM；旧 MCM 脚本 SkyOutSysMCM 保留为空壳只为兼容旧存档 quest 绑定。原版 MCM 的细节功能已逐条搬到菜单里（含套装编辑器：从背包/穿戴/FormID/全表搜索添加护甲、槽位冲突确认、32 槽逐槽策略、blanket 策略），对照表见 `docs/MCM_PARITY.md`。Settings 页可直接改两个热键、随机换装的过滤条件、法术行为、兼容开关与调试日志，**改动立即写回 `Data/SKSE/Plugins/SkyrimOutfitSystemSE.ini`**。
- **1000+ 套装浏览**：名字搜索、仅收藏过滤、分页（每页 5–500 可调）、双击应用、行内收藏/删除/重命名。
- **随机换装**：默认 Ctrl+Shift+O，可只从收藏挑、可排除当前套装；菜单里也有 “Roll now”。
- **定点兼容：Procedural Armor Damage（PAD）**：不做任何补丁。PAD 靠“背包里真实穿着（ExtraWorn）+ 场景里找到那件护甲的模型”判定可撕件；本插件换上幻化外观后会如实把真实护甲写回 biped 记录，所以 PAD 的门禁照常通过，撕的是你**看得见**的那层 mesh，损伤记在真实护甲上（符合“数值仍按真实护甲”的设定）。**没有真实护甲打底的槽位 PAD 永远撕不到**（背包里没有那件护甲，它无处记损伤）——此时日志会打一行 `PAD compatibility: <actor> shows pieces over slot mask 0x… with no real armour underneath`，排障时点 Settings → Mod compatibility 的 **Log slot report** 按钮，日志会逐槽列出「幻化显示什么 / 真实穿着什么」。注意 PAD 自己的 “Tracked armors” 页只列**已经受过伤**的护甲副本，列表为空只说明还没撕过，不代表它没看到装备。
- **定点兼容：Advanced Nudity Detection（AND）**：不做任何补丁、不改它的代码。AND 的判定结果发布在阵营上（AND_NudeActorFaction / AND_ToplessFaction / AND_BottomlessFaction），本插件按幻化后的覆盖情况写阵营等级，AND 自己的 Papyrus 接口、对话条件与其它读阵营的 mod 都会跟着变；幻化成空套装时三项都会变红。阵营按 EditorID 解析，AND 升级只要不改 EditorID 就继续有效，找不到时只记日志并退回原版判定。
- **原生 Papyrus 新函数**：RandomOutfit、GetOutfitCount、GetOutfitNameByIndex、SearchOutfits、OpenQuickPicker / CloseQuickPicker / IsQuickPickerVisible、IsQuickSlotRandom / SetQuickSlotRandom。
- **快速选择窗口**：原生弹窗（搜索 + 仅收藏 + 分页 + **每页数量可调且记住**），默认热键 Ctrl+Shift+L，默认 640×800 屏幕居中，**拖过的位置与尺寸会写回 ini、重启后保持**；“Change Outfit” 法术与菜单里的 Quick picker 页打开的是同一个窗口。**全键盘/手柄可操作（手柄需 SMF 3.18+；旧版由 1.5.6 的 raw fallback 兜底）**：`W`/`S`、`↑`/`↓`、手柄十字上下选择（点按单步、长按 0.22 秒后加速），`A`/`D`、`←`/`→`、手柄十字左右翻页，`E`/`Enter`/手柄 A 换上，`R`/手柄 Y 随机换一套，`X`/手柄 X 取消幻化，`Esc`/手柄 B 关闭；打开时焦点落在当前穿着的套装上，随机后焦点跟到抽中的那套。底部还有 **Remove outfit（取消幻化）** 按钮，与 X 键等价。不再需要 UIExtensions，也不受 Papyrus 列表菜单 ~100 页限制。法术默认就弹这个窗口（想改回随机换装：菜单 Settings → Random outfit 勾选 “The "Change Outfit" power rolls a random outfit”，或 ini `[RandomOutfit] QuickSlotRandom = on`）。
- **`.tri` 形变跟随支持（1.5.28，待实机验证）**：规范化 meshes 路径，按实际 addon + sex + 1P/3P 解析资源，补充可读的 `NiStringExtraData("BODYTRI")` 目标，依赖 RaceMenu 自己的 morph 遍历；有合适接口时可额外重放。自动刷新仅排队 `RequestBake`，不再摘幻化再穿回，不改基础钩子与身体槽。可读资源 N/M 只证明路径可打开，不证明 TRI shape 匹配或顶点移动；无接口重放不能保证实时，不承诺“边拖边变”已修好。
- **切换幻化时重置 SMP（Faster HDT-SMP）**：Settings → Mod compatibility 勾选 “Reset SMP physics when the outfit changes”（ini `[Compatibility] AutoResetSMP`，默认 off）。每次换装只对**换装的那个角色**调用 FSMP 的 `DynamicHDT.ResetPhysics(actor, false)`（温和 soft reload），不会全局 reset，也不影响其他 NPC。没装 FSMP 时开关无效但不报错，菜单会显示检测结果。
- **SexLab / OStim 场景套装**：Settings → Mod compatibility 勾选 “Wear <current outfit>_sex during SexLab / OStim scenes”（ini `[Compatibility] SceneOutfitSwitching`，默认 off）。场景开始时，如果当前幻化名已以 `_sex` 结尾就不变；否则查找“当前名称 + `_sex`”，找到就换上、找不到就保持；场景结束后恢复进入前的套装（若玩家中途手动换过则不覆盖）。
- **外置可编辑翻译文件**：`Data\interface\translations\SkyrimOutfitSystemNG_<language>.txt`，**一个语言一个文件**（随包带 `_english.txt` / `_chinese.txt`，UTF-16LE，各 248 条）。Skyrim 传统格式 `$英文原文<TAB>译文`，也接受 UTF-8 与 `key=value`。首次运行若目录里没有语言文件会自动写出全量模板；改完点 Settings → Language 的 **Reload the language files** 即生效，删掉某行会回退到内置词典，所以加/改/裁翻译都不用重新编译。**多一种语言** = 复制一份改名（如 `_german.txt`），重启后自动出现在语言下拉里，不需要改插件。格式细节见 `docs/I18N.md`。
- **菜单中英双语**：Settings → Language 可切 自动（跟随游戏 sLanguage）/ English / 中文，改动立即写回 ini `[UI] Language`；左侧菜单节点标题也走翻译（这部分需重启游戏才刷新，框架会缓存节点标题）。中文字形由菜单框架提供：`Data\SKSE\Plugins\SKSEMenuFramework.ini` 里 `[Fonts] EnableChinese = true` 且 `Data\SKSE\Plugins\Fonts\CN.ttf`（或 PrimaryFont 指定的字体）存在时才会切到中文，否则界面保持英文并在日志说明原因（避免满屏方块）。不做任何字体注入或 hook。
- **钩子安全预检**：Address Library ID 缺失或目标函数根本不在宿主函数里时只记日志并跳过该钩子，不会崩游戏。1.5.22 起**没有任何字节偏移**：`REL::RelocationID(seID, aeID)` 只用来定位宿主函数，站点靠在该函数体内扫「`E8` 且 `rel32` 等于目标函数地址」确定（1 条采用、多条钩第一条并留日志、0 条报错并列出全 `.text` 调用点与距离）。`ShimWornFlags` / `CustomSkinPlayer` 的壳代码只碰 `rcx`（`InventoryChanges*` → `owner`），不假定 actor 在哪个寄存器。`CustomSkinPlayer` 钩不到站点时幻化**不再关闭**：改由主线程延迟任务贴件（`MarkAttachPending`，FormID 100 ms 节流）；只有「藏真甲」的 `DontVanillaSkinPlayer` 装不上时才会关掉幻化并显示真实护甲，日志 `critical` 一行说明。
- **独立 trampoline**：私有 64 KB 内存块（在 .text ±1 GB 内分配），不依赖 SKSE branch pool。

## 配置文件

Data/SKSE/Plugins/SkyrimOutfitSystemSE.ini（缺失时用随包默认值，不覆盖用户修改）。读取顺序：先 Data/SKSE/Plugins 下这份，再 My Games/<game>/SKSE/SkyrimOutfitSystemSE.ini（可用于个人覆盖）：

[Customization] NewOutfitDefaultBasePolicy = XXOO / NewOutfitSetDefaultSlotPolicies = on
[Debug] ExtraLogging / InstallHooks
[RandomOutfit] Hotkey = ctrl+shift+O / QuickPickerHotkey = ctrl+shift+L / FavoritesOnly / ExcludeCurrent / QuickSlotRandom（off = 法术弹快速选择窗口，默认；on = 法术随机换装）
[Compatibility] GoldenItems = Dragonborn.esm:00021739 / AdvancedNudityDetection = on / ProceduralArmorDamage = on / AutoResetSMP = off / SceneOutfitSwitching = off
[UI] Language = auto（auto 跟随游戏语言，也可写 en / zh / 任意语言文件名后缀） / OutfitListWidth = 470 / OutfitPageSize = 50 / QuickPickerPageSize = 20 / QuickPickerX / QuickPickerY / QuickPickerWidth / QuickPickerHeight（窗口几何由插件自动写回；删掉这四项即恢复居中 640×800）

日志：Documents/My Games/<game>/SKSE/SkyrimOutfitSystemNG.log。

## 从源码构建

见 docs/BUILD.md。简述：VS2022 + CMake >= 3.21 + vcpkg（x64-windows-static），产物自动拷到 dist/SKSE/Plugins/。

## 目录

src/cpp/ 原版 C++ 层；src/core/ 取代 Rust crate 的 C++ 数据层；src/papyrus/ 脚本源码；mod_files/ ESP+ini+翻译；dist/ 交付目录；docs/ 文档。

## 从旧版本升级

1. 直接用新包覆盖安装（DLL 名不变：`SkyrimOutfitSystemNG.dll`）。
2. 首次读取旧存档时日志会出现 `Migrating a vN record ...` 与 `Migrated co-save vN: ...`，之后存档会写回 JSON 版本 7。
3. 1.0.1 修复了「载入存档卡在载入界面」的根因（服务层互斥锁未释放），旧版本的卡死不需要删除存档。
4. 1.0.2 必须整包覆盖：上一版漏打包了 SkyrimOutfitSystemNativeFuncs.pex，只换 DLL 不会恢复 Papyrus 原生函数；覆盖后旧存档里的套装与指派不受影响。
5. 1.0.3 建议整包覆盖：脚本 SkyOutSysQuickslotEffect.pex 与 SkyrimOutfitSystemNativeFuncs.pex 都有改动（法术改为弹快速选择窗口、新增 IsQuickSlotRandom/SetQuickSlotRandom），只换 DLL 会导致法术行为与菜单设置对不上。
6. 1.1.0 整包覆盖：感知功能（重绑 Actor.GetWornForm / WornHasKeyword）已移除，改为 PAD / AND 定点兼容；`IsPerceptionEnabled` / `SetPerceptionEnabled` / `GetPerceivedArmor` 三个 Papyrus 函数不再存在，ini 里的 `[Perception]` 段与 `InstallPerception` 也失效（改用 `[Compatibility] AdvancedNudityDetection` / `ProceduralArmorDamage`）。旧存档不受影响，套装与指派照常读取。
7. 1.2.0 建议整包覆盖（脚本未变，只需换 DLL 与 ini）：新增菜单语言切换与 PAD 槽位诊断，`SkyrimOutfitSystemSE.ini` 多了 `[UI] Language` 段。
8. 1.3.0 建议整包覆盖：SMF 菜单补齐原版 MCM 的全部细节功能（套装编辑器、Apply to 目标选择、附近角色添加、快捷槽法术开关），`SkyrimOutfitSystemSE.ini` 多了 `[General] QuickslotsEnabled`；`SkyOutSysMCM.pex` 重新编译，补回了原版脚本对外提供的 `GetModVersion()` / `GetVersion()` / `GetQuickslotManager()` 三个接口（只有第三方脚本会调用它们，不装也能玩）。旧存档与 ESP 不需要换。
9. **1.4.0** 整包覆盖：新增可编辑翻译文件 `SKSE/Plugins/SkyrimOutfitSystemNG_i18n.txt`（首次运行自动生成）；`SkyrimOutfitSystemSE.ini` 的 `[UI]` 段多了 `OutfitListWidth` / `OutfitPageSize` / `QuickPickerPageSize`；快速选择窗口改为自绘居中窗口并支持全键盘/手柄操作；`Custom()` 现在会把 `BIPOBJECT.item` 还原成真实护甲，PAD（Procedural Armor Damage）终于能撕到幻化外观。旧存档、ESP、Papyrus 脚本都不需要换。
10. **1.4.1** 建议整包覆盖：翻译文件从 `SKSE/Plugins/SkyrimOutfitSystemNG_i18n.txt` 换成 `Data\interface\translations\SkyrimOutfitSystemNG_english.txt` / `_chinese.txt`（旧文件可以删掉，插件不再读它）；`SkyrimOutfitSystemSE.ini` 的 `[UI]` 段可能自动多出 `QuickPickerX/Y/Width/Height`（你拖动快速选择窗口后才会写）；修复了 v4 老存档槽位策略被误认成 `XXOO`（始终使用套装）的迁移 bug；Settings 页新增 **Export / Import outfits to OutfitSystemData.json**，结构与原版一致。旧存档、ESP、Papyrus 脚本不需要换。
11. **1.4.2** 建议整包覆盖（脚本、ESP、旧存档都不用换）：快速选择窗口改为自己开窗 —— 位置/尺寸真正生效、标题不再显示 “Debug”、ini 里的 `QuickPickerX/Y/Width/Height` 有效；长按首次延迟 0.40→0.22 秒；`OutfitSystemData.json` 的策略字段现在认中文标签（“需要已装备（槽位有装备时才显示套装）”这类也能正确导入）；套装编辑器的槽位名带编号（`头部（槽位 0）`）；中文策略文案的“套装”改为“幻化”。同时请更新 `Data\interface\translations\SkyrimOutfitSystemNG_english.txt` / `_chinese.txt`（词典 224 条不变，但 7 条中文措辞改了）。
12. **1.4.3** 建议整包覆盖（脚本、ESP、旧存档都不用换）：槽位编号从内部索引（`头部（槽位 0）`）改成 CK / xEdit / 游戏里显示的号（`头部（槽位 30）`、`盾牌（槽位 39）`、`FX（槽位 61）`），32 个槽位名按官方对照表重写（`项链`、`未知 (一般来说是 面部)` 等）；`OutfitSystemData.json` 导入导出全面对齐原版 v0.6.x —— `slotPolicy` / `slotPolicies` 都用数字枚举（`"slotPolicy": 2` = 需要已装备、`5` = 始终使用幻化、`8` = 穿透），导入仍兼容四字符码/翻译键/中英标签。请同时更新 `Data\interface\translations\` 下两份语言文件（槽位名改了）。

13. **1.4.4** 建议整包覆盖（脚本、ESP、旧存档都不用换）：Outfits 列表的 **Delete** 现在真的会删（弹出确认条，删除后同步清掉所有 actor 与状态引用）；套装编辑器加了横向滚动条，长槽位名不再挡住按钮；默认日志只保留生命周期行，诊断行移入 **Verbose logging**；地点/状态自动切换改由 SKSE 原生事件检测（不再等 Papyrus 的 10 秒轮询），`SkyOutSysAutoSwitchTrigger.pex` 换成空实现；**状态套装每个下拉新增 `<random>`（`<随机>`）**，进入该状态时从套装里随机抽一套（遵守“仅收藏”与“排除当前”），离开再回来会重抽。请整包更新 Scripts 里的 `SkyOutSysAutoSwitchTrigger.pex` 与两份语言文件（233 条）。
14. **1.5.0** 建议整包覆盖（ESP / 旧存档不用换）：Outfits 列表的 Wear/Delete 按钮重新布局，不再被整行 Selectable 抢点击；快速选择窗口当时加了 `NoNavInputs` / `NoNavFocus` 想让手柄只保留一个焦点（**1.5.4 已移除**，因为会让 SMF 主菜单的手柄导航整体失效）；新增两个默认关闭的兼容开关 `[Compatibility] AutoResetSMP`（逐角色重置 Faster HDT-SMP）与 `SceneOutfitSwitching`（SexLab / OStim 场景自动换 `_sex` 套装），设置页 Mod compatibility 可勾选；词典 226 → 233 条，请同时更新 `Data\interface\translations\` 下两份语言文件。
15. **1.5.1** 建议整包覆盖（脚本 / ESP / 旧存档都不用换）：这是一次纯性能与健壮性更新——菜单列表按 revision 缓存、护甲搜索缓存、槽位/策略缓存、短时间持锁、设置保存节流；同时修复了重命名/改收藏后列表可能不刷新、页码越界、热键在文本框松开后卡住、快速选择窗口跨线程刷新等问题。中文翻译中的“套装”统一改为“幻化”，请同时更新两份语言文件（仍 233 条）。
16. **1.5.2** 建议整包覆盖：这是第二轮无人值守审计，重点修线程边界与边角崩溃——AutoSwitch / 热键改为主线程排队，输入 sink 不再读 ImGui；菜单增加玩家与显示名空值防护；状态勾选/状态套装修改后立即应用当前状态；删除套装即时失效快选缓存；快选/Outfits 页改页大小后即使不打开 Settings 也会落盘；修复 OStim 与 SexLab 场景重叠、搜索字符串分配、页内 vector 与 label 复制等性能点。翻译仍 237 条（1.5.3 起），无需替换语言文件。

17. **1.5.3** 建议整包覆盖：第三轮无人值守审计，修 Papyrus `DeleteOutfit`/`OverwriteOutfit` 后不刷新外观、只有状态套装时战斗/死亡/地点不切换、仅收藏且唯一收藏就是当前套装时随机无反应、坏 co-save / 坏 JSON 抛异常、金件集合并发读；同时移除 `/MP` 解决大工程页面文件不足的 C1076/C3859。词典去重后 **237 条**，请同时更新两份语言文件（若不替换，新增的 12 条界面文案会回退成英文）。
18. **1.5.4** 修复快速选择窗口让手柄导航全局失效的问题（**1.5.6 更正**：实机确认 1.5.4 + SMF 3.9 仍失效，升级 3.18 或 3.23 后恢复；3.18 已正常，3.23 不是分水岭。3.9 的窗口手柄处理是主因；1.5.4 的 NoNavInputs 修改保留但不是充分修复）：1.5.0 起快选窗带有 ImGuiWindowFlags_NoNavInputs，ImGui 1.90 会因此把整个上下文的 io.NavActive 置为 false，连 SMF 主菜单也收不到手柄。1.5.4 去掉该窗口标志，改为在快选窗正文前后压入 ImGuiItemFlags_NoNav，只让快选窗内的控件不参与 ImGui 导航。OStim / SexLab 场景换装本身未改；日志会以 info 级别打印期望的 `当前套装名_sex`。

19. **1.5.5** 兼容性收尾：实机确认 SKSE Menu Framework 3.9 升级后手柄恢复（3.18 与 3.23 均正常），1.5.4 的代码无需改动；1.5.5 额外把 BlockUserInput 显式置 true，并加入 RegisterInpoutEvent 原始手柄兜底（1.5.6 起只在框架低于 3.18 时启用，3.18+ 不注册，避免吞掉框架自己的事件）。

20. **1.5.6** 更正版本门控：用户实测 **SMF 3.18 下 1.5.4 的快速选择窗口手柄已正常，3.23 不是分水岭**；上游 3.14.0 引入 GamepadNavigation 并修复 gamepad 不可用的回归。1.5.6 把 raw fallback 阈值从 3.23 下调到 3.18：3.18+ 交给框架自身翻译，只有 3.9 等更旧版本才注册 raw 回调。**升级建议：SMF 3.18+ 与 1.5.6；如果手柄仍异常，先看日志里的 Menu: SKSE Menu Framework <版本>。**

21. **1.5.7** 建议整包覆盖：快速选择窗口新增 **X 键取消幻化**（键盘 `X` / 手柄 `X`，与 Remove outfit 按钮同一个 helper）；修复带 `.tri` 形变的幻化装备在 OBody、RaceMenu 滑块或游戏内换身形预设后不实时刷新的问题——新增 `MorphRefresh` 轮询 RaceMenu `BodyMorph` 签名，变化时强制清空幻化槽并重建 partClone，再延迟 `ApplyBodyMorphs`。未装 RaceMenu 不受影响。两项均待实机验证。

22. **1.5.8** 修复套装编辑器右侧 Body slots 的“移除”按钮点击无反应：确认条原来固定画在编辑器顶部，滚到槽位列表时看不到；现在改为在被点击行内显示“移除 / 取消”，并把按钮位置按可视宽度 clamp，长护甲名不会再把它挤出子窗口。删除成功会写 `Outfit editor: removed ...` 的 info 日志。

23. **1.5.9** 修复 OBody 换身体预设后只有带身体参照的 32 槽上衣刷新、裤子等其他幻化件仍穿旧模型导致穿模：强制重建范围从 applySet 扩大到整个 displaySet 的并集，真实穿戴但属于显示集的件也会 ClearBipedPart 后重新 ApplyArmorAddon。开 Verbose logging 可看 `Custom: force-rebuilding ...`。

24. **1.5.10** 修复无 RaceMenu 环境下 OBody 换预设后整套幻化仍不刷新：`MorphRefresh` 增加 OBody 4.4.0+ 插件 API 后端，轮询 `GetPresetAssignedToActor` 的预设名签名；OBody-only 时先 `ApplyOBodyMorphsToActor` 再触发 displaySet 全量重建。日志应出现 `MorphRefresh: OBody plugin API v1 acquired` 与 `MorphRefresh: OBody is ready`；**仍需 OBody 4.4.0+**，未实机验证。

25. **1.5.11** 修复 OBody 换身体预设后衣服与身体模型整体消失（只剩头部/手）：强制重建原来把第一、第三人称两套 biped 都清空，第二次 Custom 又把第一次刚重建的那套清掉；现在只清当前 Custom 对应的 ActorWeightModel。日志开 Verbose logging 仍应看到两条 `Custom: force-rebuilding ...`，但外观应保持完整；未实机验证。

26. **1.5.12** 修复 OBody 换预设后只有上衣跟随新身形、裤子/鞋子/头发/发饰仍穿模：签名变化后除立即重建，还会在 **500 ms 后**再重建一次，等 OBody 的 `ApplyOBodyMorphsToActor` 真正生效；OBody-only 时重建后也会重放整套 morph。顺带修好 1.5.97 上 RaceMenu 应答偏晚的问题——RaceMenu 接口现在每 2 秒重试，最多 30 次。开 Verbose logging 可看 `MorphRefresh: rebuilding the illusion of ... after the body morph provider settled.`；未实机验证。

27. **1.5.13** 修复「OBody 换预设后仍穿模、连按 Z 重穿当前套装也不刷新」：`Custom` 现在也看 `OutfitService` 的 revision —— 套装被新建/编辑/改派后 2 秒内一定全量重建（清槽 + 重贴 displaySet），所以 Z 键、快选窗按 E 都能真正刷新；OBody 的延迟重建改为 400 ms + 1400 ms 两段，强制窗口 4 秒。另加 debug 诊断 `Custom: <Armor> slot .. clone 0x...`，可直接看出槽位几何有没有换。

28. **1.5.14** 修复「换身形后幻化件仍穿模 / 按 Z 重穿也不刷新」：指派类接口补 `++_revision`（1.5.13 里 `outfit revision` 恒为 false），并且 `MorphRefresh` 现在会自动执行「先摘掉幻化、250 ms 后再穿上」的循环——也就是玩家此前必须手动做的「取消幻化再穿回」，因而 `.tri` 形变克隆会在新身形落定后重建。
29. **1.5.15**：幻化循环不再闪出原版装备——摘掉与穿回放进同一个主线程任务，中间不渲染任何一帧；按 Z 重穿当前套装即时生效。若某机型仍需间隔，用 `[Compatibility] ReillusionGapMs`（默认 0）。

30. **1.5.16**：Fantasy 栏（Outfits）底部新增 **Copy** / **Paste**，整套预设（名字、基础策略、32 槽单槽策略、全部部件）走系统剪贴板，格式与 `OutfitSystemData.json` 同结构；粘贴时重名自动加 `_copy` / `_copy2`，完成后左侧列表自动选中并滚到新套装。

31. **1.5.17**：兼容 **SLIF NG**（BodyInflationNG.dll，Nexus 192276）等膨胀框架 —— 新增它的只读查询后端（`GetModuleHandleA` + `GetProcAddress("SLIFNG_GetQueryInterface")`，纯 C 导出，绕开 SKSE 的 receiver 名字错位问题），每 250 ms 读 4 个规范膨胀键（belly / breast / butt / scrotum）的 `GetApplied` 并入形变签名；同时给 RaceMenu 与 OBody 的消息握手加了无名广播兜底（1.5.97 上 skee 常因名字错位收不到命名消息，日志里的 `Failed to dispatch message to skee` 就是它）。签名需静置 250 ms、同一角色两次幻化循环至少隔 600 ms，膨胀这种连续变化不会把画面刷爆。

32. **1.5.18**：修三条身形症状 —— ①RaceMenu 拖 Body Morph 滑块 / ②OBody NG 换预设 / ③SLIF 膨胀后，幻化件都要跟着变形。新增**与插件无关的几何签名**（直接采样两套 `BipedAnim` 的 partClone 顶点缓冲做 FNV），所以即使 RaceMenu 的 skee 接口一次都没接上（1.5.97 上常见）也能跟随；SLIF 的 10 个 morph 滑块（`PregnancyBelly` 等）也并入签名。同时把 1.5.13 遗留的 400 ms + 1400 ms 两轮冗余重建合并成一次 700 ms，OBody 换预设的刷新次数从 4 次降到 3 次（不再那么卡）；循环结束后有 2.5 秒静默窗口，不会自己把自己再触发一轮。未实机验证。
33. **1.5.19**：三条身形症状的二次修复 —— skee 握手**永不放弃**（心跳 5 秒 ×8 次后每 30 秒一直试到会话结束，`kPostLoadGame` 与每次真正要用接口前再试；失败时打印 skee64.dll 是否在进程内，区分「没装」与「装了不答」）、几何探针改为**递归采样** `partClone` 子树（1.5.18 只测 partClone 本身，容器节点全部落空 → 探针恒 0）、变化先交给 RaceMenu/OBody **直接刷一次**（provider-first，350 ms 后校验，最多再等 4 × 350 ms），成功时完全不摘幻化（不露真甲、不卡），未动才回退幻化循环；删除 1.5.18 的 700 ms 二次重建。新 ini 键 `[Compatibility] MorphProviderFirst = 1`。

34. **1.5.20**：幻化件真正跟随身形（m00002 / m00270）。真因：幻化 clone 由插件直接 `ApplyArmorAddon` 重建，**不经过 skee 的 `AttachBipedObject` 钩子**，节点上没有引擎生成的 `BODYTRI`，而 skee 的 `MorphCache::ApplyMorphs()` 找不到 `BODYTRI` 就整棵子树跳过 → RaceMenu / OBody / SLIF 写多少遍都写不到幻化件，1.5.19 的 provider-first 因此只能回退幻化循环（631 ms 露原版）。修法：新增 MorphPush 子模块 —— 重建后按 ARMA `bipedModels[性别].GetModel()` 推导 `.tri` 路径（去 `.nif`、先试去 `_0`/`_1`、`BSResourceNiBinaryStream` 实测存在、按 FormID+性别缓存）并给槽节点挂 `NiStringExtraData("BODYTRI")`（value 用引擎堆 `RE::malloc`）；订阅 skee v5 `AddMorphShapeCallback` 与 OBody `Obody_ApplyMorph`，回调里只标脏（`ApplyMorphs` 持锁，重入即死锁），主线程再 `ApplyVertexDiff(…, false)` 幂等重放；新增 `AbsorbOwnWrite()` 把自家写入吸收成新基线并开 1.2 s 静默窗，防止轮询误判又去摘幻化；skee `IBodyMorphInterface` 声明补齐到 v5 全 23 槽（原先少 `rootNode` 形参、`AddMorphShapeCallback` 会落到错误槽位），`GetVersion() < 5` 不装回调，握手依次试 `skee64` → `skee` → 广播。

35. **1.5.21**：修 1.5.20 的「幻化全没了、只剩裸体」（m00746）。日志里 `CustomSkinPlayer` 站点（SE `24231 + 0x81` = rva `0x364301`）的字节是 `E8 E2 CD C7 FF`，解出的目标在模块外 —— 这份 exe 该函数内部布局与记录偏移不同，钩子被安全检查禁用；而 `DontVanillaSkinPlayer` 仍在藏真护甲，幻化件又只能由 `CustomSkinPlayer::Custom()` 贴上去，于是全裸。修法：`ResolveSite` 支持自定位（±0x800 内唯一 `E8` 到 `ExecuteVisitorOnWorn` → 采用并记 warn；窗口内没有但全 `.text` 唯一调用点 → 采用；多候选拒绝并留证），只有 `CustomSkinPlayer` 传窗口；新增幻化总开关 `SetIllusionDisplayEnabled`（`atomic_bool`，fail-closed），两个皮肤钩子没都装上就关掉幻化、`ShouldOverrideSkinning` 恒 false，原版流程照跑 → 看到**真实护甲**而不是裸体，`critical` 一行说明；`ShouldOverrideSkinning` 在 `skyrim_cast` 前用 `VirtualQuery`（`IsReadableObject`）挡坏指针；装钩子前打印 `SkyrimSE.exe` 版本/大小/写入时间/`base`/`.text` 范围，失败时打印站点 ±0x10 字节窗口与全 `.text` 调用点统计。

36. **1.5.22**：修「1.5.21 装上后幻化依旧失败」（m01170），并把钩子层彻底改成与运行时无关。根因：五个站点偏移全部抄自上游 1.5.73 的注释，而 Address Library 只保证**函数入口**；`24231` 在 1.5.97 的入口是 rva `0x364280`，下一条地址库条目只比它大 `~0xD0`，`+0x81` 落在函数中部却是垃圾字节。改法：新增 `Hooking::FindCallInFunction(hostId, targetId)` —— 在宿主函数入口后 `0x800` 字节内逐字节扫 `E8`，`rel32` 精确等于目标（或经一跳 thunk 到达）即候选；`SiteOffset()` 与所有偏移常量删除。壳代码去掉寄存器假设（`ShimWornFlags` / `CustomSkinPlayer` 只保存 `rcx`，actor 由 `inventory->owner` 取；`DontVanillaSkinPlayer` 的 `r13` 与 `FixEquipConflictCheck` 的 `rdi`/`rbx` 属调用约定固定事实，保留）。`CustomSkinPlayer` 扫不到站点时不再关闭幻化：`MarkAttachPending` 按 FormID 100 ms 节流入队，`SKSE::GetTaskInterface()->AddTask` 在主线程 `DrainAttachPending()` → `Custom(actor, nullptr)`（对 `{false,true}` 两套 biped 各贴一次）；总开关只要求 `DontVanillaSkinPlayer`。仍是同一份 `Hooks.cpp` + 单张 `REL::RelocationID` 表，`REL::Module::IsAE()` 只出现在注入机器码本身不同处。
37. **1.5.23**：修「1.5.20 起底层幻化乱套」并恢复 1.5.17 的行为（m01836 / m01837）。用户实测 1.5.17 全功能正常、只有 `.tri` BodyMorphs 不跟随，说明 1.5.20 新增的「站点 `E8` 目标必须等于 `InventoryChanges::ExecuteVisitorOnWorn`」校验把 1.5.17 一直可用的成员调用判死了（那处 `E8 E2 CD C7 FF` 解出的目标在模块外）。修法：新增 `Hooking::FindCallInFunctionOrOffset` —— 先按 1.5.22 扫描，0 命中时回退到 `entry + legacyOffset`（SE `0x81` / AE `0x1EF`，1.0.0 起的历史站点），仅当 `InText(site,5)` 且首字节 `E8` 才安装并 warn `the site this hook has used since 1.0.0 ...`；历史站点也不是 `E8` 时才放弃。只有 `CustomSkinPlayer` 用这个例外，其余四个钩子仍纯扫描。幻化总开关恢复成对判定（`DontVanilla && CustomSkinPlayer`），只装一半时整条幻化关闭并 critical 提示 —— 看到的是真实护甲而不是裸体。1.5.22 那条「display set is attached from the deferred pass」的 active 文案已删除。
38. **1.5.24**：修「1.5.23 装上后只显示头手、切套装时原装备 46 槽外露」（m02035）。真因一：1.5.20 起贴件入口只传 `InventoryChanges*`，模型用 `reinterpret_cast<RE::ActorWeightModel*>(&actor->GetBiped(fp))` 造出来，而 `RE::ActorWeightModel` 在 CommonLibSSE-NG 里只有前向声明 —— `ApplyArmorAddon` 全部拒绝，日志是五条 `slot NN clone 0x0`。真因二：`fullRebuild` 先 `ClearSlots(displayMask)` 再重贴整份 displaySet（含原版通道已贴好的真实装备件），旧槽离开 displaySet 时还「还原真甲」。修法：壳代码回到 1.5.17 的寄存器契约（`rbx` = 被蒙皮对象、`rdi` = 引擎的 `ActorWeightModel*`），只贴 displaySet 里未真实穿戴的件、不再清空重贴、离开 displaySet 的槽只清空不还原真甲；`ClearSlots`/`SlotClone` 改走 `ModelBiped()`（只接受该 actor 的两套 biped 之一），`ApplyArmorAddon` 被拒时打 5 秒节流 warn。
39. **1.5.25**：修「1.5.24 幻化衣服有了、身体没了」（m02242）。真因：1.5.24 为了不让「46 槽外露」，对离开幻化集的槽调用 `ClearSlots` → `RE::BipedAnimAugments::ClearBipedPart`，还把 `BipedAnim::objects[bit]` 的 `item` / `addon` / `part` 置空 —— 那是引擎给该槽维护的唯一条目（含引擎自己的皮肤/身体部件），清空后只要装备栏不变，引擎就不会重建，于是身体（CK 槽 32，掩码 `0x4`）永久空模型，脱 32 槽护甲、切空套装都救不回来。现在删除 `ClearSlots` / `SlotClone`：离开幻化集的槽只打 `Custom: N slot(s) left the display set (mask 0x…); left to the engine for …`，原样交回引擎；另外每个贴过的槽打一行 debug 状态 `Custom: … slot NN item … addon … part … clone …` 便于取证。

40. **1.5.26**：**【历史结论已纠正：握手见顶部；N/M 不充分，不代表实机修复】** 修「幻化件不跟随 RaceMenu 的 Body Morphs」（m00002 / m00270 / m02536）。1.5.20–1.5.25 把「先拿到 skee 的 `BodyMorph` 接口」当成前提（`BakeIllusion()` 开头 `if (!g_api || ...) return kSkipped;`），而用户机器的 `skse64.log` 证明该接口整局都拿不到：我们的 `0x9E3779B9` 广播恒投给 24/35/39/45/66/91/95/140 这 8 个「在 `kPostLoad` 投递循环中间才注册 catch-all」的插件，skee(132) 与 OStim(99) 是加载期注册的，永远不在收件集里（定时器任务里发的也一样）。修法：给每个幻化克隆件**无条件**贴 `.tri` 目标 —— skee 每次 `ApplyBodyMorphs`（RaceMenu 拖滑块、OBody 换预设都会调）都会遍历每个 biped 槽的 `data.object` 并在子树里找 `NiStringExtraData "BODYTRI"`，RaceMenu 自己就会 morph 我们的克隆；接口降级为加分项（即时 `ApplyVertexDiff` 重放 + `MorphShapeCallback`），握手改由 `QueueProviderRetry()` 在主线程任务里发（不再在皮肤钩子/消息回调里同步 `Dispatch`）；几何兜底探针新增 `HashSkinnedShape()` 读 `NiSkinPartition` 的 `rawVertexData`（此前只认 `BSDynamicTriShape`，蒙皮网格根本采不到）。新日志：`MorphRefresh: <FormID> carries a .tri morph target on N of M cloned slot(s) … no interface needed.`（每角色一次）。
41. **1.5.27**：**【历史结论已纠正：握手见顶部；N/M 不充分，不代表实机修复】** 修「1.5.26 装上一个 `.tri` 目标都没贴上」（m02992）。日志里 `MorphRefresh: … has no BodyMorph interface; 0 .tri target(s) of 5 slot(s) stay armed …` 出现 5 次、整局 0 条 `carries a .tri morph target on`。真因是路径推导用了 `armor->GetArmorAddon(race)`：它只回答「ARMA 的 race/additionalRaces 是否命中该种族」（`TESObjectARMA::IsValidRace`），自定义种族/套装未列种族/引擎走默认种族回退时返回 null ⇒ 路径为空 ⇒ 连贴都不贴；而同一份日志的逐槽转储里 `addon` 全部非 0，说明引擎自己解析得好好的。修法：①`BodyTriPathFor(armor, addon, race, female)` 优先用 `biped->objects[i].addon`，`GetArmorAddon(race)` 仅兜底、查不到时不写缓存；②`TriCandidates()` 一次试四个名字；③`.nif` 自带的 `BODYTRI` 也计入目标；④每个护甲/性别一条 debug（`uses .tri <file> (from the armour addon the engine attached)` / `has no .tri beside it … Probed: a, b, c, d.`）；⑤新增 `g_armed`：克隆件已武装时不再 `CycleIllusion`（消除 1.5.26 日志里 5 次 `taking the illusion off for one engine pass` 的闪原版）。新日志：`… carries a .tri morph target on N of M cloned slot(s)` 里 `N` 应等于幻化覆盖槽数。

42. **1.5.28**：修正 1.5.27 的双 meshes 前缀探测与 BODYTRI 误计数，分离实际 addon/sex/1P/3P 缓存，保留有效 biped 回调、仅 selfWrite 防重入，provider 前采几何签名，自动刷新改为 RequestBake（不摘穿）。尚未实机；详见顶部最新说明。

43. **1.5.29**：修"场景结束后幻化件残留、要再按一次键才刷新"（m05502）。1.5.25 起把所有"离开幻化集的槽"都只打 `left to the engine`，但当引擎那边没有真甲可重建（场景套装、非穿戴幻化件）时，我们上一套的克隆就没人拆——场景结束回普通幻化会停在旧外观。现在：①`Patches.cpp` 用 `AppliedState { displayMask; ownedMask; }` 记录"哪些槽里的克隆是我们放的、且没有真实穿戴物打底"，离开幻化集时只在"我们放的 + 无人真实穿戴 + 非皮肤槽 + 槽里仍有 `partClone`"四条同时成立才拆（`Custom: detached N illusion clone(s) whose slot left the display set …`），其余仍交回引擎；②`MorphRefresh.cpp` 主线程新增幻化几何对账（`IllusionMissingSlots` → `refreshArmorFor` 重建，300 ms 退避、最多 3 次），对账只认"幻化集说有、biped 槽里没有"，不认任何框架名字。基础幻化钩子与 1.5.25 的槽策略未动。尚未实机验证。

44. **1.5.30**：修「OStim/SexLab 场景里切 `_sex` 套装不显示，必须按一次 Tab/Z 才更新」与「场景结束回普通幻化不刷新」（m06712），**（其补跑修法已被 1.5.32 删除：实机无效）** 并按 m06781 把模块文件名改为 **`OutfitSystemNG.dll`**。真因不是调度器坏了，而是「幻化集已切换」不等于「渲染模型已更新」：一次装备 pass 只把 `addon`/`part` 写进 biped 槽，真正被渲染的克隆几何由紧随其后的 pass 物化 —— 菜单路径之所以灵，是开关菜单让引擎连跑十几轮 pass；脚本路径一次 pass 之后就没有后续 pass 了。修法：①`MorphRefresh::RequestOutfitPasses(actor)` 在每次切换后补跑 120 / 400 / 900 ms 三次装备 pass（主线程 `DrainOutfitPasses()`，幻化总开关关闭时不跑）；②`Features::RefreshActor()` 末尾统一调用它，`SceneOutfits` 两处改走 `Features::RefreshActor(player)`，与「点列表」共用同一条序列；③模块改名为 `OutfitSystemNG.dll`，插件身份（`Plugin::NAME`、日志文件名 `SkyrimOutfitSystemNG.log`、Papyrus 翻译前缀、存档标识）一律不变，**旧名 `SkyrimOutfitSystemNG.dll` 必须删除**（否则双加载）。基础幻化钩子、贴件语义、槽清理策略均未改动。尚未实机验证。

45. **1.5.31**：修「场景开始 / 场景结束都必须按一次 Tab/Z 才显示幻化」（m07050）。真因是 1.5.30 只重复写 biped 条目，从未让引擎重建被绘制的 3D 模型；**（其重建修法已被 1.5.32 删除：实机无效）** 新增 `OutfitSystem::rebuildActorModel()`（`Actor::Update3DModel()`）挂在随访链第 2 步，并把 `SceneOutfits` 的场景开始/结束改成调用 `Features::AssignOutfit()`（与「点套装列表」同一条函数）。钩子层、贴件语义、槽清理、`.tri` 推导均未改动。

46. **1.5.32**：场景套装回到「找对名字 → 切套装」（m07382 / m07424）。**删除** 1.5.30 的三次补跑装备 pass 与 1.5.31 的 `rebuildActorModel()`（两者实机均无效），场景开始/结束只调 `Features::AssignOutfit()`（= 点套装列表那条函数），事件改为游戏线程同步执行（与按地点/战斗切换同构），名字查找先试 `<当前套装>_sex` 再全名忽略大小写匹配；`_sex` 套装不再有任何专属槽位/时机规则。尚未实机验证。

47. **1.5.33**：场景套装回退到 1.5.29 的写法，并改成「按地点/状态切换」那套换装（m07666）。**回退** `src` 到 `v1.5.29`，撤掉 1.5.30 的补跑 pass、1.5.31 的 `rebuildActorModel()`、1.5.32 的 `Features::AssignOutfit()` 场景入口；**新增** `OutfitSystem::setOutfitByName()`，场景开始/结束都走「锁内指派 → 锁外 `refreshArmorFor()`」，与 `AutoSwitch::Evaluate()` 同构；**删除**场景专用的 SKSE 任务队列、`Tick()` 心跳与 `gc` 状态，事件改为 `ModCallbackEvent` sink 里同步执行。`AutoSwitch`（按地点/状态切换）本版一行未改。详见顶部最新说明与 `docs/DELIVERY.md` §4.61。

48. **1.5.34**：修「OStim 场景开始后幻化压根没切换」（m07842）。1.5.33 的 `OnGameThread()` 门禁把工作线程送来的场景事件整条丢弃（日志：三条 `arrived off the game thread; the scene switch is skipped.`）→ 本版改为「一次 `AddTask` 交接到游戏线程 → 统一 `Dispatch()` 入口」。换装序列、钩子层、贴件语义、槽清理、`.tri` 推导、DLL 文件名一律未改。


## 常见问题

**施放 “Change Outfit” 法术没反应？** 先确认 Scripts 目录里有 SkyrimOutfitSystemNativeFuncs.pex（1.0.1 及更早漏了它，见 docs/CHANGELOG.md）。缺这个文件时 Papyrus.0.log 会出现 “Cannot open store for class “SkyrimOutfitSystemNativeFuncs””。另外快速选择窗口需要 SKSE Menu Framework；没装时法术会弹 HUD 提示而不是静默失败。

**施放 “Change Outfit” 直接随机换装，我想要列表？** 1.0.3 起默认就是列表（快速选择窗口）。若你的 ini 里还有旧行为，检查 `[RandomOutfit] QuickSlotRandom`：off = 弹窗，on = 随机；也可以在菜单 Settings → Random outfit 里改。菜单里找不到设置项的话，说明 DLL 还是旧版（Settings 页应有 Hotkeys 与 Random outfit 两段）。

**NPC / 其它 mod 还是看不穿我的幻化？** 1.1.0 起不再重绑 Papyrus 的 `Actor.GetWornForm` / `WornHasKeyword`（感知功能已移除），改为针对具体 mod 的数据级兼容：

- **Advanced Nudity Detection**：写它的裸体/上身/下身三个阵营。装好后日志应有一行 `Compatibility: Advanced Nudity Detection 4.2.0.0 detected; nudity, topless and bottomless ranks now follow the displayed outfit.`，AND 自己的状态显示与依赖它的对话条件随之变化。若阵营 EditorID 被改，日志会变成 `its nudity factions are missing`。
- **Procedural Armor Damage**：靠真实护甲被发现，幻化件本身撕不到（它不在背包里）。开 `ExtraLogging` 后 `PAD compatibility:` 行会指出“有幻化但下面没有真实护甲”的槽位。
- **引擎内部的 TESCondition**（原版 NPC 台词、守卫盘问这类）读的是真实装备的 `ExtraWorn` 标记，条件函数表地址不在 Address Library 里，插件无法安全改写——幻化成空套装时引擎仍认为你穿着衣服。这是外观替换技术的边界（衣服没真脱），只能靠具体 mod 自己的接口绕过。
- 直接调 `Actor.GetWornArmor` / `GetInventory` 的 DLL 插件同样看不到幻化（CommonLibSSE-NG 里这些是内联实现，没有可挂钩的入口）。

**翻译文件放哪、怎么加一种语言？** `Data\interface\translations\` 下，文件名必须是 `SkyrimOutfitSystemNG_<language>.txt`（`<language>` 就是语言下拉里显示的名字，如 `english`、`chinese`、`german`）。复制 `_english.txt` 改名为 `_german.txt`、把 TAB 右边改成德语，重启游戏即可在下拉里看到 German；不需要改 ini、不需要编译。文件必须存成 UTF-16LE 带 BOM 或 UTF-8，行格式 `$English source text<TAB>译文`。

**快速选择窗口标题写着 “Debug”，而且位置/大小记不住？** 1.4.1 及更早的版本用 `igGetCurrentWindow() != nullptr` 判断“框架已开窗”，但 ImGui 每帧都会隐式 `Begin("Debug##Default")`，这个判断恒为真 —— 于是内容全画进那个隐式窗口（标题就是 Debug），`[UI] QuickPickerX/Y/Width/Height` 也从未被应用。1.4.2 改为由快速选择窗口自己 `Begin/End`，并用 `ImGuiCond_Always` 套用 ini 里的矩形：现在拖动/缩放后关闭会记住，重启游戏也保持；直接改 ini 的数字同样生效。日志会打印 `Quick picker: opening at x,y with wxh (restored from the ini|centred).`

**导入 OutfitSystemData.json 后策略变成“从不显示任何东西”？** 1.4.1 及更早只认四字符码；如果你的文件里写的是本地化标签（如“需要已装备（槽位有装备时才显示套装）”），认不出的值会让该槽位什么都不显示。1.4.2 的 `NormalizePolicyCode()` 同时接受四字符码、翻译键、英文标签与新旧中文标签。**导出格式**：1.4.3 起写数字枚举（与原版 `OutfitSystemData.json` 一致），1.4.1/1.4.2 曾写成四字符码字符串。若仍是旧 DLL，请升级后再导入。

**老存档里的槽位策略变成「始终使用套装」了？** 这是 1.4.0 及更早版本的迁移 bug（v4 co-save 的槽位策略是数字枚举，之前没解析，全部落回默认 `XXOO`）。1.4.1 已按原版 `Policy::from_int` 映射还原（0=XXXX、1=XXXE、2=XXXO、3=XXOX、4=XXOE、5=XXOO、6=XEXX、7=XEXE、8=XEXO、9=XEOX、10=XEOE、11=XEOO，≥128 为 `CustomPolicyNNN`）。已用旧版本迁移过一次的存档，重新导入旧 co-save（或用原版 `OutfitSystemData.json` 导入）即可恢复。

**菜单是英文的，能切中文吗？** 能：Settings → Language 选「中文」（或 ini `[UI] Language = zh`）。前提是菜单框架本身能画中文——`Data/SKSE/Plugins/SKSEMenuFramework.ini` 里 `[Fonts] EnableChinese = true`，并且它指定的字体（默认 `CN.ttf`）真的在 `Data/SKSE/Plugins/Fonts/` 里。不满足时插件会保持英文并在日志写明缺什么（`Language: Chinese was requested but the menu framework has no CJK font loaded`），不会画出一屏方块。

**PAD 菜单里的 “Tracked armors” 是空的 / 挨打幻化衣服没变化？** “Tracked armors” 只列已经产生过损伤记录的护甲副本，空列表 = 还没撕过任何东西。真正决定能不能撕的是「这个槽位底下有没有真实护甲」：幻化件只在背包里不存在，PAD 必须有一个真实护甲条目来存损伤。点 Settings → Mod compatibility → **Log slot report**，日志里每个槽位会写成 `shown 'X' | real 'Y'`；出现 `real '<none>'` 的槽位就是 PAD 摸不到的地方（例如只幻化了一件衣服、身上没穿任何真实胸甲）。先在那些槽位穿上真实护甲，PAD 就会正常撕。

## 已知限制

- 本机没有安装 Skyrim，所有游戏内行为**没有实机验证**；静态验证项见 docs/DELIVERY.md。
- 旧 co-save（原版 protobuf 记录，版本 4/5/6）会在读档时自动迁移为 JSON（版本 7）；版本 3 及更早不支持。
- RemoveActor 拒绝移除玩家。
- VR 未构建。

## 许可

沿用原项目 CC BY-NC-SA 4.0，见 LICENSE.md。
