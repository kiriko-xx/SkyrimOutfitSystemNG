# 架构说明

本文描述 Skyrim Outfit System NG 的模块划分、数据模型、幻化数据流与生命周期。阅读前请先看 README。

## 1.5.28 当前 morph 架构与边界

**验证状态（1.5.28）**：尚未实机验证，不能声称已实机修好。生产函数路径回归 6/6 通过；最终 Release BUILD_EXIT=0，DLL 版本/哈希与交付副本一致。包内逐文件清单见 SHA256SUMS.txt；最终 zip 大小、哈希及解包复核记录见包外 VERSIONS.md 与同名 .zip.sha256，避免自引用。

- **修正资源路径**：1.5.27 日志里的模型路径已经带 `meshes` 前缀，`TriFileExists()` 又加一次，实际探测成 `meshes\meshes\...`；1.5.28 规范化模型/TRI 路径，资源打开路径只保留一次 `meshes` 前缀。
- **收紧 BODYTRI 诊断**：不再只按 extra data 名称匹配；必须是 `NiStringExtraData`、路径非空且对应资源可打开才计数。日志改为 `has readable BODYTRI resources on N of M...`，并明确 `paths only, not proof of TRI shape matching or morph movement`。N/M 只是路径资源可读性，不是 TRI shape 名匹配、顶点移动或实时跟随的成功证据；即使 N=M 也不能宣告修好。
- **分离模型缓存**：以实际 addon（ARMA）+ sex + 1P/3P 区分缓存；1P 选择 `bipedModel1stPersons`，3P 使用第三人称模型，避免多 ARMA、性别与视角之间串用路径。
- **保留有效回调**：去掉会误吞所有 biped 回调的 `IsIllusionClone` 过滤，只用 `selfWrite` 防止自家写入重入。provider 调用前采集几何签名，再作调用后比较，避免把已更新的状态当基线。
- **自动刷新不摘幻化**：自动 morph 刷新不再 `CycleIllusion` 摘掉再穿回，而排队 `RequestBake`；不改基础幻化钩子、不清身体槽，也不以重新穿脱作为自动兜底。
- **共享 extra data 安全性**：规范化已有 BODYTRI 时，不原位改写共享字符串；为该节点创建新的 `NiStringExtraData` 替换，持有 `NiPointer` 保留原对象，替换失败时恢复。`GetBodyMorphs` 等未使用虚槽的声明签名纠正，虚表槽顺序不变。
- **能力边界**：主机制仍依赖 RaceMenu 自己的 morph 遍历能访问克隆件、TRI shape 能匹配。拿到且通过版本门禁的接口时才可额外重放；无接口重放不能保证实时，轮询/路径可读日志都不能替代实机顶点与外观验证。

### sender 订阅与接口可用性

SKSE64 2.0.20 的 `RegisterListener(nullptr)` 只订阅注册当时已有的 sender 列表（含当时预留槽），不是对未来 sender 的持续订阅；skee 的 `[Features] bEnableEarlyRegistration=1` 若使其过早注册，可能漏掉后加载的 SOS。`QueueProviderRetry()` 只重发消息，不能补建 skee 对 SOS 的订阅关系。公开配置修法：检查**实际游戏生效的** `Data/SKSE/Plugins/skee64.ini`，若 `[Features] bEnableEarlyRegistration=1`，由用户改为 `0` 并完全退出、重启游戏；不自动修改，也不打包用户 ini。当前没有本次 `skse64.log`，不能断言本次配置必为 `1`。协议依据：[SKSE v2.0.20 PluginManager.cpp L488–616](https://github.com/ianpatt/skse64/blob/v2.0.20/skse64/PluginManager.cpp#L488-L616)。

> **1.5.28 纠正（历史说明，不作当前验收依据）**：旧文档将握手归因于“仅消息嵌套”/receiver 名字错位的解释已纠正，实际是注册时机导致 sender 订阅遗漏；主线程 Queue 重试并非完整修法。旧 `carries a .tri morph target on N of M`、`N > 0` 或 `N = M` 判定均不充分，不能证明 TRI shape 匹配或 morph 顶点移动；旧实时成功措辞是当时目标/假设而非实机结论。当前以 1.5.28 最新章节为准。

以下版本演进描述保留历史，不代表当前仍使用清槽/幻化循环或旧的握手解释。

## 1. 目标与约束

- 在**原 Skyrim Outfit System SE 0.7.0 的代码基础上**升级到 CommonLibSSE-NG，同时支持 SE 1.5.97 与 AE 1.6.1170+。
- 幻化（把已穿护甲的外观替换成套装件）的引擎介入点、策略语义、Papyrus API 名称全部保持不变。
- 原项目的 Rust 数据层（cxx + tokio + protobuf + Svelte Web UI）在本机没有 Rust 工具链、且 cxx 桥接与新版 CommonLibSSE 的 ABI 风险过高，因此**等价改写为 C++**（src/core），对外行为对齐。
- 任何引擎钩子在地址库缺失或站点异常时必须**只禁用自己**，不能让游戏崩。

## 2. 与原项目的关系

| 部分 | 处理 |
| --- | --- |
| src/cpp/OutfitSystem.cpp（Papyrus 层 + 事件 + 护甲搜索/槽位策略包装） | **保留**，仅改 NG 不兼容的少量 API 调用 |
| src/cpp/hooking/{Patches.cpp, Hooks.cpp}（xbyak 钩子） | **保留**，两份 SE/AE 平行实现合并为单套 `REL::RelocationID` 表，补安全预检与惰性地址解析；1.5.22 起站点不再用字节偏移，改为「地址库定函数 + 函数体内扫描 `E8`」 |
| src/cpp/RE/REAugments.cpp（引擎函数封装） | **保留**，vtable 定义对齐 NG |
| src/cpp/{main.cpp, Utility.cpp} | 重写入口与路径工具以适配 NG |
| src/rust/**（cxx crate，约 100 KB Rust） | **替换**为 src/core/**（C++） |
| src/web/**（Svelte Web UI）+ [Server] 配置段 | **移除**，由游戏内菜单取代 |
| src/papyrus/skyoutsysmcm.psc（SkyUI MCM，48 KB） | **替换**为空壳存根，设置迁到菜单框架 |
| 其余 psc / ESP / 翻译 | 保留，快捷槽脚本重写以去掉 UIExtensions 依赖 |

## 3. 模块总览

```
              +------------------------- SKSE plugin entry -------------------------+
              |  src/cpp/main.cpp: SKSE::Init -> Settings::Load -> InitializeLog   |
              |  -> PapyrusInterface / MessagingInterface / SerializationInterface |
              +--------------------------------+-----------------------------------+
                                               | kDataLoaded
            +----------------------------------+-------------------------------------+
            |                    |                  |                |                |
   hooking/Hooks.cpp    OutfitSystem.cpp      compat/*.cpp       NGHotkeys.cpp    MenuUI.cpp
   (xbyak, 5 hooks)     (Papyrus layer)       (mod compat)    (DIK hotkey)     (SMF section)
            |                    |                  |                |                |
            +--------------------+---------+--------+----------------+----------------+
                                               |
                                        src/core (data layer)
                     OutfitService / Outfit / Policy / Settings / Bindings
```

| 模块 | 职责 |
| --- | --- |
| src/cpp/main.cpp | SKSE 入口、日志、trampoline 分配、按运行时选钩子、消息/存档回调、Papyrus 与事件注册 |
| src/cpp/OutfitSystem.cpp | Papyrus 函数实现（66 个注册名）、战斗/死亡事件、护甲搜索与槽位策略包装 |
| src/cpp/hooking/Patches.cpp | 五个钩子的策略实现：跳过原版贴图、伪造 worn 掩码、应用幻化件、修复装备冲突与技能升级；1.5.21 起带**幻化总开关** `SetIllusionDisplayEnabled` / `IllusionDisplayEnabled`（`atomic_bool`，默认 false，`ShouldOverrideSkinning` 开头即查），并在 `skyrim_cast` 前用 `IsReadableObject`（`VirtualQuery`）挡住坏指针。1.5.22：`OverrideWornFlags` 改为单参（`InventoryChanges*` → `owner` 取 actor，非幻化时回原版掩码）、新增 `CustomFromInventory` 与 `Custom(actor, nullptr)`（对 `{false,true}` 两套 biped 各贴一次）、`MarkAttachPending`/`DrainAttachPending` 延迟附加队列（FormID 100 ms 节流 → `SKSE::GetTaskInterface()->AddTask`）；总开关只要求 `DontVanillaSkinPlayer` |
| src/cpp/hooking/Hooks.cpp | 单套钩子实现：所有地址用 `REL::RelocationID(seID, aeID)` 声明，站点由 `FindCallInFunction` 在宿主函数体内扫描；`SiteOffset()` 已删除，代码里没有任何字节偏移。只有机器码本身不同的两处按 `REL::Module::IsAE()` 分支（`DontVanillaSkinPlayer` 的 actor 在 `r13`、`FixSkillLeveling` 的 AE 内联路径）。`ShimWornFlags` / `CustomSkinPlayer` 的壳代码只碰 `rcx` |
| src/cpp/GameState.{h,cpp} | 读档闸门：`IsLoading()` 期间幻化钩子退回原版路径，避免读档过程中重刷外观 |
| src/cpp/hooking/HookSupport.cpp | `IdExists` / `RequireIds` / **`FindCallInFunction(host, target)`**（1.5.22：宿主函数入口后 `kBodyScanLimit = 0x800` 内扫 `E8`，`rel32` 等于目标或经一跳 thunk 到达即候选；1 条采用、多条钩第一条并 warn、0 条报错并列出全 `.text` 调用点）+ 诊断（`LogModuleIdentity` 记录 exe 版本/大小/写入时间/`base`/`.text` 范围、`LogCodeWindow`、`LogSiteBytes`）。`ResolveSite` 与 1.5.21 的 ±窗口自定位仍在，只留给非「调用某函数」的站点（`_DEBUG` 的 RTTIPrinter） |
| src/cpp/RE/REAugments.cpp | REL::Relocation 包装（ApplyArmorAddon、ExecuteVisitorOnWorn、GetWornMask…） |
| src/cpp/NGFeatures.cpp | 随机换装、Actor 外观刷新、套装列表/搜索、Papyrus 新函数包装 |
| src/cpp/compat/DisplaySet.{h,cpp} | 按当前幻化算出“显示集”（1.1.0 起取代原 Perception 的这部分计算），供兼容层判定覆盖情况 |
| src/cpp/compat/AndCompat.{h,cpp} | Advanced Nudity Detection：按 EditorID 解析它的三个裸体阵营，按幻化覆盖写阵营等级 |
| src/cpp/compat/PadCompat.{h,cpp} | Procedural Armor Damage：检测与诊断（`Diagnose` 逐槽报告 + 孤儿槽位掩码变化时才记日志），不改 PAD |
| src/cpp/compat/ModCompat.{h,cpp} | 兼容层统一入口：Install / Reload / Tick（1 秒节流心跳）/ OnOutfitApplied / GetStatus |
| src/cpp/NGHotkeys.cpp | DIK 扫描码组合键，边沿触发：Ctrl+Shift+O 随机换装、Ctrl+Shift+L 快速选择窗口 |
| src/cpp/AutoSwitch.{h,cpp} | 原生地点/状态自动切换：`TESActorLocationChangeEvent` + `TESFastTravelEndEvent` 两个事件 sink，外加输入心跳 2 秒兜底；按 actor 缓存上次状态，只有状态变化才重算，见 §8.1 |
| src/cpp/SMP.{h,cpp} | 逐角色 SMP 物理重置：`[Compatibility] AutoResetSMP` 开启时，`refreshArmorFor()` 末尾通过 FSMP 的 `DynamicHDT.ResetPhysics(actor, false)` 只刷新目标 actor，见 §8.2 |
| src/cpp/SceneOutfits.{h,cpp} | SexLab / OStim 场景套装：监听 `ModCallbackEvent`，按“当前套装名 + _sex”自动换装并在场景结束恢复，见 §8.3 |
| src/cpp/MorphRefresh.{h,cpp} | `.tri` 形变实时刷新：运行期取 RaceMenu `BodyMorph` v4+ 和/或 OBody 4.4.0+ 插件 API，250 ms 轮询已幻化 actor 的 morph/预设名签名；变化时走幻化循环（1.5.15 起同帧完成）并开 2 秒强制重建窗口；`RefreshActor()`；1.5.9 起按 displaySet 并集清槽并重贴全部显示件；1.5.10 起无 RaceMenu 时走 OBody 后端，重建前调 `ApplyOBodyMorphsToActor`；1.5.11 起 `ClearSlots` 只清当前 `Custom` 的 `ActorWeightModel`；1.5.12 起签名变化后再排一次 500 ms 延迟重建（等 OBody 稳定），RaceMenu 接口每 2 秒重试最多 30 次；1.5.17 起新增 SLIF NG（BodyInflationNG.dll）只读查询后端与 skee/OBody 无名广播兜底；1.5.18 起再加**与 provider 无关的几何签名**（两套 `BipedAnim` 的 `partClone` 动态三角形顶点缓冲采样 FNV），SLIF 10 个 morph 滑块也并入签名，400/1400 ms 两轮冗余重建合并为单次 700 ms，循环后置 2.5 秒 quiet 窗口，见 §8.7 ；1.5.19 起：skee 握手**永不放弃**（心跳前 8 次每 5 秒、之后每 30 秒直到会话结束，`kPostLoadGame` 再试，每次真正要用接口前惰性再试；失败诊断探测 skee64/skee/skeevr 是否在进程）、几何探针改为 `HashSubtree()` 递归 `partClone` 子树（深度 4、每槽 24 个形状）、签名变化先走 **provider-first**（`ApplyBodyMorphs` / `ApplyOBodyMorphsToActor` 各一次，350 ms 后校验，最多 4 次，未动才回退幻化循环），并删除 700 ms 二次重建（`kSettleRebuildDelay` / `g_pendingRebuild` 已移除） | **1.5.20 起改为推送优先**：重建后 `Patches.cpp` 调 `MorphRefresh::NotifySlotApplied()`，MorphPush 子模块给幻化槽补 `BODYTRI`（按 ARMA 模型路径推导 `.tri`），并订阅 skee v5 `AddMorphShapeCallback` 与 OBody `Obody_ApplyMorph`，在主线程用 `ApplyVertexDiff(…, false)` 幂等重放（不重建、不摘幻化），轮询降级为安全网。
| src/cpp/MenuUI.cpp | SKSE Menu Framework 的四页 UI + 独立快速选择窗口（搜索 / 仅收藏 / 分页 / 自绘 `Begin`/`End` + 居中默认尺寸 / W-S-A-D 与手柄十字导航 / E-R-Esc 键位 / X 取消幻化（键盘与手柄同键） / 焦点记忆 / 取消幻化按钮）；Outfits 页左列表右编辑器，列表宽度可调且带横向滚动，工具条有 `Apply to` 目标选择（对应原版 MCM 的 actor 选择） |
| src/cpp/OutfitClipboard.{h,cpp} | 复制 / 粘贴整套预设（1.5.16）：用 Win32 `CF_UNICODETEXT` 读写系统剪贴板，JSON 结构与 `OutfitSystemData.json` 的单个 outfit 完全一致（名字 / isFavorite / armors{rawFormId,localFormId,modName} / slotPolicies / slotPolicy），策略写原版数字枚举；粘贴时重名自动加 `_copy` / `_copy2`，经 `OutfitService::create_outfit_preset()` 一次加锁建好整套 |
| src/cpp/MenuOutfitEditor.{h,cpp} | 套装编辑器（1.3.0）：四种添加方式、槽位冲突确认、槽位清单、blanket + 32 槽逐槽策略；逐控件与原版 MCM 的对照见 `docs/MCM_PARITY.md` |
| src/cpp/I18n.{h,cpp} | 翻译层：内置词典（248 条）+ **外置可编辑文件** `Data\interface\translations\SkyrimOutfitSystemNG_<language>.txt`（每语言一份，Skyrim 传统 `$英文<TAB>译文`，UTF-16LE/UTF-8，缺失回退内置词典再回退英文）、`Tr`/`TrId`（`中文###English` 保控件 id）、`ChineseAvailable`（读 SMF 的 `[Fonts]` 与字体文件，不注入字体）、`[UI] Language` = auto / 语言文件名后缀、`ReloadFile()`、`Languages()`（扫描目录得出可选语言）。详见 `docs/I18N.md` |
| src/cpp/MenuBridge.cpp | 运行期定位菜单框架模块并取导出函数；3.x 的 `BlockUserInput` 显式置 true，旧框架可注册原始输入回调 |
| src/cpp/ImGuiApi.cpp | 运行期绑定 cimgui 的 32 个必需 `ig*` 符号（缺一即禁用菜单）+ 14 个可选符号（窗口/按键/尺寸，缺失时快速选择窗口退化为无导航的普通窗口） |
| src/core/Outfit.{h,cpp} | 单个套装：护甲集合、收藏、32 槽策略 + blanket、幻化集合计算 |
| src/core/OutfitService.{h,cpp} | 全局单例：套装表、actor 指派、状态换装（含 `<random>` 哨兵与同状态不重抽）、序列化；1.5.16 起 `create_outfit_preset()` 一次加锁建整套（先重置 32 槽与基础策略再套用来源值） |
| src/core/Policy.{h,cpp} | 12 个内建 4 字符策略的求值 |
| src/core/Settings.{h,cpp} | ini 读取与黄金件解析 |
| src/core/Bindings.cpp | 自由函数：list_available_policies_c、translation_key_c、自然排序 |
| src/core/LegacySave.cpp | 原版 protobuf co-save（v4/v5/v6）解码与迁移，手写 wire format 读取器 |

## 3.1 并发与读档闸门

- `OutfitService` 内部持有一把 `std::recursive_mutex`，外部通过 `outfit_service_get_singleton_ptr()` / `outfit_service_get_mut_singleton_ptr()` 取得 RAII 守卫。**守卫的构造函数加锁、析构函数必须解锁**（1.0.1 修的就是这里漏了解锁导致的载入卡死）。
- 服务层不保证线程安全以外的顺序性：Papyrus 调用在主线程，幻化钩子在渲染线程，因此所有跨线程访问都必须经过守卫。
- `SOS::GameState::IsLoading()` 在读档窗口内为真（`kPreLoadGame` / `kNewGame` / Revert 回调置真，`kDataLoaded` / `kPostLoadGame` 置假）。置真期间：钩子的 `ShouldOverrideSkinning` 一律 false，`refreshArmorFor` 对没有 3D 的 actor 直接跳过，兼容层的 `Sync` 也整体跳过（`DisplaySet::OverrideActive` 第一道闸门就是读档中）。

## 4. 数据模型

```
Outfit
  name            : 大小写不敏感（内部以小写作 key，保留原始写法）
  armors          : set<const TESObjectARMO*>
  favorite        : bool
  slotPolicies[32]: optional<4 字符策略码>   // 空 = 继承 blanket
  blanketPolicy   : 4 字符策略码（默认 XXOO）

ActorAssignments
  current         : optional<套装名>        // 空 = 该 actor 不做幻化
  autoSwitch      : bool                    // 状态自动换装开关
  stateBased[13]  : optional<套装名>        // 索引 = StateType 数值
```

StateType 数值与原版 Papyrus 一致（MCM/脚本依赖，不可改）：
`World=0, Town=1, Dungeon=2, WorldSnowy=3, TownSnowy=4, DungeonSnowy=5, WorldRainy=6, TownRainy=7, DungeonRainy=8, City=9, CitySnowy=10, CityRainy=11, Combat=12`。

### 4.1 策略（Policy）

4 字符码，位置含义：第 1 位=没穿也没配，第 2 位=只穿了，第 3 位=只配了，第 4 位=两者都有。字符 `E`=显示已穿件，`O`=显示套装件，`X`=不显示。

| 码 | 排序 | 高级 | 含义 |
| --- | --- | --- | --- |
| XXXX | 100 | 是 | 永不显示 |
| XXXE | 101 | 是 | 只显示已穿 |
| XXXO | 2 | 否 | 只显示套装（Require equipped 的反面） |
| XXOX | 102 | 是 | |
| XXOE | 103 | 是 | |
| XXOO | 1 | 否 | 默认：有套装就整套显示套装 |
| XEXX | 104 | 是 | |
| XEXE | 105 | 是 | |
| XEXO | 3 | 否 | 盾牌默认：已穿优先，否则套装 |
| XEOX | 106 | 是 | |
| XEOE | 107 | 是 | |
| XEOO | 108 | 是 | |

求值见 src/core/Policy.cpp 的 `EvaluatePolicy`；槽位级联在 `Outfit::compute_display_set_c`。

## 5. 幻化数据流

玩家穿戴/刷新时，引擎按下列顺序被拦截（细节见 HOOKS.md）：

1. **ShimWornFlags**：`InventoryChanges::GetWornMask` 被替换——返回真实掩码 ∪ 套装件的槽位掩码，使后续遍历认为这些槽已有内容（否则会漏贴模型）。
2. **DontVanillaSkinPlayer**：`TESObjectARMO::ApplyArmorAddon` 的调用点被拦截——若当前护甲不在幻化集合里，跳过原调用，避免真实护甲贴图覆盖幻化件。
3. **CustomSkinPlayer**：`InventoryChanges::ExecuteVisitorOnWorn` 返回后，把套装中**本人没穿**的件通过 `ApplyArmorAddon` 真正贴到 biped 上（`BipedAnim::objects[]`）。贴完把被改写的 `BIPOBJECT.item` **还原成真实护甲**（诚实还原，见 HOOKS.md §3.3）：外观在 `addon`/`partClone`，`item` 记录保持真实，读取 biped 的 mod（PAD 等）才认这些槽。
4. **FixEquipConflictCheck**：装备冲突检测遍历时跳过幻化件，避免“戴第二顶帽子会卸掉第一顶”。
5. **FixSkillLeveling**：技能升级按真实护甲的轻重甲类型计数，而不是幻化件，避免穿重甲练轻甲。

`compute_display_set_c(equipped)` 是唯一决定“显示哪些件”的地方：

```
mask = 0; result = {}
for slot in 0..31:
    if mask & (1 << slot): continue        # 已被更靠前的件覆盖
    code = slotPolicies[slot] or blanket
    if code 未知: continue
    choice = EvaluatePolicy(code, equipped[slot], outfit[slot])
    if equipped[slot] 是黄金件: choice = equipped[slot]
    if choice: result += choice; mask |= choice->GetSlotMask()
```

## 6. 第三方 mod 兼容（零 hook）

1.1.0 删掉了 1.0.2/1.0.3 的 Papyrus 重绑（`Actor.GetWornForm` / `WornHasKeyword`）：实机日志显示判定方一次都没走 Papyrus 入口，它们读的是引擎 TESCondition（真实装备的 `ExtraWorn` 标记，条件函数表不在 Address Library），重绑覆盖不到。

现在改为**只用对方公开数据契约**的定点兼容，`src/cpp/compat/`：

| mod | 契约 | 我们做什么 |
| --- | --- | --- |
| Procedural Armor Damage | 背包里 `IsWorn` 的真实护甲 + 场景里 `BIPOBJECT.item` 指向同一件 | 什么都不做（`CustomSkinPlayer` 的诚实还原满足门禁——**1.4.0 才真正实现**，1.3.0 及更早只是文档这么写）；只在掩码变化时记一条“幻化件下面没有真实护甲”的 INFO，并提供 `Diagnose` 逐槽报告 |
| Advanced Nudity Detection | `AND_NudeActorFaction` / `AND_ToplessFaction` / `AND_BottomlessFaction` 阵营等级 | 用 `Actor::AddToFaction` 按幻化覆盖写等级（1 = 空） |

- 兼容层不 hook、不打补丁、不读对方内部结构：PAD 一行代码不碰，AND 只写它对外发布的阵营。对方升级只要契约（DLL 名、阵营 EditorID）不变就继续有效。
- 防漂移：AND 自己会重扫真实装备覆盖等级，因此 ① `OnOutfitApplied`（每次幻化刷新末尾）立即同步；② `ModCompat::Tick()` 由输入事件驱动、节流 1 秒，`SKSE::GetTaskInterface()->AddTask` 回主线程后补写一次。
- 近似性：AND 的覆盖判定只看槽位掩码（`Body(2)`、`ModChestPrimary(16)/ModChestSecondary(26)` 为上装；`Body`、`ModPelvisPrimary(19)/ModPelvisSecondary(22)/ModLegRight(23)/ModLegLeft(24)` 为下装），不按它的关键词做精判。
- 引擎自身的对话/台词条件仍不受影响（读真实装备的 `ExtraWorn`），这是外观替换技术的边界；想影响它们只能像 AND 这样针对具体 mod 的对外数据做兼容。
## 7. UI（SKSE Menu Framework）

- 菜单框架是**可选**依赖：`MenuBridge::EnsureLoaded()` 运行期按模块目录 + 游戏根目录查找 `SKSEMenuFramework.dll`，成功才 `AddSectionItem`。
- 不使用链接期符号、不使用延迟导入：`ImGuiApi::Bind()` 用 `GetProcAddress` 取 32 个 `ig*`（cimgui C 接口）函数指针，全部命中才启用 UI。
- 页面：Quick picker（独立窗口的镜像页）、Outfits（搜索/收藏/分页/应用/新建/重命名/删除）、Actors（指派、状态换装、刷新外观）、Settings（开关、热键、随机参数、语言与界面尺寸）。
- 快速选择窗口由 `MenuBridge::AddWindow` 注册，框架调用渲染函数时**不提供** `Begin`/`End`，所以窗口自己画（`IG::BeginWindow/EndWindow`，默认 640×800、`FirstUseEver` 居中）。3.18 及以上由框架把键盘/手柄翻译成 `ImGuiKey_*`（用户实测 3.18 与 3.23 均正常，3.23 不是分水岭）；3.9 等旧版对自定义窗口不转发手柄，1.5.6 在 `FrameworkVersion() < 3.18` 时通过 `RegisterInpoutEvent` 注册原始回调，直接把 `RE::ButtonEvent` 的十字键/A/Y/B 写进原子状态供渲染线程消费（3.18+ 不注册，避免吞掉框架事件）。上游 3.14.0 引入 `GamepadNavigation` 并修复 gamepad 不可用的回归。
- 翻译：所有界面文案经 `IG::` 包装走 `I18n::Tr`，可被外置 txt 覆盖；左侧树节点标题在注册时生成，切语言需重启（`Register()` 里用 `I18n::Tr` 拼路径）。
- 1000+ 套装的性能策略：只在“名字数量 / 搜索串 / 收藏过滤”签名变化时重建过滤结果；绘制只遍历当前页（默认 50 条）。

## 8. 输入

`NGHotkeys` 注册 `BSTEventSink<RE::InputEvent*>`，注意 NG 的签名是 `ProcessEvent(RE::InputEvent* const* a_event, BSTEventSource<RE::InputEvent*>*)`（二级指针）。按键用 DIK 扫描码（`BSKeyboardDevice::Key`），组合键用 `GetAsyncKeyState` 查修饰键，按下沿只触发一次（按住不会连发）。

### 8.1 地点/状态自动切换（1.4.4 起原生）

- 事件：`ScriptEventSourceHolder` 上的 `TESActorLocationChangeEvent` 与 `TESFastTravelEndEvent`；每个事件只评估 `should_override()` 且 `stateBasedAutoSwitchingEnabled` 的 actor。
- 兜底：`NGHotkeys` 的输入 sink 每次事件调 `AutoSwitch::Tick()`，内部 2 秒节流遍历已指派 actor，用于天气变化（雪/雨）或漏事件。
- 去抖：`AutoSwitch` 与每个 `ActorAssignments::lastState` 两层缓存，状态未变直接返回，避免每帧重刷、也避免战斗事件反复重抽 `<random>`。
- 旧 Papyrus 脚本 `SkyOutSysAutoSwitchTrigger.psc` 1.4.4 起为空实现（保留 alias 兼容旧存档/ESP），不再有 `Utility.Wait(10.0)` 轮询。

### 8.2 切换幻化时按角色重置 SMP（1.5.0）

- 开关：`[Compatibility] AutoResetSMP`，默认 off；设置页 Mod compatibility 同时显示 FSMP 是否就绪。
- 触发点：`OutfitSystem::refreshArmorFor(actor)` 的最后一步，覆盖手动换装、状态切换、场景套装、Papyrus 刷新等所有路径。
- 实现：`SOS::SMP::ResetActor(actor)` 用 `ActorHandle` 把弱引用带到 `SKSE::GetTaskInterface()->AddTask` 的主线程任务；确认 `Is3DLoaded()` 后调用 `RE::MakeFunctionArguments(static_cast<RE::Actor*>(actor), false)` + `IVirtualMachine::DispatchStaticCall("DynamicHDT", "ResetPhysics", ...)`。`false` 对应 FSMP 的 `softReloadMeshes()`。
- 只影响目标：FSMP 的 `ResetPhysicsImpl` 按 `skeleton->GetUserData()->formID` 匹配单个 actor，不存在全局遍历。
- 降级：VM 不存在或 `TypeIsValid("DynamicHDT")` 为假时只写一次 debug 日志并跳过；没有 FSMP 不影响幻化。

### 8.3 SexLab / OStim 场景套装（1.5.0）

- 开关：`[Compatibility] SceneOutfitSwitching`，默认 off。
- 事件：`SKSE::GetModCallbackEventSource()` 的 `ModCallbackEvent`；`ostim_start` / `ostim_end` 与 `PlayerAnimationStart` / `PlayerAnimationEnd`（只认 SexLab 的 Player 线程）。
- 开始：当前套装名已以 `_sex` 结尾（忽略大小写）→ 不变；否则 `candidate = 当前名 + "_sex"`，`OutfitService::has_outfit()` 命中才换；找不到保持。
- 结束：仅当玩家仍穿着这次场景换上的套装时才恢复 `previous`；玩家中途手动换装则不覆盖。
- 状态：`g_active/g_previousOutfit/g_sceneOutfit` 只在内存中，读档/新游戏时 `Reset()` 清空，不写入 co-save。

### 8.4 菜单与幻化热路径缓存（1.5.1）

- **Outfit**：槽位缓存 _outfitBySlot（护甲增删置脏）与策略缓存 _resolvedPolicies / _resolvedBlanket（策略修改置脏）；compute_display_set_c 不再每次遍历护甲集合或线性查找 32 次策略表。
- **OutfitService**：原子 _revision，所有会改变套装集合、收藏、名称或策略的写操作递增；all_outfit_names() / get_outfit_names() / outfit_count() 内部加锁并返回副本。
- **MenuUI**：OutfitNamesCache() 按 revision 缓存；RefreshList / RefreshQuickList 先比 revision + 搜索词 + 收藏签名，未变直接返回；Outfits / Actors 面板用短读快照 + 短写操作，不整帧持锁。
- **MenuOutfitEditor**：完整护甲列表缓存按 TESDataHandler 指针 + GetFormArray(FormType::Armor) 的 size 与 data 指针失效；重建时清搜索签名。
- **并发**：Papyrus/输入线程不直接改 UI 状态；快速选择窗口的待刷新标志由渲染线程 exchange 后执行；随机数用 thread_local。
- 缓存只由 OutfitService 的 recursive_mutex 保护下的调用方访问，皮肤钩子、菜单与 Papyrus 不会并发写同一份缓存。

### 8.5 1.5.2 线程边界与缓存补充

- **输入线程只排队**：`AutoSwitch::Tick` 只做节流 + `g_tickPending.exchange` + `SKSE::GetTaskInterface()->AddTask`；真正的 `TickOnMainThread` 在主线程访问 actor/OutfitService。随机/快选热键的 `Fire` 同样排队，避免在 `BSInputDeviceManager` 的 sink 里碰 actor 数据。
- **文本输入采样**：`NGHotkeys::SetTextInputActive` 由菜单渲染 thunk 在渲染线程调用，记录时间戳；输入线程只读 `CachedTextInputActive()`（500ms 内有效），因此热键不会读 ImGui 的线程本地状态。
- **守卫零分配**：`OutfitServiceReadGuard/WriteGuard` 为栈上返回值，构造加锁、析构解锁；`operator->` 返回 `this`，调用点仍写 `service->inner()`。
- **皮肤热路径**：`DisplaySet` 的 `WornArmorVisitor` 引用调用方 vector 并 `reserve(32)`；`ComputeDisplaySet` 用 `thread_local` 复用一个 worn 缓冲，避免每次钩子分配。
- **菜单缓存**：Outfits / Quick picker 过滤用 `ContainsNoCase` 做无分配大小写折叠；Outfits 页复用页内 `pageFavorites/pageArmorCounts` vector；编辑器 `GetArmorList` 对背包/穿戴列表做 500ms TTL 缓存。
- **一致性**：`check_consistency` 在插入玩家或清理非法 FormID 后 `++_revision`；删除套装同时失效 `g_filterSignature` / `g_quickSignature` / `g_filtered`。

### 8.6 1.5.3 边界补充

- Papyrus 的 DeleteOutfit / OverwriteOutfit 走服务层整体替换（`replace_outfit_armors`，递增 revision）并排队刷新，避免第三方脚本绕过菜单后外观残留。
- 状态切换资格统一为 `enabled && stateBasedAutoSwitchingEnabled && (should_override || has_any_state_outfit)`；AutoSwitch、TESCombatEvent、TESDeathEvent 共用同一语义。
- co-save v7 与 OutfitSystemData.json 的反序列化体包 try/catch，字段类型错误只记日志并返回失败。
- `_resolvedGolden` 用 shared_ptr 快照 + atomic_load/store，金件判定读路径无锁。

### 8.7 `.tri` 形变刷新演进（1.5.7 起，历史记录）

> **1.5.28 纠正（历史说明，不作当前验收依据）**：旧文档将握手归因于“仅消息嵌套”/receiver 名字错位的解释已纠正，实际是注册时机导致 sender 订阅遗漏；主线程 Queue 重试并非完整修法。旧 `carries a .tri morph target on N of M`、`N > 0` 或 `N = M` 判定均不充分，不能证明 TRI shape 匹配或 morph 顶点移动；旧实时成功措辞是当时目标/假设而非实机结论。当前以 1.5.28 最新章节为准。

带 `.tri` 的服装（OBody、游戏内实时换身形预设等）在 morph 变化后，幻化件的 `partClone` 仍是旧顶点缓冲。`ApplyArmorAddon` 对同一 `item` 会复用已有槽位，不会因为 `RefreshActor()` 就重建，所以旧版只能重新穿脱或先取消幻化。

1. `MorphRefresh::Install()` 先尝试 RaceMenu：`Dispatch(0x9E3779B9, ..., "skee")` 取 `BodyMorph` 接口，`GetVersion() >= 4` 才启用；再尝试 OBody：向 receiver `OBody` 发送 `RequestPluginInterface`（`0xc0B0D9cc`，v1），用静态 `IOBodyReadinessEventListener` 跟踪 ready。两者都不可用才停用。
2. `Tick()` 由输入心跳调用（250 ms 节流），把轮询排到主线程；对每个已幻化、3D 已加载的 actor 用 `VisitMorphValues` 算 FNV 顺序无关签名。
3. 签名变化：RaceMenu 混入 `VisitMorphValues` 的 FNV 签名；OBody 混入 `GetPresetAssignedToActor` 的预设名 FNV 签名。记录 `g_forced[FormID] = now + 2s` 并 `Features::RefreshActor()`；OBody-only（无 RaceMenu）时先调用 `ApplyOBodyMorphsToActor`。皮肤钩子 `CustomSkinPlayer::Custom` 查到标记后，先按 displaySet 的并集槽位调用 `BipedAnimAugments::ClearBipedPart`（15496/15661）并清空 `item/addon/part`，再对 displaySet 全部件 `ApplyArmorAddon`（1.5.9 起包含真实穿戴件），强制重建 partClone 读取当前形变。
4. 重建完 `NotifyRebuilt()` 排一个 150 ms 的任务调用 morph pass：有 RaceMenu 时 `ApplyBodyMorphs(actor, true)`，OBody-only 时 `ApplyOBodyMorphsToActor`（1.5.12 起不再要求 RaceMenu），覆盖 partClone 由任务池延迟挂载的时序。
5. 1.5.12 起，签名变化除立即 `RefreshActor()` 外再排一次 **500 ms 延迟重建**（`g_pendingRebuild` / `g_rebuildScheduled` 去重，仍受 2 秒强制窗口保护）：OBody 会在装备变化后重新派生预设形态，我们的重建本身就像一次装备变化，必须等它稳定后重贴克隆。`Tick()` 同时每 2 秒重试一次 RaceMenu 接口（最多 30 次），成功后切回 RaceMenu 权威路径。
6. 1.5.17 起：`MorphRefresh::Install()` 除 RaceMenu / OBody 外再尝试 SLIF NG —— `GetModuleHandleA("BodyInflationNG.dll")` + `GetProcAddress("SLIFNG_GetQueryInterface")` 直接拿 `IQueryInterface1*`（纯 C 导出，不经过 SKSE 消息，因此不受 receiver 名字错位影响；拿不到再退回 `0x534C4946 'SLIF'` 消息握手）。签名里并入该 actor 的 4 个规范膨胀键（`slif_belly` / `slif_breast` / `slif_butt` / `slif_scrotum`）的 `GetApplied`，所以 SLIF 的 morph 路径与骨架节点缩放路径都会触发幻化重建。同时 RaceMenu 与 OBody 的消息握手在命名 dispatch 失败后再发一次 receiver = `nullptr` 的广播（SKSE 2.1.2+ 把 receiver 名解析成「注册了消息监听器的插件」列表下标，某个 DLL 加载失败会导致整体错位）。签名必须静置 250 ms、同角色两次循环至少间隔 600 ms，避免连续值 ramp 把画面刷爆。
7. 1.5.18 起不再依赖任何 provider：`ComputeGeometrySignature()` 遍历 `{false, true}` 两套 `actor->GetBiped()` 的 `objects[i].partClone`（`i < BIPED_OBJECTS::kTotal`，单槽 16 个 shape 上限），`AsDynamicTriShape()` → `GetDynamicTrishapeRuntimeData()`，要求 `dynamicData` 非空且 `dataSize >= 64`，对头 1024 字节逐字节 + `i += 251` 稀疏采样 + `dataSize` 做 FNV，再按 `0x9E3779B97F4A7C15 * (i + 1)` 混合槽位序号 —— 任何改写顶点缓冲的插件都会让签名变化。SLIF 侧除 4 个规范键再读 10 个滑块（`PregnancyBelly` / `Breasts` / `Butt` 等）的 `GetCombinedMorph` 与 `GetApplied("morph:<slider>")`。冗余的 400/1400 ms 两轮重建删除，改单次 `kSettleRebuildDelay = 700 ms`；循环结束后重算签名作基线并置 `kPostCycleQuiet = 2500 ms` 静默窗口，`kSignatureSettle` 350 ms、`kMinCycleInterval` 1200 ms。


1.5.12/1.5.13 起：签名变化后除了立即重建，还会在 400 ms 与 1400 ms 各重建一次（`g_pendingRebuild` / `g_pendingRebuild2`），强制窗口延长到 4 秒（1.5.18 起这两轮合并为单次 700 ms 的 `kSettleRebuildDelay`，`g_pendingRebuild2` / `g_rebuildScheduled2` 已删除）；这是因为 OBody 的 `ApplyBodyMorphs` 排队生效，并且会被我们自己的装备变化再次派生。
8. 1.5.19：上面第 7 条有两个失效点，实测（m21255 的日志）直接表现为「三条症状一条没解决」。其一，探针只对 `partClone` 本身调 `AsDynamicTriShape()`，而它多数是容器节点（NiNode / add-on node / skin wrapper），41 个槽全落空 → 签名里从来没有几何项；改为 `HashSubtree()` 递归（深度 4、每槽 24 个形状），子节点里的 `BSDynamicTriShape` 才能被采到。其二，provider 拿不到时的兜底太贵：现在签名变化先调一次 `ApplyBodyMorphs(actor, true)`（RaceMenu，延迟提交）或 `ApplyOBodyMorphsToActor(actor)`（OBody，排队执行），350 ms 后比对签名，最多再等 4 × 350 ms；只有仍不动才回退 `CycleIllusion`（引擎重载 `.tri` 目标的唯一路径）。成功路径完全不摘幻化，日志 `the provider pass moved the meshes of <FormID>; the illusion followed without a cycle.`。握手侧同时改掉「12 次 × 5 秒、从 kDataLoaded 起算」的定时器（进游戏前就耗尽）：`RetryProviders()` 无上限，heartbeat（5 秒 ×8 → 30 秒）、`kPostLoadGame`、每次使用前惰性重试（2.5 秒间隔）三处都会调；`ReportRaceMenuSilence()` 每 5 分钟最多报一次并打印 skee 模块是否在进程内。

9. 1.5.20：第 8 条仍然解决不了「幻化件不动」，因为问题不在探针而在**幻化 clone 本身没有 `.tri` 目标**：`CustomSkinPlayer::Custom()` 用 `ClearBipedPart` + `TESObjectARMA::ApplyArmorAddon` 直接造 clone，绕过了 skee 的 `AttachBipedObject` 钩子（`SKEEHooks.cpp` → `InstallArmorAddonHook` → `ActorUpdateManager::OnAttach` → `BodyMorphInterface::OnAttach` → `ApplyVertexDiff`），所以引擎不会给它生成 `BODYTRI`；而 `MorphCache::ApplyMorphs()` 只在 `VisitObjects` 找到 `"BODYTRI"` 时才 `CreateTRIPath` + `CacheFile` 读 `.tri`，**找不到就整棵子树跳过**。修法见模块表与 `DELIVERY.md` §4.49：`MorphRefresh` 内新增匿名 namespace `morphPush`（`TriFileExists` / `DeriveTriPath` / `BodyTriPathFor` / `NodeHasBodyTri` / `AttachBodyTri` / `BakeIllusion` / `DrainPending` / `RequestBake` / `OnMorphShape` / `OBodyMorphSink` / `InstallPush` / `AbsorbOwnWrite`），全部走引擎堆与主线程任务，回调内只标脏。

10. 1.5.21：1.5.20 在用户环境（SE 1.5.97）直接变成**裸体**。日志唯一 error 是 `CustomSkinPlayer: the call at +0x81 targets 0x7FF66C1610E8, but 0x7FF66C3651D0 was expected`（`site rva 0x364301 -> E8 E2 CD C7 FF`），即该 exe 在 `24231 + 0x81` 处不是那条到 `InventoryChanges::ExecuteVisitorOnWorn` 的调用（`E8 E2 CD C7 FF` 解出模块外的 `base - 0x1EF18`）。原因不是地址库算错（ID 24231/15856 解出的 rva 与代码期望一致，同函数区另两个钩子也能对上），而是这份 exe 的函数内部布局与记录偏移不同。1.5.21 两层改动：①`ResolveSite` 加自定位（±0x800 唯一 `E8` → 采用并 warn；否则全 `.text` 唯一调用点兜底；多候选拒绝并留证），只有 `CustomSkinPlayer` 传窗口；②`Patches` 加幻化总开关，`DontVanillaSkinPlayer` 与 `CustomSkinPlayer` 必须成对装上，缺一个就把幻化整条链路关掉（`ShouldOverrideSkinning` 恒 false），宁可显示真实护甲也不裸体，同时 `critical` 一行说明；③`ShouldOverrideSkinning` 用 `VirtualQuery` 验指针，防自定位挑错调用点崩在 RTTI 上。详见 `DELIVERY.md` §4.50 与 `HOOKS.md` §1.2。
11. 1.5.22：1.5.21 的 ±0x800 自定位仍是在一个写死偏移周围找替身，被用户判定为「专门去钩 1597」（m01170）。根因是那些偏移全部抄自上游 1.5.73 的注释，而地址库只保证**函数入口**：`24231` 在 1.5.97 的入口是 rva `0x364280`，下一条地址库条目只比它大 `~0xD0`，`+0x81` 落在函数中部却是垃圾字节。改法：`HookSupport` 新增 `FindCallInFunction(hostId, targetId)` —— 在宿主函数入口后 `0x800` 字节内逐字节扫 `E8`，`rel32` 精确等于目标函数地址（或经一跳 thunk 到达）即候选，1 条采用、多条钩第一条并 warn、0 条报错并打印全 `.text` 调用统计；`SiteOffset()` 与所有偏移常量删除。壳代码同时去掉寄存器假设：`ShimWornFlags` / `CustomSkinPlayer` 只保存 `rcx`（`InventoryChanges*`），actor 由 `owner` 取。`CustomSkinPlayer` 扫不到站点时不再关闭幻化：`MarkAttachPending` 按 FormID 100 ms 节流入队，`SKSE::GetTaskInterface()->AddTask` 在主线程 `DrainAttachPending()` → `Custom(actor, nullptr)`（遍历 `{false,true}` 两套 biped）；总开关只要求 `DontVanillaSkinPlayer`。详见 `DELIVERY.md` §4.51 与 `HOOKS.md` §1.2 / §3.3。
12. 1.5.23：1.5.20 起 `CustomSkinPlayer` 新增的「`E8` 目标必须等于 `InventoryChanges::ExecuteVisitorOnWorn`」校验把 1.5.17 上一直可用的成员调用判死（该处 `E8 E2 CD C7 FF` 解出的目标在模块外），于是 1.5.20 裸体、1.5.21 幻化整体关闭、1.5.22 扫不到仍拒绝安装。用户实测 1.5.17 全功能正常（m01837），证明站点本身有效、错的只是目标假设。1.5.23 新增 `Hooking::FindCallInFunctionOrOffset`：先按 1.5.22 的 `FindCallInFunction` 扫描，0 命中时回退到 `entry + legacyOffset`（SE `0x81` / AE `0x1EF`，1.0.0 起的历史站点），仅当 `InText(site,5)` 且首字节 `E8` 才安装并 warn `the site this hook has used since 1.0.0 ...`；历史站点也不是 `E8` 时才 error 放弃。其余四个钩子仍纯扫描。`ApplyPlayerSkinningHooks` 的总开关恢复成对判定（`DontVanilla && CustomSkinPlayer`），只装一半时整条幻化关闭并 critical `Illusion display is off for this session ...`，避免「藏真甲、无人贴件」的裸体。详见 `DELIVERY.md` §4.52 / `HOOKS.md` §1.2、§2.3、§3.3。
13. 1.5.24：1.5.20–1.5.23 的贴件入口只传 `InventoryChanges*`，模型靠 `&actor->GetBiped(fp)` 强转成 `ActorWeightModel*` —— 该类型在 CommonLibSSE-NG 里只有前向声明，`ApplyArmorAddon` 读到错位字段，每件都是 `clone 0x0`（只显示头手）。1.5.24 把壳代码回到 1.5.17 的寄存器契约（`rbx` = 被蒙皮对象、`rdi` = 引擎的 `ActorWeightModel*`），并去掉「`ClearSlots(displayMask)` + 重贴整份 displaySet」与「离开 displaySet 的槽还原真甲」两条旧逻辑；`ModelBiped()` 只在模型第一个字段确实是该 actor 两套 biped 之一时才允许清理遗留槽。
14. 1.5.25：1.5.24 为修「46 槽外露」在 `CustomOne` 里对离开幻化集的槽调 `ClearSlots` → `RE::BipedAnimAugments::ClearBipedPart`，并把 `BipedAnim::objects[bit]` 的 `item`/`addon`/`part` 置空；而那是引擎给该槽维护的唯一条目（含引擎自己的皮肤部件），清空后只要装备栏不变，引擎就不会重建 → 身体（CK 槽 32，掩码 `0x4`）永久空模型（m02242）。本版删除 `ClearSlots`/`SlotClone`，残留槽只打 `Custom: N slot(s) left the display set (mask 0x…); left to the engine for …` 并原样交回引擎；另新增 `LogSlotState` 供「贴上了但看不见」取证。
15. 1.5.26：1.5.20–1.5.25 把「先拿到 skee 的 `BodyMorph` 接口」当成功能前提 —— `BakeIllusion()` 开头 `if (!g_api || !a_actor || !a_actor->Is3DLoaded()) return kSkipped;`，`NotifySlotApplied()` 也在 `!g_api` 时直接返回，于是接口拿不到就一个 `BODYTRI` 都不贴。而用户机器的 skse64.log 证明该接口整局都拿不到：我们的 `0x9E3779B9` 广播恒投给 24/35/39/45/66/91/95/140 这 8 个「在 `kPostLoad` 投递循环中间才注册 catch-all」的插件，skee（handle 132）与 OStim（99）是加载期注册的，永远不在收件集里（定时器任务里发的也一样 ⇒ 与线程/嵌套无关）。本版改为**无条件**给幻化克隆件贴 `.tri` 目标：skee 每次 `ApplyBodyMorphs` → `MorphCache::UpdateMorphs` 都会遍历每个 biped 槽的 `data.object` 并在子树里找 `NiStringExtraData "BODYTRI"`（`BodyMorphInterface.cpp` 690–741 行），RaceMenu 拖滑块与 OBody 换预设都走这条；接口降级为加分项（即时 `ApplyVertexDiff` 重放 + skee v5 `MorphShapeCallback`）。配套：握手改由 `QueueProviderRetry()`（`atomic_bool` 去重 + `SKSE::GetTaskInterface()->AddTask`）在主线程任务里发起，禁止在消息回调/皮肤钩子里同步 `Dispatch`；几何兜底探针新增 `HashSkinnedShape()` 读 `NiSkinPartition::partitions[0].buffData->rawVertexData`（`BSTriShape` 蒙皮网格此前完全采不到，导致 250 ms 轮询在拖滑块时从不触发）。

16. 1.5.27：第 15 条把 `.tri` 目标改成了「无条件贴」，但用户实测（m02992）日志里仍是 `0 .tri target(s) of 5 slot(s)`（5 次）—— 一整局一个 `BODYTRI` 都没贴上。真因在**路径推导**：`BodyTriPathFor()` 用 `armor->GetArmorAddon(race)` 取 ARMA，而 `TESObjectARMO::GetArmorAddon` 只回答「该 ARMA 的 `race` / `additionalRaces` 是否命中传入种族」（`TESObjectARMA::IsValidRace`），自定义种族、套装未列该种族、或引擎自己走默认种族回退时它返回 null，于是路径为空、连 `AttachBodyTri()` 都不调用。同一份日志的逐槽转储（`Custom: <armor> slot <nn> item <f> addon <f> part <n> clone <n>`，addon 全部非 0）证明引擎自己解析得好好的 —— `biped->objects[i].addon` 就是那份 ARMA。本版：①`BodyTriPathFor(armor, addon, race, female)` 优先用引擎已解析的 `addon`，`GetArmorAddon(race)` 仅兜底，查不到 addon 时不写缓存；②新增 `TriCandidates()` 一次生成 `<stem>.tri`（去 `_0`/`_1`）、`<stem>_0.tri`、`<stem>_1.tri`、原样名四个候选，本性别优先，逐个用 `BSResourceNiBinaryStream` 探测；③`NodeHasBodyTri()` 的计数移出「路径非空」分支（`.nif` 自带 `BODYTRI` 也算目标）；④每个护甲/性别首次一条 debug（`uses .tri <file> (from the armour addon the engine attached)` 或 `has no .tri beside it … Probed: …`）；⑤新增 `morphPush::g_armed`：`PollOnMainThread()` / `VerifyProviderPass()` 发现签名变化时，若该角色克隆件已武装就只重设基线、不再 `CycleIllusion`（消除 1.5.26 日志里 5 次 `taking the illusion off for one engine pass` 的闪原版）。

17. 1.5.29：第 14 条「离开幻化集的槽一律交回引擎」在引擎没有真甲可重建时会残留 —— 场景套装（OStim/SexLab）与非穿戴幻化件都属这种情况，表现为场景结束后外观仍是旧的一套、要再按一次键才刷新（m05502）。两处改动：①`Patches.cpp` 把 `g_appliedMasks`（uint32）升级为 `AppliedState { displayMask; ownedMask; }`，只有「上一轮我们自己放的（`previousOwned & flag`）+ 无人真实穿戴 + 非 `Actor::GetSkin()` 皮肤槽 + 槽里仍有 `partClone`」四条同时成立才 `ClearBipedPart`，其余仍 `left to the engine` —— 仍然绝不清空引擎维护的条目（守住第 14 条）；②`MorphRefresh.cpp` 主线程新增对账：`CustomSkinPlayer::IllusionMissingSlots(actor)` 只读查出「`displayMask` 说有、biped 槽里没有」的槽，`ReattachDroppedIllusion()` 用已验收的 `OutfitSystem::refreshArmorFor()` 重建，同角色 300 ms 退避、最多 3 次后放手。对账不认任何框架名字，只认插件自身状态（守卫测试 `tests/skinning_stale_slots.ps1` 强制这一点）。
18. 1.5.30：「幻化集已切换」不等于「渲染模型已更新」——一次装备 pass 只把 `addon`/`part` 写进 biped 槽（`Patches.cpp` 的 `CustomOne` 在 `ApplyArmorAddon` 返回后立刻打 `LogSlotState`，此时 `clone` 仍为 0），真正被渲染的克隆几何由紧随其后的 pass 物化；菜单路径之所以「一按就出来」是因为开关菜单让引擎连跑十几轮 pass，而脚本路径（`SceneOutfits` 场景开始/结束）一次 pass 之后没有任何后续 pass，于是切 `_sex` 不显示、场景结束回普通幻化也不更新（m06712）。本版新增 `MorphRefresh::RequestOutfitPasses(actor)`：登记 120 / 400 / 900 ms 三次随访，主线程 `PollOnMainThread()` 里的 `DrainOutfitPasses()` 到期调已验收的 `OutfitSystem::refreshArmorFor()`，日志 `MorphRefresh: repeating the equip pass for <FormID> (follow-up N of 3), because one pass alone does not materialise the illusion.`；`Features::RefreshActor()` 末尾统一调用它，`SceneOutfits` 两处改用 `Features::RefreshActor(player)`，使「点列表」与「脚本切换」走同一条序列。同时按 m06781 把模块文件名改为 `OutfitSystemNG.dll`（`CMakeLists.txt` 的 `OUTPUT_NAME`），只改磁盘文件名：`Plugin::NAME`、日志文件名、Papyrus 翻译前缀、存档标识全部不变，旧名必须从 VFS 删除防双加载（见 `docs/RACEMENU_V4_AUDIT.md`）。
19. 1.5.31：第 18 条的「补跑几次装备 pass」实测不够（m07050 —— 三次随访全部按时执行、`Custom: attaching 5 outfit-only piece(s)` 每次都有，模型仍不可见；同帧 `HairTintDiagnostics` dump 显示 biped 五槽都有几何）。真正缺的一步是**让引擎重建被绘制的 3D 模型**：新增 `OutfitSystem::rebuildActorModel()`（`Actor::Update3DModel()`，带 `Is3DLoaded()` 守卫），在 `MorphRefresh` 随访链第 2 步（`kModelRebuildAttempt = 2`）之前调用一次；同时按用户要求删掉 `SceneOutfits` 自己拼的 `set_outfit_c` + `RefreshActor`，场景开始/结束改调 `Features::AssignOutfit()`（与「点套装列表」同一条函数）。结论：装备通道（biped 条目）→ 几何物化（后续 pass）→ 上屏（引擎重建 3D）是三层，任何「切换后必须立刻可见」的路径都要显式走完第三层。

20. 1.5.32：第 18、19 条的两个方向（补跑装备 pass / 重建 3D 模型）都被实机否掉，用户要求回到「只找对名字 → 切套装」（m07424 原话：「你只用找对名字然后切换……不行你就参考按地点/战斗切换幻化套装这里的逻辑」）。本版删除 `kOutfitPassCount` / `kOutfitPassDelay` / `OutfitPassState` / `g_outfitPasses` / `RequestOutfitPassesInternal` / `DrainOutfitPasses` / 公开 `RequestOutfitPasses` 与 `OutfitSystem::rebuildActorModel()`，`NGFeatures::RefreshActor()` 回到只有一次 `::OutfitSystem::refreshArmorFor()`；`SceneOutfits.cpp` 的 StartScene / EndScene 一律调 `Features::AssignOutfit()`（与 `MenuUI::ApplyTo`、`QuickWearFocused` 同一条路），并把场景事件从「SKSE 任务队列」改成「游戏线程上同步执行」（新增 `OnGameThread()`，与 `AutoSwitch` 的 `TESActorLocationChangeEvent` / `TESFastTravelEndEvent` sink 同构；`g_dispatchMutex` 改 `std::recursive_mutex` 防同步重入），名字查找先试 `<当前套装>_sex` 再在全部套装名里做忽略大小写匹配。**`_sex` 套装不再有任何专属槽位/时机规则**（全部由用户在套装编辑器里配置）。
21. 1.5.33：场景套装回退到 1.5.29 的写法，并换成「按地点/状态切换」那套两步换装（m07666 原话：「不行直接回退到 1.5.29，从这开始改，把原来的 Sexlab 和 OStim 场景的专用换装 `_sex` 套装逻辑删了，参照按地点/状态自动切换这里的换装方法去换装」）。`AutoSwitch.cpp` / `AutoSwitch.h` 一行未改（这就是用户问的「按地点/状态自动切换」）；改动的是 `SceneOutfits.cpp`：删除 `QueueSceneEvent` / `DrainSceneEvents` / `ScheduleDrain` / `g_generation` / `g_pendingEvents` / `g_drainScheduled` / `std::deque` / `GameState::IsLoading` 分支 / `enum class SceneEvent` / `DispatchSceneEvent()` / `Tick()`（连同 `main.cpp:279`、`NGHotkeys.cpp:255` 两处调用），换成同步执行 + `std::recursive_mutex`；新增 `OutfitSystem::setOutfitByName()` 与 `setOutfitUsingState()` 并排，场景开始/结束都走「锁内指派 → 锁外 `refreshArmorFor()`」，与 `AutoSwitch::Evaluate()` 完全同构。守护测试 `tests/scene_outfit_switch.ps1`。
22. 1.5.34：第 21 条把 `AutoSwitch` 的 `OnGameThread()` 门禁一起抄了过来，而 OStim / SexLab 的事件来自它们自己的工作线程 → 事件在查名字之前就被丢掉（m07842 日志里三条 `arrived off the game thread; the scene switch is skipped.`）。本版改为「离线程事件用一个 `SKSE::GetTaskInterface()->AddTask` 交接到游戏线程 → 统一 `Dispatch(eventName)` 入口」，游戏线程上仍内联执行；不建队列、不加 `Tick()`，换装调用序列仍是 `setOutfitByName()` + `refreshArmorFor()`。教训：照抄另一个模块的线程模型之前，先确认两个模块的触发源是不是同一个线程。
1.5.14 起 `MorphRefresh` 增加「幻化循环」：形变签名变化或用户重穿同一套时，先 `set_outfit_c("")` + `RefreshActor` 摘掉幻化，再 `set_outfit_c(name)` + `RefreshActor` 穿回；引擎只会在此时为新克隆加载 `.tri` 目标。1.5.15 起这两步在**同一个主线程任务内背靠背执行**（`RunIllusionCycle()`），中间不渲染任何一帧，所以看不到原版装备，观感与在快选窗里切换套装一致；`[Compatibility] ReillusionGapMs` > 0 时才退回两段式。

另外 `Patches.cpp` 新增 `RevisionRebuildPending(FormID, revision)`：`OutfitService` 的 revision 一旦变化（套装新建/编辑/改派，包括快选窗按 E 与 Z 键重穿），该 actor 在 2 秒内走全量重建，而不是只重贴 applySet。全量重建时逐件打印 `Custom: <ArmorFormID> slot <nn> clone <ptr>`（debug），可直接看出槽位几何是否真的换了。

23. **1.5.39（m09191 / m09426）：菜单层三项新功能，引擎侧零改动。** ①逐槽策略编辑器展开状态改按套装名保存（`MenuOutfitEditor.cpp` 的 `std::unordered_set<std::string> g_slotPolicyOpen`，key = 套装名），旧全局布尔删除；它只是 UI 状态，不进 JSON，判据永远是 `slotPolicies` 非空。②新增三个模块：`src/core/PluginScan.*`（加载顺序视图 `GetLoadedMods()` + `GetLoadedLightMods()`、护甲按 `OwningPlugin()` 归组、`IsOutfitArmor()` 三条判据 = 有 EditorID + 至少一个非 FX 槽位 + 至少一个 armor addon 有模型）、`src/core/EspOutfits.*`（`GenerateFromPlugin()` 先过滤再按 32 槽「每槽第 i 件配第 i 套、不足复用第 0 件」生成、分桶用 `mask & ~kFxBit`、同名原地覆盖）、`src/core/GoldenItems.*`（金件存 `Data\SKSE\Plugins\OutfitSystemGoldenItems.json`，**不做服装过滤**）。③`Settings` 新增 `espOutfitAutoFavorite`（默认 true）与 `goldenIniMigrated`（旧 ini 金件一次性迁移）；`Settings::IsGoldenItem()` 委托 `Golden::IsGolden()`，`Outfit.cpp` 的贴件判断逻辑未变。④钩子安装、贴件语义、槽位清理、`.tri` 推导、`SceneOutfits` 一行未改（`git diff v1.5.34 HEAD -- src/cpp/hooking src/cpp/MorphRefresh.cpp src/cpp/SceneOutfits.cpp` 为空）。教训：**「能不能收」的规则分两条路** —— 幻化套装只收正常服装、金件放开，绝不能合并成一个过滤函数（m09426）。

24. **1.5.40：菜单两处回归的修复（不属于引擎侧）**。①`PluginScan::IsOutfitArmor()` 改为 `Verdict() == OutfitVerdict::kWearable`：名字判据 `GetFormEditorID()` 或 `GetFullName()` 任一非空，模型判据同时看 `RE::SEXES::kMale` / `kFemale` 的 `bipedModels[sex]` 与 `bipedModel1stPersons[sex]`（1.5.39 只查男性 ⇒ 女性服装包全被判非服装 ⇒ 幻化页 ESP 列表为空）；`CollectArmors` 结束时打一行 `count as clothing (… unnamed / … FX only / … without an addon / … without a model)`。②`EspOutfits::PluginsWithArmor()` 的列表过滤从 `entry.outfitArmors == 0` 改为 `entry.armors == 0` —— 「能不能列出来」与「能不能收进来」分开：列表只要求该 ESP 有槽位护甲，逐件是否收进套装仍走 `IsOutfitArmor()`。③`I18n` 增加 `PlaceholderSignature()` / `PlaceholdersMatch()`，`Apply()` 对 `kDictionary` 与翻译文件每条都校验占位符序列，不一致则忽略该条并回退英文（1.5.39 的中文译文把 `%zu`/`%s` 换了顺序，而 `I18n::T()` 的返回被当 printf 格式串，`%s` 吃到 `size_t` ⇒ `strlen(0x8)` ⇒ AV at `OutfitSystemNG.dll+0125E41`）。

25. **1.5.41（m10014）：启动即闪退——不是引擎层问题，是金件存库锁的自死锁。** 崩溃栈的字符串槽里是 `resource deadlock would occur`，前 7 帧在 `OutfitSystemNG.dll`：`MessageHandler(kDataLoaded)` → `Golden::LoadFromDisk` → `Golden::ImportText`（持 `g_mutex`）→ 公共 `TotalCount()`（再持一次）→ MSVC 抛 `std::system_error(errc::resource_deadlock_would_occur)`，异常逃出 SKSE 回调 → 退出游戏。三处结构性改动：①`src/core/GoldenItems.cpp` 的 `g_mutex` 由 `std::mutex` 改 `std::recursive_mutex`，并新增匿名 namespace 的 `TotalCountLocked()`，三条持锁路径不再调加锁 getter；②`src/cpp/main.cpp` 的 `kDataLoaded` 金件块整体包 `try/catch`（失败只打 `Golden items: the store could not be loaded …; the golden list stays empty for this session.`）；③`MessageHandler` 拆成 `HandleMessage` + try/catch 壳（`Message handler: an exception escaped …`）。**约定**：任何在 SKSE 回调里跑的代码（消息处理器 / 事件 sink / 数据加载）都不许让异常逃出去；同线程重入加锁一律用递归锁 + `…Locked()` 私有 getter。引擎侧（钩子、贴件、槽清理、`.tri`、场景套装）一行未动。

26. **1.5.42（m10183）：删除幻化套装预设闪退——菜单分页切片在列表清空后仍按旧页码算。** 崩溃栈前 6 帧在 `OutfitSystemNG.dll`、`[RSP+188]` 是 `vector too long`、`[RSP+2C0]` 是 `SOS::Menu::`anonymous-namespace'::RenderOutfits(void)`，PE `.pdata` 对位把 `0x8C164` 落到 `RenderOutfits`（`0x8B640..0x8DA34`）内、`0x2BA0` 落到存有 `vector too long` 的小函数（`std::_Xlength_error` 抛出点）。真因：删除按钮先 `delete_outfit()` 再 `g_filtered.clear();`，随后仍用**删除前**的 `g_page` 算本页区间 `begin = (g_page - 1) * pageSize` / `end = min(g_filtered.size(), begin + pageSize)`；列表清空后 `end = 0` 而 `begin > 0`，`end - begin` 以 `size_t` 下溢成接近 `SIZE_MAX`，`pageFavorites.assign(...)` / `pageArmorCounts.assign(...)` 抛 `std::length_error`，异常从菜单渲染回调逃出 → 游戏终止。第 1 页删除不触发（`begin = 0`），只在第 2 页及以后崩，与用户「删预设就崩」吻合。修法：先取 `const std::size_t total = g_filtered.size();`，`begin = min(total, max(0, (g_page - 1) * pageSize))`、`end = min(total, begin + pageSize)` ⇒ `begin <= end <= total` 恒成立；`assign` 本身不放宽，另加 `tests/menu_page_slice.ps1` 把这条夹紧锁住。引擎侧与金件存库一行未改。
27. **1.5.43（m10387）：金件页三处——组行命中矩形、编辑器自动写盘、light 插件 id 宽度。** 全在 UI 与数据契约层，引擎侧未动。①`MenuUI::RenderGolden()` 的组行原先用 `IG::Selectable(label, isSelected)`（尺寸默认 `(0,0)`，命中区只有一行文字高），且左栏宽取用户可拖的 `outfitListWidth`、右栏 `BeginChild(..., ImVec2(0, 440))` 的 0 表示「剩余宽度」，剩余为负时右栏覆盖左栏吞点击；改为新增 `GoldenRow()`（宽 `max(ContentRegionAvail().x, CalcTextSize(label).x + 32)`、高 `CalcTextSize(字母).y + 8`）+ `listWidth` 夹到 `[240, max(240, avail - 320)]` + 右栏 `max(320, avail)`。②编辑器原先四处（批量添加 / 移除单件 / 移除整组 / 手输添加）改完即 `SaveToDisk()`；改为文件静态 `g_goldenDirty`，`RenderGolden()` 内 `SaveToDisk()` 只剩导出分支一处，数据路径下方常显 `[not saved] …` / `Changes reach the file only when you press Export.`；内存快照仍由 `Publish()` 即时更新，所以「即时生效」与「不写盘」并存。③金件 id 原存 `formId & 0x00FFFFFF`，对 light(ESL) 插件这 24 位含高 12 位**光索引**（日志里 `002E280C` / `002E1D66` / `002E0823` 的高位 0x2E0/0x2E1/0x2E2 连续递增即证据），而 `TESDataHandler::GetFormID(modIndex, lightIndex, formID)` 对 light 插件返回 `0xFE000000 + (lightIndex << 12) + formID`（不掩码），`LookupForm` 于是把光索引叠两次 ⇒ 61 条全 missing；`PluginScan` 新增 `light` 表 + `IsLightPlugin()` / `LocalIdFor()`（light 掩 `0xFFF`）/ `ResolveForm()`（掩码 id 优先、24 位兜底），`GoldenItems` 的 `Normalise()` / `AddArmor()` / `AddAllFromPlugin()` / `AddLocal()` / `Resolve()` 与菜单单件行全部改走它们（旧档加载即自愈）。
## 9. 存档

co-save 记录：签名 `'AAOS'`（0x41414F53），版本 **7**，内容为 UTF-8 JSON：

```json
{
  "version": 2,
  "enabled": true,
  "outfits": [{ "name": "...", "favorite": false, "blanket": "XXOO",
                "slots": [{"slot": 9, "code": "XEXO"}],
                "armors": [{"form": 12345}] }],
  "actors": [{ "actor": 20, "autoSwitch": true, "current": "...",
               "states": [{"state": 12, "outfit": "..."}] }]
}
```

- 载入时 `ResolveFormID` 迁移旧存档的 FormID，再用 `LookupByID` 解析；解析不到的件被丢弃。
- 记录版本 ≤ 6 是 Rust 时代的 protobuf，只记一条日志后跳过（不复用旧存档数据）。
- 另有 `ExportSettings` / `ImportSettings` 写读 `Data/SKSE/Plugins/OutfitSystemData.json`。

## 10. 生命周期

| 时机 | 动作 |
| --- | --- |
| SKSEPlugin_Load | `SKSE::Init`、`Settings::Load`、日志、注册消息/存档回调、`RegisterPapyrus` |
| kDataLoaded | 分配 trampoline → 按 `REL::Module::IsAE()` 装 5 个钩子 → `RegisterEvents` → `AutoSwitch::Install` → `MorphRefresh::Install` → `SceneOutfits::Install` → `SOS::Compat::Install` → `Hotkeys::Install` → `Menu::Register` |
| kNewGame / kPreLoadGame | 清空指派（保留套装）、`check_consistency`、黄金件重新解析、`AutoSwitch::Reset` / `MorphRefresh::Reset` / `SceneOutfits::Reset`、`SOS::Compat::Reload` |
| kPostLoadGame | 同上 + 等玩家 3D 就绪后 `CreateRefreshAllArmorsTask()` 重刷所有 actor 外观（刷新时会重写一次兼容状态） |
| kSaveGame | 无（co-save 由 SerializationInterface 触发） |

## 11. 错误处理约定

- 钩子安装分三步：`RequireIds`（地址库是否有这些 ID）→ `FindCallInFunction`（宿主函数入口起 `0x800` 内扫 `E8` 且目标精确命中；`CustomSkinPlayer` 走 `FindCallInFunctionOrOffset`，扫不到时回退历史站点且要求那里是 5 字节 `E8`）→ `write_branch`。任一步失败只 LOG(err) 并返回，其余钩子继续装。1.5.21 起两个皮肤钩子必须成对：缺一个就 `SetIllusionDisplayEnabled(false)`，幻化整条关闭、原版流程照跑（显示真实护甲而不是裸体）。
- 单钩子失败不会让插件停止：例如只有 `CustomSkinPlayer` 装不上时，幻化“显示集合”仍生效但模型不更新，日志有明显提示。
- 存档解析失败只 LOG(err)，保留内存中已加载的数据，不抛异常跨 DLL 边界。
