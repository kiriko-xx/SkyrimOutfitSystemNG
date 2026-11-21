# 测试说明（TESTING）

本机没有安装 Skyrim，**所有游戏内行为都未经实机验证**。本文给出：可离线做的静态验证、首次实机测试的顺序、功能矩阵与回归清单。

## 1.5.28 最新验证协议（当前验收入口）

**验证状态（1.5.28）**：尚未实机验证，不能声称已实机修好。生产函数路径回归 6/6 通过；最终 Release BUILD_EXIT=0，DLL 版本/哈希与交付副本一致。包内逐文件清单见 SHA256SUMS.txt；最终 zip 大小、哈希及解包复核记录见包外 VERSIONS.md 与同名 .zip.sha256，避免自引用。

> **1.5.28 纠正（历史说明，不作当前验收依据）**：旧文档将握手归因于“仅消息嵌套”/receiver 名字错位的解释已纠正，实际是注册时机导致 sender 订阅遗漏；主线程 Queue 重试并非完整修法。旧 `carries a .tri morph target on N of M`、`N > 0` 或 `N = M` 判定均不充分，不能证明 TRI shape 匹配或 morph 顶点移动；旧实时成功措辞是当时目标/假设而非实机结论。当前以 1.5.28 最新章节为准。

### 离线与源码审查

| 用例 | 期望与记录 |
| --- | --- |
| 路径规范化回归 | 已有 6 例通过；必须覆盖已有 meshes 前缀不再拼成 meshes\meshes、无前缀/斜杠变体。只验证路径逻辑，不外推游戏表现 |
| BODYTRI 类型/值/资源 | 同名但非 NiStringExtraData、空路径、不可打开资源不计数；正确类型+非空+可打开才计数；N/M 不验证 TRI shape 名或顶点 |
| 1P/3P 与性别 | 1P 使用 bipedModel1stPersons；男女、1P/3P 不串缓存 |
| 多 ARMA | 同一 ARMO 的不同实际 addon 使用各自模型路径，不能沿用另一 ARMA 的命中/失败缓存 |
| 共享 BODYTRI / ABI 声明 | 已有 BODYTRI 规范化以新 NiStringExtraData 替换本节点，不原位修改共享字符串；NiPointer 保留原对象并在失败时恢复；GetBodyMorphs 等未用虚槽只纠正签名，不变更槽顺序 |
| 回调与签名 | 不以 IsIllusionClone 吞掉全部 biped 回调，仅 selfWrite 防重入；provider 之前取几何基线再比较 |
| 自动刷新路径 | RequestBake 排队，不再 CycleIllusion 摘掉再穿回；基础钩子与身体槽策略不变 |
| skee API 版本门禁 | 无接口/不支持版本安全跳过对应调用；MorphShapeCallback 仅在 v5+ 使用，不越过虚表版本；有接口与无接口分别记录，不能把排队重试当接口已取得 |

### 实机矩阵（全部待测）

| 用例 | 验收要求 |
| --- | --- |
| 基本幻化回归 | 应用、切换、取消、空套装、真实护甲穿脱、32 身体槽均完整；不只剩头手，不出现空模型；不得为了 morph 改基础钩子/清身体槽 |
| 视角与多模型 | 同一套装的 1P/3P、男女、多 ARMA/自定义种族逐一检查对应资源与可见外观 |
| RaceMenu Body Morphs | 连续拖动多个明显滑块，记录身体和各幻化件实际顶点/外观变化及时间，不能只贴 N/M 日志 |
| OBody / SLIF | 分别换明显不同预设、膨胀与撤销，记录每件是否变化及延迟；无接口时明确可能不实时 |
| 无可见中间态 | 录屏检查全过程不露原版甲、不摘穿、不闪空身体；无旧 CycleIllusion 日志仅是辅助证据 |
| 界面关重开 | 拖动后关闭 RaceMenu/快选/设置窗口再重开、再拖，形状不回落、事件仍能工作 |
| TRI 不匹配负例 | 使用可打开但 shape 名不匹配的 TRI；即使 N=M 也不得判为 morph 成功；同时覆盖无 TRI/坏资源 |
| 接口门禁与回调 | 可用接口、无接口、不支持版本分别测试；selfWrite 不回调递归，外部 biped morph 不被误过滤 |

### 握手排障协议

SKSE64 2.0.20 的 `RegisterListener(nullptr)` 只订阅注册当时已有的 sender 列表（含当时预留槽），不是对未来 sender 的持续订阅；skee 的 `[Features] bEnableEarlyRegistration=1` 若使其过早注册，可能漏掉后加载的 SOS。`QueueProviderRetry()` 只重发消息，不能补建 skee 对 SOS 的订阅关系。公开配置修法：检查**实际游戏生效的** `Data/SKSE/Plugins/skee64.ini`，若 `[Features] bEnableEarlyRegistration=1`，由用户改为 `0` 并完全退出、重启游戏；不自动修改，也不打包用户 ini。当前没有本次 `skse64.log`，不能断言本次配置必为 `1`。协议依据：[SKSE v2.0.20 PluginManager.cpp L488–616](https://github.com/ianpatt/skse64/blob/v2.0.20/skse64/PluginManager.cpp#L488-L616)。

重启后收集本次 SKSE/plugin 日志、RaceMenu/skee 版本及实际生效配置，再判断订阅与接口交换；不要把历史日志的收件集当成此次证据。公开配置修法不等于 1.5.28 已实机修好。

## 1. 静态验证（已在构建机完成，1.5.17）

| 项 | 方法 | 期望 |
| --- | --- | --- |
| 编译 | `cmake --build build --config Release` | `BUILD_EXIT=0`，0 error |
| 导出符号 | 字符串扫描 DLL | 含 `SKSEPlugin_Load` / `SKSEPlugin_Query` / `SKSEPlugin_Version` |
| 导入表 | 自写 PE 解析 | 只有 KERNEL32/USER32/ole32/VERSION/SHELL32 |
| 延迟导入 | 同上 | delay-import 目录 RVA = 0 |
| 独立加载 | `LoadLibraryExW(path, 0)`（空目录） | 句柄非 0，`GetLastError()==0` |
| Papyrus 脚本 | `papyrus.exe compile` | 6 个脚本全部 `status=0`（含 `SkyrimOutfitSystemNativeFuncs`） |
| 资源完整性 | 目录对比 | esp / pex / psc / 翻译 / ini 齐全 |

## 2. 首次实机测试顺序

1. **只装前置**（SKSE + Address Library），确认游戏能进主菜单。
2. 装本包 + 菜单框架，进游戏，看日志：
   - `Trampoline ready: ...`
   - 五条 `... hook installed at rva 0x...`（缺一条说明该钩子站点的字节被别的 mod 改过）
   - `Compatibility: Advanced Nudity Detection ... detected; nudity, topless and bottomless ranks now follow the displayed outfit.`（装了 AND 时）
   - `Menu section registered`（没装框架时是 `Menu: SKSE Menu Framework not found`）
3. 打开菜单框架（默认 F1）→ `Skyrim Outfit System` → Outfits 页新建套装（Create from worn），双击应用，看外观是否变化。
4. 换一件同槽位护甲，确认外观仍是套装件（说明 `DontVanillaSkinPlayer` 生效）。
5. 控制台 `player.GetWornForm(4)`（头盔槽掩码 4）应返回幻化件；`player.GetActorValue("DamageResist")` 应随真实护甲变化（说明数值未被篡改）。

## 3. 功能矩阵

### 3.0 读档 / 存档（1.0.1 修复重点）

| 用例 | 期望 |
| --- | --- |
| 进游戏后读一个带幻化的存档 | 载入界面正常结束，不卡死；日志有 `Post-load: hooks resumed` 与 `Player 3D is ready ...` |
| 读档后立刻看外观 | 幻化件仍在（说明延迟刷新生效） |
| 在原版（Rust 版）存档上首次加载 | 日志 `Migrating a v6 record written by the original mod (... bytes).` → `Migrated co-save v6: N outfits, N actor assignments.` |
| 迁移后再手动存档、再读档 | 第二次不再出现 `Migrating`，只有 `Loaded N outfits ...` |
| 新建游戏 | 日志 `New game / pre-load: outfit data reset`，套装表为空，玩家自动有指派 |
| 读档过程中切换菜单/施法 | 不崩、不卡；读档窗口内幻化的钩子走原版路径（外观可能短暂是真实护甲，读完即恢复） |
| 存档时同时有 Papyrus 脚本调用 | 不卡死（互斥锁正常释放） |

### 3.1 幻化基础

| 用例 | 期望 |
| --- | --- |
| 套装只有头盔，穿全套乌木甲 | 只有头部换外观，其余正常 |
| 套装里有玩家没穿的件 | 该件被贴到模型上（`CustomSkinPlayer`） |
| 套装件与真实件同槽位 | 显示套装件 |
| 隐藏某槽（策略为 X） | 该槽不显示任何东西（会露皮肤） |
| 黄金件（龙鳞护甲） | 永远显示真实件 |
| 切换套装 | `RefreshArmorFor` 后立刻生效 |
| 关闭 `[Debug] InstallHooks` | 无幻化，但游戏正常，日志有 `skipped` |

### 3.2 第三方 mod 兼容（1.1.0，零 hook）

前提：安装 Procedural Armor Damage 1.0.4 与 Advanced Nudity Detection 4.2.0，ini `[Compatibility]` 两个开关默认 on。

| 用例 | 期望 |
| --- | --- |
| 启动日志 | `Compatibility: Advanced Nudity Detection 4.2.0.0 detected; nudity, topless and bottomless ranks now follow the displayed outfit.` |
| **AND**：穿完整护甲 + 应用幻化 | `AND_ModEventListener.GetNude(player)` 返回 False；AND 自身的状态显示为“有穿” |
| **AND**：把套装换成空套装 | `GetNude` → True、`GetTopless` → True、`GetBottomless` → True（三项一起变红） |
| **AND**：只幻化上身（下装保留真实裤子） | `GetTopless` = False，`GetBottomless` = False |
| **AND**：只幻化下装（上装保留真实甲） | `GetTopless` = False，`GetBottomless` = False |
| **AND**：等 3 秒不再有任何操作（让 AND 重扫） | 等级被心跳补写回来，仍与幻化一致（日志 ExtraLogging 下会有 `AND compatibility: ... rank N.`） |
| **AND**：读档后立刻测试 | 阵营状态在 1 秒内回到幻化结果 |
| **AND**：删除/禁用 AND 后启动 | 日志无 `Compatibility: Advanced Nudity Detection`，其它功能正常 |
| **PAD**：真实护甲 + 幻化，挨打 | 幻化外观下那件真实护甲被撕（PAD 的破损数据变化，外观出现缺口） |
| **PAD**：幻化件盖在没有真实护甲的槽位 | 该件不会被撕；开 ExtraLogging 时日志有 `PAD compatibility: ... without a real armour underneath` |
| **PAD**：不装 PAD | 日志无 `PAD compatibility`，幻化不受影响 |
| 两个 mod 都不装 | 日志只有 `Compatibility: Advanced Nudity Detection = not installed, Procedural Armor Damage = not installed` 之类的汇总，幻化正常 |

### 3.3 状态自动换装

| 用例 | 期望 |
| --- | --- |
| 城内 + 配 City 套装 | 进城切换 |
| 战斗开始 | 切 Combat 套装（优先级最高） |
| 未配 Combat | 回落到其它匹配状态 |
| 全部未配 | 使用基础 `SetSelectedOutfit` |
| 下雪 | 命中 Snowy 变体 |

### 3.3b 快捷换装窗口（1.0.2 引入，1.0.3 起为法术默认行为）

| 用例 | 期望 |
| --- | --- |
| 施放 “Change Outfit” 法术（ini `QuickSlotRandom = off`，默认） | 立刻弹出快速选择窗口（不是无反应、不是随机换装）。脚本属性 `RandomOnCast` 已不决定行为，ESP 里存 True 也不影响 |
| 窗口内输入关键词 | 列表实时过滤；清空后恢复全部 |
| 勾选 Favourites | 只剩收藏套装 |
| 翻页（首页 / 上一页 / 下一页 / 末页） | 页码在 1..N 之间，列表内容随之变化 |
| 单击条目 | 立即换装并关闭窗口 |
| Random 按钮 | 随机应用一套并关闭窗口 |
| Ctrl+Shift+L | 窗口开关切换 |
| 未安装 SKSE Menu Framework | 法术弹 HUD 提示 “install SKSE Menu Framework”，DLL 仍正常加载、幻化与热键可用 |
| `SkyrimOutfitSystemNativeFuncs.pex` 存在性 | `dist/Scripts` 里必须有；Papyrus.0.log 不得出现 `Cannot open store for class` 或 `could find no matching static function` |

### 3.4 随机换装与 UI

| 用例 | 期望 |
| --- | --- |
| `Ctrl+Shift+O` | 随机换装并弹 HUD 提示 |
| 按住不放 | 只触发一次 |
| 在输入框里打字 | 不触发 |
| 1000+ 套装搜索 | 输入即时过滤，翻页无卡顿 |
| 双击列表项 | 应用套装 |

### 3.5 存档

| 用例 | 期望 |
| --- | --- |
| 新建角色 | 无套装、玩家在指派表 |
| 存盘再读 | 套装、指派、状态配置全部恢复 |
| 读旧版（protobuf v4/v5/v6）存档 | 自动迁移：`Migrating a vN record ...` → `Migrated co-save vN: N outfits, N actor assignments.`；套装与指派都在 |
| `ExportSettings` / `ImportSettings` | JSON 往返一致 |

### 3.6 设置项（1.0.3 引入）

| 用例 | 期望 |
| --- | --- |
| 施放 “Change Outfit” 法术，ini `QuickSlotRandom = off` | 弹快速选择窗口 |
| 同上，菜单里勾选 “The "Change Outfit" power rolls a random outfit” 后再施放 | 直接随机换装并弹 HUD 名称 |
| 改完设置后看 `Data/SKSE/Plugins/SkyrimOutfitSystemSE.ini` | `[RandomOutfit] QuickSlotRandom = on`，且文件里的其它注释仍在（Save 是读回再写） |
| 重开游戏再施放 | 行为保持上次的设置（说明写盘成功，不是只改内存） |
| 菜单 Settings 页的快速选择窗口热键输入框 | 改成 alt+F7 后立刻生效，不需要重启；ini 里 `QuickPickerHotkey = alt+F7` |
| 菜单里改随机换装热键 | 同上，写 `[RandomOutfit] Hotkey` |

### 3.7 菜单语言与控制台诊断（1.2.0）

| # | 步骤 | 期望 |
| --- | --- | --- |
| 1 | Settings → Language → English | 界面英文，ini `[UI] Language = en` |
| 2 | 选“中文”，且 SMF 有中文字体 | 界面立即变中文；ini 变 `zh`；重进游戏仍是中文 |
| 3 | 选“中文”，但 SMF 没字体（`EnableChinese=false` 或无 `Fonts/CN.ttf`） | 界面保持英文，日志出现 `Language: Chinese was requested but ...` |
| 4 | 选“自动 (game language)”，游戏为英文 | 界面英文；`sLanguage` 为中文时界面中文（有字体时） |
| 5 | 切语言后操作一次列表/下拉 | 控件不丢状态、不串行（标签用 `中文###English`，id 恒为英文） |
| 6 | Settings → Mod compatibility → Log slot report | 日志出现 `PAD slot report: slot ...` 若干行 + 真实护甲清单；HUD 提示“槽位报告已写入” |
| 7 | 应用一个含“无真实护甲”槽位的套装 | 日志出现一次 `PAD compatibility: ... with no real armour underneath`；反复刷新同套装不再重复刷屏 |

### 3.8 套装编辑器与 MCM 平价（1.3.0）

| # | 步骤 | 期望 |
| --- | --- | --- |
| 1 | Outfits 页选中一个套装 | 右栏出现 `Editing: <name>`、件数、`Back to the list`、`Wear` / `Take off` / `Favourite` |
| 2 | Add armour → From the inventory | 列出背包里的护甲；点一条即加入，日志 `Outfit editor: 'X' now has N pieces.` |
| 3 | Add armour → From the worn armour | 列出当前穿着的护甲（与 Papyrus `GetWornItems` 一致） |
| 4 | Add armour → By form ID，输入 `00012EB7` | 加入铁甲；输入不存在的 ID 提示 `No form with that ID is loaded.`；输入非护甲 ID 提示 `That form is not armour.` |
| 5 | Add armour → From the full armour list，输入 `glass` | 过滤出玻璃甲；勾掉 `Playable only` 后条目变多；结果超过 400 条时提示 refine |
| 6 | 往已有头盔的套装里加另一顶头盔 | 出现 `... uses a body slot that is already covered by this outfit. Replace the conflicting pieces?`；点替换后旧头盔消失、新头盔加入 |
| 7 | Body slots 里点某件护甲的 Remove | 先出确认条 `Remove "X" from the outfit?`，确认后才移除 |
| 8 | Slot policies → Base policy 选 `Passthrough` | 套装整体策略变；脱掉全部真实护甲后外观跟着策略变化（需实机确认渲染） |
| 9 | 勾 `Per-slot policy editor` → 某槽选 `Never show anything` | 该槽不再显示幻化件；选 `Use base policy` 恢复继承 |
| 10 | `Reset all policies to the defaults` | 全部槽回到默认（盾牌槽 `XEXO`，其余 `XXOO`） |
| 11 | Outfits 页 `Apply to` 选一个 NPC | 列表 Wear / 双击作用于该 NPC；编辑器 `Target: <NPC>`；列表里该 NPC 当前套装带 `*` 前缀 |
| 12 | Actors 页 `Add a nearby actor...` | 列出同 cell 未死亡 NPC；选中后加入列表（不重复） |
| 13 | Actors 页点 Remove | 出确认条 `Stop managing <name>? The player cannot be removed.`，确认后移除；玩家的 Remove 只提示不可移除 |
| 14 | Settings → General 取消 `Give the player the "Change Outfit" power` | `player.HasSpell` 变 false、法术书里消失；重新勾选后回来；ini `[General] QuickslotsEnabled` 同步 |
| 15 | 新建/改名输入超过 256 字节的名字 | 提示 `That outfit name is too long.`，不会创建；重名提示 `That name is already taken.` |
| 16 | 删除套装 | 先出确认条 `Delete the outfit "X"? This cannot be undone.`，确认后才删 |
| 17 | 控制台 `cqf SkyOutSysMCM GetModVersion`（或第三方脚本调用） | 返回 `16777216`（0x01030000）；`GetQuickslotManager()` 返回快捷槽管理 quest 的脚本实例，不报「函数不存在」 |

### 3.9 翻译文件与快速选择窗口操作（1.4.0）

**外置翻译文件**（1.4.1 起文件布局已改为 `Data\interface\translations` 分语言文件，用例见 §3.10）

**快速选择窗口**

1. Ctrl+Shift+L 打开：窗口**首次**居中、默认 640×800；拖动/缩放后关闭再打开保持上次位置（跨重启的持久化见 §3.10-C）。
2. 打开时高亮行 = 当前穿着（列表里带 `* ` 前缀那行），列表自动滚到它可见。
3. `W`/`S`、`↑`/`↓`、手柄十字上下：逐条移动；按住 0.4 s 后开始连续、每 0.045 s 一次（加速）。
4. `A`/`D`、`←`/`→`、手柄十字左右：整页翻动，页码 `x/y` 变化，焦点跟随页内位置不越界。
5. `E`/`Enter`/手柄 A：换上并关窗；`R`/手柄 Y：随机一套（窗口不关，高亮跳到抽中的那套并滚到可见）；`Esc`/手柄 B：关闭。
6. 搜索框里打字时 `W/S/A/D` 不移动焦点（`IsAnyItemActive` 判定），`Esc` 仍能关窗。
7. 底部 **Remove outfit**：玩家当前套装被清空，HUD 显示 `Outfit removed.`，角色回到真实护甲外观。
8. 把每页数量改成 10：关窗重开、重启游戏后仍是 10（`[UI] QuickPickerPageSize`）。

**Outfits 列表**

1. 名字超过 40 字符的套装不再被 `Wear`/`*`/`Delete` 截断：出现横向滚动条，行宽随文本撑开。
2. Settings → Interface 的 **List width** 拖动时列表宽度即时变化，并写回 `[UI] OutfitListWidth`。
3. 双击只在**行上**才应用套装（双击空白处不换装）。

**PAD（Procedural Armor Damage）撕幻化件**

1. 真实穿护甲 A，幻化成外观 B：外面看起来是 B。
2. 触发 PAD 的损伤后，**B 的外观**出现破损（掉三角面），而不是 A 的模型。
3. 日志里该槽不再出现 `shows pieces over slot mask … with no real armour underneath`。
4. 对照组：`[Compatibility] ProceduralArmorDamage = off` 重启，同样操作 B 不再破损（证明还原开关就是修法）。
5. 边界：完全没有真实护甲打底的槽（裸槽贴裙子）仍不可撕，日志会说明是哪个掩码（预期）。

### 3.10 1.4.1 五项（翻译文件拆分 / 文案补全 / 窗口位置 / 老存档 / JSON 导入导出）

**A. 语言文件（`Data\interface\translations`）**

1. 装包后目录里应有 `SkyrimOutfitSystemNG_english.txt` 与 `SkyrimOutfitSystemNG_chinese.txt`（UTF-16LE，各 233 行）。日志：`Language: 2 language file(s), 466 lines in total, from ...`。
2. Settings → Language 下拉里应出现 `English` 与 `中文` 两项（外加 `Auto (game language)`）。
3. 把 `_chinese.txt` 里 `$Wear<TAB>穿上` 改成 `穿上衣服`，点 **Reload the language files**，Outfits 页按钮立刻变。
4. 删掉某一行（例如 `Always use outfit`）：该处回退到内置中文词典而不是英文。
5. 复制 `_english.txt` 为 `_german.txt` 并改几行 → 重启游戏后下拉里出现 `German`，选中即生效（不需要改插件）。
6. 把两份文件临时移走再启动：日志 `Language: wrote the editable translation files to …`，目录里重新出现两份全量模板。

**B. 文案覆盖**

1. 套装编辑器里 12 条槽位策略全部是中文（含 `XXXO` → 需要已装备（槽位有装备时才显示套装）、`XXOO` → 始终使用套装、`<inherit>` → `<继承>`）。
2. 32 个槽位名（Head、Hair, Short、Body、…、Unnamed 44–60、FX）都是中文。
3. Actors 页 13 个状态名都是中文（战斗 / 野外（雪天）…）。
4. 拼接提示：点套装上的 `Wear` 后 HUD 显示 `套装：<名称>` 而不是 `Outfit: <名称>`。
5. 关掉菜单框架再进游戏：HUD 提示 `Skyrim Outfit System: install SKSE Menu Framework ...` 为中文。

**C. 快速选择窗口位置固化**

1. Ctrl+Shift+L 打开，拖到屏幕左下角、拉大到 900×700，关闭窗口。
2. 检查 ini：`[UI] QuickPickerX / QuickPickerY / QuickPickerWidth / QuickPickerHeight` 已写入实际值（关闭瞬间写盘）。
3. 重启游戏再打开：窗口回到左下角、尺寸仍是 900×700（不再是居中 640×800）。
4. 删掉这四个键 → 恢复到居中默认。
5. 若日志有 `ImGuiApi: igGetWindowPos/igGetWindowSize are missing`，本项自动退化为居中（预期，不影响其它功能）。

**D. v4 老存档槽位策略**

1. 用旧版（co-save v4）存档：某套装原本设 `XXXO`（需要已装备）→ 读档后套装编辑器里该套装 blanket 应显示 `XXXO`，不是 `XXOO`。
2. 老存档里若设过槽位级数字策略（例如盾牌 XEXO、身体 XXOX），迁移后逐槽一致。
3. 日志：`Loaded N outfits and M actor assignments from a legacy co-save.`，无 `skipping slot policies` 类警告。
4. 已用 1.4.0 迁移过一次、策略被写成 XXOO 的存档：重新导入旧 co-save（或旧存档）后恢复。

**E. `OutfitSystemData.json` 导入导出（原版结构）**

1. Settings → Outfit data → **Export outfits to OutfitSystemData.json**：`Data\SKSE\Plugins\OutfitSystemData.json` 生成，HUD `Wrote JSON config to ...`。
2. 用文本编辑器打开：顶层键是 `enabled` / `outfits` / `actorOutfitAssignments`；套装条目是 `name` / `isFavorite` / `armors[{rawFormId,localFormId,modName}]` / `slotPolicy` / `slotPolicies{"9":"XEXO"}`；默认值字段可省略。
3. 导入：先删掉一个套装，再点 **Import outfits from ...**，套装回来、指派回来，HUD `Read JSON config from ...`，外观自动刷新。
4. 用**原版 mod** 导出的 `OutfitSystemData.json` 直接导入：套装名、收藏、护甲、槽位策略、角色指派都应还原（这是对齐原版结构的验收点）。
5. 坏文件（删掉一个 `}`）：HUD `Failed to parse config data. Invalid syntax.`，原有数据不被破坏。
6. Papyrus 等价入口：`SkyrimOutfitSystemNativeFuncs.ExportSettings()` / `ImportSettings()` 行为相同。
### 3.11 1.4.2 五项（快选窗口自绘 / 长按 / JSON 策略 / 槽位编号 / 措辞）

**A. 快速选择窗口（Debug 标题 / 位置 / ini）**

1. 按 Z（或热键）打开快选窗：标题必须是“天际幻化系统 - 快速选择”（英文 `Skyrim Outfit System - Quick picker`），**不能是 “Debug”**。
2. 首次打开应居中、尺寸 640×800；拖动窗口到左上、缩放到 900×600，关闭后重新打开（不重启游戏）应回到 900×600 的左上位置。
3. 重启游戏再打开：位置与尺寸仍是上次的（`DataSKSEPluginsSkyrimOutfitSystemSE.ini` 的 `[UI] QuickPickerX/Y/Width/Height` 已写入非 -1 值）。
4. 直接改 ini：`QuickPickerX=200`、`QuickPickerY=150`、`QuickPickerWidth=700`、`QuickPickerHeight=500`，重启游戏打开 → 窗口应出现在 (200,150)、700×500。
5. 日志应出现 `Quick picker: opening at 200,150 with 700x500 (restored from the ini).`；若是首次则是 `(centred)`。
6. 日志出现 `ImGuiApi: N/M optional helpers resolved ...`：若 N<M，逐条看 `optional entry point ... is missing`，窗口仍应能打开（只有对应小功能缺失）。
7. F1 菜单里的“快速选择”页也应正常显示（不额外开窗、不显示 Debug 标题）。

**B. 长按灵敏度**

1. 按住 `S`/`↓`/手柄十字下：约 0.2 秒后应开始连续下移。
2. 轻点一次只移动一格（不能出现惯性连跳）。
3. 连续按住 2 秒以上，移动速度应稳定在每 30 ms 一格左右（视觉上流畅、不卡顿）。

**C. OutfitSystemData.json 策略**

1. Export 后打开文件：`slotPolicy` / `slotPolicies` 的值必须是**数字枚举**（`"slotPolicy": 2`、`"slotPolicies": {"9": 8}`），与 v0.6.x 的 `OutfitSystemData.json` 同格式。（1.4.2 曾写成四字符码，1.4.3 已改回数字，见 §3.12。）
2. 手工把某个套装改成 `"slotPolicy": "始终使用套装"` 再 Import：该套装基础策略应显示“始终使用幻化”（XXOO），**不能是“从不显示任何东西”**。
3. 手工写 `"slotPolicy": "需要已装备（槽位有装备时才显示套装）"` 再 Import：应识别为 XXXO。
4. 手工写 `"slotPolicies": { "0": "$SkyOutSys_Desc_EasyPolicyName_XXOX" }`：应识别为 XXOX。
5. 写一个不存在的策略 `"slotPolicy": "不存在的策略"`：日志 `has the unknown base policy ...; it keeps the default.`，套装仍可用（不崩、不空白）。
6. `"slotPolicies": { "9": 3 }`（数字）也应识别为 XXOX。

**D. 槽位编号**

1. 套装编辑器 → 槽位下拉：中文显示 `头部（槽位 30）`、`身体（槽位 32）`、`盾牌（槽位 39）`；英文 `Head (slot 30)` …（1.4.2 曾用内部 0 基索引，1.4.3 改成 CK 显示号 = 索引 + 30）
2. 32 个槽位编号应为 30–61 连续无重复；日志 `PAD slot report: slot N` 的 N 是内部 0 基索引，对应显示号 N+30。

**E. 中文措辞**

1. 中文界面下策略项显示“始终使用幻化”“只有幻化时显示幻化”“需要已装备（槽位有装备时才显示幻化）”。
2. 全局搜索 `始终使用套装` 只应出现在 `docs/` 与旧版说明里；`Datainterface	ranslationsSkyrimOutfitSystemNG_chinese.txt` 中不应再有“始终使用套装”。
3. 旧版 1.4.1 写出的 JSON（含“套装”措辞）导入仍然正确（见 C.3）。

### 3.12 1.4.3 两项（槽位号 30–61 / JSON 数字策略）

**A. 槽位编号（显示号 = 内部索引 + 30）**

1. 套装编辑器 → 槽位下拉，中文应依次为：`头部（槽位 30）`、`头发, 短（槽位 31）`、`身体（槽位 32）`、`手（槽位 33）`、`前臂（槽位 34）`、`项链（槽位 35）`、`戒指（槽位 36）`、`脚（槽位 37）`、`小腿（槽位 38）`、`盾牌（槽位 39）`、`尾巴（槽位 40）`、`头发, 长（槽位 41）`、`头环（槽位 42）`、`耳朵（槽位 43）`、`未知 (一般来说是 面部)（槽位 44）`、`未知 (一般来说是 颈部)（槽位 45）`、`未知 (一般来说是 胸部)（槽位 46）`、`未知 (一般来说是 背部)（槽位 47）`、`未知 (一般来说是 其他)（槽位 48）`、`未知 (一般来说是 骨盆外侧)（槽位 49）`、`Decapitate-Head（槽位 50）`、`Decapitate（槽位 51）`、`未知 (一般来说是 骨盆下方)（槽位 52）`、`未知 (一般来说是 右腿)（槽位 53）`、`未知 (一般来说是 左腿)（槽位 54）`、`未知 (一般来说是 面部 2)（槽位 55）`、`未知 (一般来说是 胸部 2)（槽位 56）`、`肩膀（槽位 57）`、`左臂（槽位 58）`、`右臂（槽位 59）`、`其他 2（槽位 60）`、`FX（槽位 61）`。
2. 英文界面同一列表应为 `Head (slot 30)` … `FX (slot 61)`，编号与中文完全一致。
3. 盾牌必须落在槽位 39（策略编辑器里“盾牌默认 XEXO”的那一项），不能是其它号。
4. 与 CK / xEdit 对得上：在 xEdit 里看同一件护甲（如盾牌）的 BOD2 槽位号，应等于菜单里显示的号。

**B. OutfitSystemData.json 数字策略（与原版 v0.6.x 互导）**

1. **参考文件导入**：把 `兼容参考/OutfitSystemData.json`（530 套）复制到 `Data/SKSE/Plugins/OutfitSystemData.json`，菜单 Settings → Outfit data → Import，日志应打印 `Imported 530 outfits and 2 actor assignments from OutfitSystemData.json.`。
2. 随便挑几套（例如 `DOA6-橘色长连衣裙`）看基础策略：必须是“需要已装备（槽位有装备时才显示幻化）”（`XXXO` / 数字 2），**绝不能是“从不显示任何东西”**。
3. 这些套装带 `"slotPolicies": {"9": 8}` 的，盾牌那一项应显示“穿透”（`XEXO`）。
4. Export 再打开文件：`slotPolicy` 与 `slotPolicies` 的值必须都是数字（0–11，自定义 128+），字段名 lowerCamelCase，`enabled` / `isFavorite` / 空数组等默认值应被省略。
5. Export → 用同一份文件再 Import：所有套装的 32 槽策略逐一不变（重点看基础策略与盾牌）。
6. 兼容旧写法：手工把某套写成 `"slotPolicy": "XXXO"`（四字符码）、`"$SkyOutSys_Desc_EasyPolicyName_XXXO"`（翻译键）、`"需要已装备（槽位有装备时才显示套装）"`（旧中文措辞）三种，Import 后都应是“需要已装备”。
7. 未知策略 `"slotPolicy": 99`：日志 `OutfitSystemData.json: the policy '99' is unknown; it is ignored.`，该套装仍可用且不空白。
8. 自定义策略 `"slotPolicy": 130`（= CustomPolicy003）：导入后应成为 `CustomPolicy003`，再 Export 仍写回 `130`。

### 3.13 1.4.4 五项（删除按钮 / 横向滚动 / 日志分级 / 原生自动切换 / <随机>）

**A. Outfits 删除按钮**

1. 左列表选一套没人使用的套装 → `Delete`：应立刻出现确认条，文字含套装名；点 `Delete` 后该套从列表消失，日志 `Outfits: deleted 'X': yes.`。
2. 删除当前选中项：右侧编辑器应收起/清空，不残留已删套装的内容。
3. 删除一套已指派给 actor 或某个状态的套装：Actors 页对应组合应回落为 `<inherit>`/空，角色外观立刻换回真实护甲（不用等刷新）。
4. 点 `Cancel`：确认条消失、套装仍在。
5. 删除不存在的名字（例如连点两次 Delete）：日志应为 `... : no.`，不崩。

**B. 套装编辑器横向滚动**

1. 找一条护甲名很长的行：槽位名与护甲名完整显示，行尾 `Remove` 按钮被推到文本右侧而不是压在字上；出现横向滚动条，可拖到行尾。
2. 逐槽策略面板：32 个 combo 都能横向滚动查看，译文很长（如 `需要已装备（槽位有装备时才显示幻化）`）也不会被右边界截断。
3. 把窗口拖窄：两个面板仍可横向滚动，不出现文字覆盖按钮。

**C. 日志分级**

1. 默认 `SkyrimOutfitSystemNG.log`：只应有生命周期行（`Skinning hooks applied.`、`Papyrus functions registered.`、`Menu: sections registered...`、`Loaded N outfits...` 等），不应有逐条 `Settings: loaded` / `Patching ...` / `ImGuiApi: ... resolved` / `Hotkeys:`。
2. 勾选 Settings → Diagnostics → Verbose logging，重启：上述诊断行应全部出现（`debug` 级别）。
3. 关闭 Verbose logging 再重启：诊断行消失，Warn/Error 仍保留（例如 `address library id N is missing`）。
4. `PAD slot report` 只在手动点 `Log slot report` 时出现，属于 info，不受 Verbose 影响。

**D. 地点/状态自动切换（原生）**

1. 给玩家或随从配一套“World”状态套装并开 Auto switch，从城内走到野外：外观应在进入新地点后 1 秒内切换，而不是等 10 秒。
2. 快速旅行到另一个地点：到达后立刻按新地点状态换装（`TESFastTravelEndEvent` 路径）。
3. 打开 Verbose logging：日志应出现 `AutoSwitch: native location and fast travel detection installed.`，每次真实切换有 `AutoSwitch: 00000014 switched to state 3.`；站在原地不动不应反复刷同一条。
4. 天气变化（晴→雪/雨）：最多 2 秒内跟上（输入心跳兜底）；不动鼠标也应生效（`Tick` 由输入事件驱动，完全无输入时下一帧事件到达即补）。
5. `Papyrus.0.log` 不应再有 `SetOutfitUsingStateForAllConfiguredActors` 的周期调用；`SkyOutSysAutoSwitchTrigger` 仍能编译/加载（空实现）。

**E. 状态套装 `<随机>`**

1. Actors → 某 actor → `State outfits` → 任一状态下拉：`<inherit>` 下面应有 `<random>`（中文 `<随机>`）。
2. 选 `<random>`：应立刻抽一套并应用到角色；日志（Verbose）`State random: actor 00000014 entered state 3 and rolls 'X'.`。
3. 离开该状态再回来：应重新抽一套（可能相同，多试几次看是否变化）。
4. 同一状态内反复进入战斗/退出战斗：`lastState` 未变时不应反复换装（避免战斗中外观乱跳）。
5. 勾选 `RandomOutfit.FavoritesOnly` 后选 `<random>`：只从收藏里抽；若该状态下没有收藏套装，应回退到全部套装而不是空白。
6. 存档退出再读档：`<random>` 仍显示为 `<random>`（不是某次抽中的具体名字），读档后进入该状态会重新抽。

### 3.14 1.5.0 四项（列表按钮 / 快选手柄 / SMP 重置 / 场景套装）

**A. Outfits 列表按钮**

1. 左列表点一行的 `Wear`：应立刻换装，不需要点两次或精确瞄准；行本身不应因为这一下点击变成“未选中”。
2. 点 `Delete`：确认条出现；点确认后套装删除，日志 `Outfits: deleted 'X': yes.`。
3. 点星标：收藏状态切换，重启后保持。
4. 用一个超长名字（接近 100 字符）再重复 1–3：按钮应跟在文字右侧、可横向滚动，不压字、不重叠。
5. 列表很窄（List width 240）时：短名字的按钮仍在右侧，长名字按钮可滚到；Wear/Delete 都能点。

**B. 快速选择窗口手柄**

1. 手柄十字上下移动：窗口里只应有一个高亮（列表项）；不应出现第二个高亮停在 `Remove outfit` / `Random` / `Close` 上。
2. 高亮列表项时按 A：只换装并关窗；不应同时执行 `Remove outfit`（取消幻化）。
3. 按 B：只关窗；按 Y：只随机。
4. 用键盘 W/S/A/D + E/R/Esc 重复 1–3：行为与手柄一致。
5. 鼠标点击列表项/按钮仍正常（NoNavInputs 不屏蔽鼠标）。

**C. SMP 重置**

1. 未装 FSMP：Settings → Mod compatibility 应显示 `Faster HDT-SMP (DynamicHDT): not detected; this switch has no effect.`，勾选后换装不报错、不崩。
2. 装 FSMP：显示 `... detected.`；勾选后给玩家换一套带物理的幻化，观察目标角色物理重新初始化（裙摆/头发不再继承上一套的骨骼状态）。
3. 同屏放两个带物理的 NPC，只给其中一个换装：只有该 NPC 的物理被重置，另一个不应有变化（逐角色调用，不是全局 `smp reset`）。
4. 关闭开关后再换装：不应触发 `DynamicHDT.ResetPhysics`（可用 Papyrus 日志/FSMP 调试输出核对）。
5. 换装时 actor 3D 尚未加载（刚读档）：任务应直接跳过，不崩；3D 就绪后再换装正常。

**D. SexLab / OStim 场景套装**

1. 建 `测试服装` 与 `测试服装_sex` 两套，给玩家穿前者，打开 SceneOutfitSwitching。
2. 进入 OStim 场景：应自动换成 `测试服装_sex`；日志（Verbose）`SceneOutfits: OStim scene started; '测试服装' -> '测试服装_sex'.`。
3. 结束场景：应恢复 `测试服装`；日志 `... OStim scene ended; '测试服装' restored.`。
4. 同样测 SexLab：`PlayerAnimationStart` / `PlayerAnimationEnd` 路径；NPC-only 场景不应触发（只监听 Player 前缀）。
5. 当前套装已叫 `X_sex`：进入场景不变，日志 `already ends with _sex; it is kept for the ... scene.`。
6. 只有 `测试服装`、没有 `测试服装_sex`：进入场景保持原套装，日志 `no '测试服装_sex' outfit ...; the current outfit is kept.`。
7. 场景中途手动换成第三套：结束时应保留第三套，不覆盖玩家选择（日志 `the player changed to ...`）。
8. 关闭开关：进入/结束场景都不换装，也不写上述日志。
9. 与 SMP 同时开启：场景换装也应触发目标角色的 SMP 重置（共用 `refreshArmorFor`）。

### 3.15 1.5.1 性能与健壮性

**A. 列表缓存刷新**

1. 建 20 套以上，保持数量不变，把其中一套改名：Outfits 列表与快速选择窗口应立即显示新名字，不需要关菜单重开。
2. 点某套的星标改为收藏（数量不变）：Favourites only 过滤结果应立即变化。
3. 在菜单外通过 Papyrus 的 CreateOutfit / DeleteOutfit 增删套装：菜单打开后应显示最新列表。
4. 反复展开/收起 Outfits 页：同一帧内不应出现旧名字与旧收藏状态混用。

**B. 页码与删除**

1. 把 Page size 设为较小值，翻到最后一页，删除该页唯一一套：不应崩溃，页码应自动回到新的最后一页。
2. 在搜索框输入只匹配一页的关键词，再删除其中一套：列表与页码应同步收缩。

**C. 热键焦点**

1. 打开快速选择窗口，在搜索框里按住 Ctrl+Shift+O 再松开（让按键在文本框内抬起），关闭窗口后再次按 Ctrl+Shift+O：热键仍应触发。
2. 在任意文本框中按住热键不放：不应连续触发；松开再按才触发下一次。
3. 打开控制台时按热键：不应触发换装。

**D. 菜单流畅度**

1. 1000 套以上、打开 Outfits 页：滚动与翻页不应因每帧复制全部名字而卡顿；搜索输入应只过滤缓存列表。
2. 打开菜单时让角色进入战斗/换装：外观刷新不应等待菜单整帧。
3. 拖动 Page size / List width 滑条：ini 写入应节流（0.5 秒），松手后值应持久化。
4. 打开快速选择窗口并拖动/缩放，随后立即按 Esc 关闭：下一次打开应保留最后几何，不回到默认 640×800 居中。

**E. 语言文件**

1. Data\interface\translations 下两份文件各应为 237 条（1.5.16 起为 248 条）；中文文件不应再出现“套装”字样（Policy.cpp 的旧别名只影响历史 JSON 导入）。
2. 切换中文后菜单、快速选择、状态策略与槽位名应显示“幻化”，不再显示“套装”。

### 3.16 1.5.2 线程安全与缓存回归

**A. 输入线程 / 菜单**

1. 站在主菜单或载入画面，反复按随机热键：不应崩溃；进游戏后第一次按应正常随机（`Fire` 已排队到主线程）。
2. 在快速选择窗口里按住方向键连续翻页 30 秒：帧率应稳定，不应出现访问冲突；日志无 ImGui 相关错误。
3. 在搜索框里输入文字时按随机热键：不应触发换装，也不应把搜索框内容写坏（文本输入状态由渲染线程缓存）。
4. 打开 Actors 页时玩家尚未生成（理论边角）：点 Add player 不应崩溃。

**B. 状态与场景**

5. 给某状态选一个套装后立即关闭菜单：外观应直接变成该套装，不需要再换一次地点/战斗。
6. 同时开 OStim 与 SexLab（或连续触发两种 start）：第二个 start 不应覆盖第一个记录；只有最后一个 end 才恢复进入前的套装。
7. 场景中手动换第三套再结束场景：应保留第三套。

**C. 列表与设置**

8. 在快速选择窗口把 Page size 拖到 200 以上、关闭菜单、重启游戏：ini 应保存新值（1.5.2 前会回弹到 200）。
9. 删除一套正在过滤结果里的套装：当前帧列表不应残留旧行，快速选择窗口也不应还能选中它。
10. 运行中直接改 ini 的 RandomOutfit.Hotkey，再打开 Settings：输入框应显示新值。
11. 在快选窗改 Page size 后直接退出游戏（不打开 Settings 页）：重启后值应保留（所有页都推进延迟保存）。

**D. 性能回归（与 §4 同环境）**

12. 1000 套 + 搜索：输入时不应有明显分配抖动；仅收藏过滤时才应看到收藏判定开销。
13. 打开 Outfits 页连续翻 20 页：内存不应持续增长；非当前穿着行的 label 不再复制字符串。
14. 打开套装编辑器、展开 “From the inventory” 后静止 1 秒再滚动：500ms 缓存应让滚动稳定；切回游戏再打开应重新扫描。

**E. 兼容**

15. 装 PAD：反复换装 100 次，PadCompat 的诊断映射不应无限增长；幻化外观仍可被撕。
16. 装 AND：菜单打开时切状态/换装，AND 阵营应在 1 秒内追平显示集合。

### 3.17 1.5.3 边界与状态回归

**A. Papyrus 删除 / 覆盖**

1. 用控制台或脚本 `DeleteOutfit("当前穿着的套装")`：外观应立即恢复真实护甲，不需要再换一件装备。
2. 用 `OverwriteOutfit("当前穿着的套装", 新护甲列表)`：外观应立即变成新护甲；菜单里的件数同步更新。
3. 删除一个未穿着的套装：外观不应变化，其它套装不受影响。

**B. 只有状态套装**

4. 给玩家只配一个状态套装（例如战斗），不配基础套装，勾选 Location/state based switching：进入战斗应立即换上该套装。
5. 关闭上述开关后再进入战斗：不应自动换装（1.5.3 前战斗事件仍会强行切状态套装）。
6. 只有状态套装时死亡：应触发该状态套装，而不是保持原外观。

**C. 随机换装回退**

7. 只收藏一套、且该套就是当前幻化：按随机热键应重新抽到它（不能毫无反应）。
8. 收藏两套且当前是其中一套：随机应抽到另一套（randomExcludeCurrent 默认 true）。
9. 一套收藏都没有、只收藏模式关闭：随机应从全部套装里抽。

**D. 坏数据不崩**

10. 把 OutfitSystemData.json 改成 `{"outfits": "oops"}` 后菜单导入：应提示导入失败，游戏不崩。
11. 用损坏的 co-save（v7 记录里塞非 JSON 字节）读档：应记 error 并跳过，不进游戏崩溃。

**E. 金件与构建**

12. 装 Dragonborn 的死亡烙印护甲（默认金件）：幻化其它套装时该件不应被隐藏；反复开关菜单/换装不应崩溃（快照读）。
13. 重新完整编译：build/build_task.log 末尾 BUILD_EXIT=0，且编译期间不再出现 C1076/C3859（/MP 已移除）。
### 3.18 1.5.4 手柄导航回归

**A. 快速选择窗口打开时**

1. 用手柄打开 SMF 主菜单：主菜单的 PageTree / PageContent / Options 应能用十字键与 A/B 正常导航（1.5.3 中会被快选窗的 NoNavInputs 全局禁用）。
2. 再用 Ctrl+Shift+L 打开快速选择窗口：快选窗内十字上下应只移动列表高亮；不能出现第二个 ImGui 导航高亮停在 Random / Remove outfit / Close 上。
3. 按 A：只穿当前高亮套装并关窗；不能同时执行 Remove outfit。
4. 关闭快选窗后回 SMF 主菜单：手柄导航应仍然可用，不需要重启游戏。

**B. 键盘与鼠标**

5. 快选窗打开时 W/S/A/D、E/R/Esc 的行为与 1.5.3 一致。
6. 鼠标点击列表行、搜索框、Page size 滑条仍正常；搜索框聚焦时 W/S 不应移动列表高亮。

**C. 场景套装日志**

7. 当前套装名没有对应 `_sex` 套装时，日志应出现 info 级 `SceneOutfits: no '<当前名>_sex' outfit ... create an outfit with exactly that name ...`，并保持当前套装。
8. 按日志提示创建完全同名的 `_sex` 套装后再次进入场景，应切换并在结束时恢复。

### 3.19 1.5.6 SMF 版本兼容（手柄）

**A. SMF 3.18 及以上（含 3.23）**

1. 装 1.5.6 + SMF 3.18（或 3.23），打开 SMF 主菜单：十字键、A/B 应正常。
2. Ctrl+Shift+L 打开快速选择窗口：十字上下移动高亮、左右翻页、A 穿上、Y 随机、B 关闭；同时 SMF 主菜单不应出现第二个高亮。
3. 关闭快选窗后再开 SMF 主菜单：手柄仍正常。
4. 日志应有 `Menu: SKSE Menu Framework 3.18 ...`（版本以实际为准）与 `handles gamepad input for the quick picker`；不应出现 `raw gamepad input enabled`。
5. 同一 DLL 换 SMF 3.23 复测，行为应一致；3.18 与 3.23 均已确认可用，3.23 不是分水岭。

**B. SMF 3.9（旧版兜底，可选）**

6. 若手上有 3.9：启动日志应出现 `raw gamepad input enabled for the quick picker (framework ... is older than 3.18).`
7. 快选窗内十字键/A/Y/B 应可操作；若仍无反应，升级 SMF 3.18+ 后再测，不要继续在旧框架上排查。
8. 该 raw fallback 路径未实机验证；优先使用 3.18+。

**C. 回归**

9. 键盘 W/S/A/D、E/R/Esc、鼠标点击与搜索框输入不受影响；搜索框聚焦时手柄不应移动列表。
10. 关闭快选窗后，游戏应恢复输入（BlockUserInput 只在窗口打开时生效）。

### 3.20 1.5.7 X 键取消幻化与 .tri 形变实时刷新

**A. 快速选择窗口 X 键**

1. 打开快速选择窗口，键盘按 `X`：当前角色的幻化应立即取消（恢复真实护甲外观），HUD 出现“已取消幻化。”，窗口保持打开。
2. 手柄按 `X`（SMF 3.18+ 由框架翻译；3.9 走 raw fallback）：行为与键盘一致。
3. 点窗口底部 **Remove outfit**：行为与 X 完全一致（同一个 helper）。
4. 搜索框输入文字时按 `X`：应只输入字符，不取消幻化。
5. 底部提示与按钮 tooltip 应显示 `X: remove` / `（X）`；中文语言下为“X：取消幻化”。

**B. .tri 形变实时刷新**

6. 给角色幻化一套带 `.tri` 的服装（如 OBody 身形），在 OBody 里换一个身形预设：不重新穿脱、不取消幻化，模型应在约 1 秒内按新身形刷新。
7. 用 RaceMenu 的 Body Morph 滑块改胸/臀/腿等：同上，幻化件应实时跟着形变；日志出现 `MorphRefresh: the body morphs of <FormID> changed; rebuilding the illusion.`（该行为 debug 级，需在 Settings 勾选 Verbose logging）。
8. 切到第一人称再切回：第一/第三人称两套 biped 都应使用新形变，不出现一半旧模型。
9. 未安装 RaceMenu 或 BodyMorph 版本低于 4：日志只有一条 `MorphRefresh: ...` 的 info/warn，换装、幻化、取消幻化均不受影响。
10. 未幻化的角色不受影响：morph 变化不会触发无意义的 RefreshActor（日志无对应 FormID）。

**C. 回归**

11. X 键不影响原有 E（穿上）、R（随机）、Esc（关闭）、W/S/A/D 导航。
12. 关闭快速选择窗口后，SMF 主菜单手柄导航仍正常；游戏输入恢复。

### 3.28 1.5.15 幻化循环的观感与速度

| 编号 | 用例 | 预期 |
| --- | --- | --- |
| A1 | OBody 换预设后盯着角色看 | 幻化件直接变成新身形，**看不到**原版装备的任何一帧 |
| A2 | 同上，看日志顺序 | `taking the illusion ... off for one pass (body morph changed)` 与 `restored with a fresh morph target` 相邻出现 |
| A3 | 同上，观察卡顿 | 与在快选窗里选中另一套并确认的手感相当（约一次刷新的停顿） |
| B1 | 幻化带 `.tri` 的衣服后按 Z 重穿当前套装 | 立即刷新（不等 250 ms 心跳），无原版装备闪现 |
| B2 | 快选窗选中当前套装按 E | 同 B1 |
| B3 | 连续快速换 3 个预设 | 每次都是一次干净切换，不出现来回抖动或只剩头手 |
| C1 | `[Compatibility] ReillusionGapMs = 30` 后换预设 | 退化为两段式：会短暂看到原版装备（预期行为，用来说明同帧路径确实生效） |
| C2 | 设回 0 | 恢复无闪烁 |

| 判定 | 说明 |
| --- | --- |
| 若 A1 仍看到原版装备 | 说明该机型上同帧拿不到新形变，属已知退路场景，把 `ReillusionGapMs` 设为 20–50 并回报 |
### 3.29 1.5.16 幻化栏复制 / 粘贴整套预设

**A. 复制**

| 编号 | 用例 | 预期 |
| --- | --- | --- |
| A1 | 选一套含多件护甲、改过基础策略与若干单槽策略的套装 → 点 Copy | HUD「幻化已复制到剪贴板。」；日志 `Outfits: copied 'X' to the clipboard (N bytes of JSON).` |
| A2 | 把剪贴板内容贴到记事本 | 是单个 JSON 对象：`name` / `isFavorite` / `armors[{rawFormId,localFormId,modName}]` / `slotPolicies{"9":8}` / `slotPolicy:2`，普通 UTF-8 文本 |
| A3 | 没有选中任何套装时点 Copy | HUD「请先在左侧选中一套幻化。」，不写剪贴板 |

**B. 粘贴**

| 编号 | 用例 | 预期 |
| --- | --- | --- |
| B1 | 复制 A1 那套 → 点 Paste | 新建 `<原名>_copy`；左侧列表自动选中它并滚动到可见位置；HUD「已粘贴幻化「..._copy」。」 |
| B2 | 再点一次 Paste | 得到 `<原名>_copy2`（重名后缀递增） |
| B3 | 打开 B1 粘贴件的套装编辑器 | 部件列表与源套装一致；基础策略与 32 槽单槽策略逐项一致（例如「需要已装备」仍是「需要已装备」、盾牌「穿透」仍是「穿透」） |
| B4 | 用记事本把 `OutfitSystemData.json` 里一个 outfit 对象整段复制到剪贴板 → Paste | 同样能建出套装，策略按数字枚举还原 |
| B5 | 复制整个 `{"enabled":true,"outfits":[...]}` → Paste | 取 `outfits` 数组第一项建出套装，不报错 |
| B6 | 剪贴板放普通文字 / 空剪贴板 → Paste | 分别提示「剪贴板内容不是有效的 JSON。」「剪贴板里没有文本。」，列表不变 |

**C. 回归**

| 编号 | 用例 | 预期 |
| --- | --- | --- |
| C1 | 粘贴后在快选窗（Ctrl+Shift+L）里查找新套装 | 列表里有新套装，可直接穿上，外观正常 |
| C2 | 复制含超长名字（>256 字节）的套装再粘贴 | 名字按 UTF-8 边界截断后加 `_copy`，不崩、不出现半个汉字 |

### 3.30 1.5.17 SLIF NG 膨胀框架兼容

**A. 接入**

| 编号 | 用例 | 预期 |
| --- | --- | --- |
| A1 | 装 SLIF NG 0.4.18+，启动后看日志 | 出现 `MorphRefresh: SLIF NG query interface v2 acquired (N tracked actors); inflation changes will rebuild the illusion.` |
| A2 | 不装 SLIF NG | 出现 `MorphRefresh: SLIF NG is not installed; inflation changes are not watched directly (the morph signature still covers them).`，换装与幻化不受影响 |
| A3 | 装 SLIF NG 0.4.17 及更老（没有 C 导出） | 退回 `0x534C4946 'SLIF'` 消息握手；仍拿不到就只报 not installed，其它后端继续工作 |
| A4 | RaceMenu / OBody / SLIF NG 同时安装 | 三条 acquired 日志可以同时出现；只要有一条可用，就不应打印 `no body morph provider is available` |

**B. 膨胀跟随**

| 编号 | 用例 | 预期 |
| --- | --- | --- |
| B1 | 给已幻化的 actor 加 SLIF 膨胀（belly） | 幻化件随身体一起鼓起、不穿模；Verbose 日志成对出现 `taking the illusion of <FormID> off for one pass` 与 `illusion of <FormID> restored with a fresh morph target.` |
| B2 | 继续加深膨胀 | 每段 ramp 最多触发一次重建；画面不闪出原版装备 |
| B3 | 膨胀回落 / 取消膨胀 | 幻化件同步缩回 |
| B4 | 用 SLIF 控制台读 `GetApplied` | 数值与外观一致（node 目标 1.0 中性、morph: 目标 0.0 中性） |

**C. 去抖与节流**

| 编号 | 用例 | 预期 |
| --- | --- | --- |
| C1 | 连续快速调整膨胀 | 同一角色两次幻化循环至少间隔 600 ms（`kMinCycleInterval`），不刷屏 |
| C2 | 膨胀值仍在连续变化 | 静置 250 ms 前不重建（`kSignatureSettle`）；停稳后补一轮 |
| C3 | 关掉 SLIF 后正常换装 | 幻化循环仍由换预设 / 重穿触发，行为与 1.5.16 一致 |

**D. 回归**

| 编号 | 用例 | 预期 |
| --- | --- | --- |
| D1 | 1.5.16 的全部用例（复制/粘贴、快选窗、X 键、状态换装） | 均不受影响 |
| D2 | 用户此前必须手动做的「先取消幻化再穿回」 | 仍不需要手动操作 |

## 3.37 1.5.24 贴件回到引擎的 ActorWeightModel（只显示头手 / 46 槽外露）

| 编号 | 操作 | 预期 |
| --- | --- | --- |
| A1 | 装 1.5.24，进游戏，穿一套普通护甲，幻化一套覆盖身体/手/脚/头多件的套装 | 人物完整显示幻化外观，不再只有头手 |
| A2 | 看日志每次贴件的块 | `Custom: attaching N outfit-only piece(s) of M displayed (mask 0x..., ...)`，随后每条 `Custom: ... slot NN clone 0x...` 都是 **非 0** 指针 |
| B1 | 反复切三套不同幻化（含只覆盖部分槽的） | 每次切换外观正确；日志里同一槽的 `clone` 指针发生变化 |
| B2 | 从「覆盖 46 槽」的套装切到不覆盖 46 槽的套装 | 原装备 46 槽**不再**露出（1.5.24 已删除「还原真甲」）；日志不再有 `restored N real piece(s)` |
| C1 | 取消幻化 | 立刻回到真实装备外观 |
| C2 | 第一人称 / 第三人称各看一次 | 两个视角都有幻化，无「第三人称有、第一人称裸」 |
| D1 | 若日志出现 `Custom: ApplyArmorAddon refused ...` | 该槽空着；把该行 + `slot NN clone 0x0` 发回（说明这件护甲的 AA 在这个种族/性别下没有可用模型，或模型指针仍不对） |
| D2 | 若日志出现 `Custom: the weight model 0x... does not point at a biped model of ...` | 只影响遗留槽清理；贴件应仍正常。持续出现则把该行发回 |
| E1 | 反例检查 | 1.5.24 的 DLL 里不应再有 `restored N real piece(s)` 与 `Custom: rebuilding N display pieces`（旧措辞）；应有 `outfit-only piece(s) of` |
| E2 | RaceMenu 拖 Body Morphs / OBody 换预设（若已能握手） | 幻化件随形变变化；`.tri` 相关行为与 1.5.20 一致 |
| F1 | 连续换装 ≥ 20 次 | 无崩溃；`Custom:` 日志不无限增长；不出现空模型 |

## 3.38 1.5.25 离开幻化集的槽不再清空（身体 / 32 槽不再变空模型）

针对 m02242：1.5.24 上「幻化衣服出现但身体消失，卸下 32 槽护甲或切空套装都救不回来」。

| 编号 | 操作 | 预期 |
| --- | --- | --- |
| A1 | 装 1.5.25，进游戏，应用一套覆盖身体/手/脚/头的幻化套装 | 人物完整显示（幻化件 + 原版身体），不出现「只有头手」 |
| A2 | 看日志贴件块 | `Custom: attaching N outfit-only piece(s) of M displayed (mask 0x…, …)` 之后有 `Custom: … slot NN item … addon … part true clone …`（debug）；`addon` 非 0 |
| B1 | 从覆盖 32 槽的套装切到**不覆盖** 32 槽的套装 | 身体（原版皮肤/身体）仍然显示；日志只有 `Custom: N slot(s) left the display set (mask 0x…); left to the engine for …`，**没有 `cleared for …`** |
| B2 | 切到**空**幻化套装（取消全部件） | 立刻回到真实装备外观，身体不空 |
| C1 | 在物品栏里脱掉 32 槽（身体）护甲 | 身体仍是原版身体，不会变成空模型 |
| C2 | 脱下后再穿回同一件 32 槽护甲 | 外观正常，无残留克隆层叠 |
| D1 | 连续切 3 套（覆盖槽数递减），每套看一次 | 递减过程中不出现空模型；不再被接管的槽最多短暂保留上一套的克隆，换装/脱装后被引擎重贴覆盖 |
| D2 | 反复「应用幻化 → 取消幻化」≥ 10 次 | 每次都能完整回到真实装备；无空模型、无崩溃 |
| E1 | 反例检查（DLL 字符串） | 1.5.25 的 DLL 里不应再有 `cleared for` / `ClearBipedPart`；应有 `left to the engine` 与 `slot {:02} item {:08X} addon {:08X} part {} clone {}` |
| E2 | 第一人称 / 第三人称各看一次 | 两个视角身体都正常（1P 与 3P 各有一份 biped，清空策略已不存在，不应再出现 1P 空模型） |
| F1 | 开 Verbose logging 跑一轮 | `Custom:` 行数与贴件次数同阶（每件一行 debug 状态），不会刷屏 |


## 3.39 1.5.26 幻化件跟随身形（`.tri` 目标 + 无接口也能被 RaceMenu morph）

> **1.5.28 纠正（历史说明，不作当前验收依据）**：旧文档将握手归因于“仅消息嵌套”/receiver 名字错位的解释已纠正，实际是注册时机导致 sender 订阅遗漏；主线程 Queue 重试并非完整修法。旧 `carries a .tri morph target on N of M`、`N > 0` 或 `N = M` 判定均不充分，不能证明 TRI shape 匹配或 morph 顶点移动；旧实时成功措辞是当时目标/假设而非实机结论。当前以 1.5.28 最新章节为准。

针对 m00002 / m00270 / m02536：幻化件在 RaceMenu 拖 Body Morphs 滑块、OBody 换预设、SLIF 膨胀时不跟随。1.5.26 起不再以「拿到 skee `BodyMorph` 接口」为前提，改为给每个幻化克隆件无条件贴 `BODYTRI`，由 RaceMenu/OBody 自己的 `ApplyBodyMorphs` 遍历到它。

前置：装 RaceMenu + skee64（版本无所谓，接口可能拿不到）；一套**带 `.tri`** 的护甲（BodySlide 生成过 `.tri`，如紧身衣/裙装）；`ExtraLogging` 按用例开关。

| 编号 | 操作 | 预期 |
| --- | --- | --- |
| A1 | 应用一套带 `.tri` 的幻化套装，看日志 | 出现 `MorphRefresh: <FormID> carries a .tri morph target on N of M cloned slot(s) (…); RaceMenu and OBody morph those clones in their own apply pass - no interface needed.`（每角色一次，info）；`N` > 0 |
| A2 | `showracemenu` → Body Morphs 页签拖体重/胸/腿滑块 | 幻化件**实时**跟着变形，中途不闪原版护甲；松手后形状与滑块一致，不需要确认或重穿 |
| B1 | OBody NG 换另一个身形预设（幻化着同一套衣服） | 幻化件随新预设变化；不出现「只有真皮肤变、衣服还是旧身形」 |
| B2 | SLIF NG 加膨胀（或任意 inflation 预设） | 幻化件跟着膨胀，取消膨胀后复原 |
| B3 | 拖滑块 → 关掉 RaceMenu 窗口 → 再开 → 再拖 | 每次都跟随；关闭窗口时幻化件保持最后形状，不回落 |
| C1 | 开 Verbose，看日志 | 每 5 秒最多一条 `MorphRefresh: … has no BodyMorph interface; N .tri target(s) of M slot(s) stay armed for RaceMenu's own pass (…)`（debug，无接口时的正常状态）；若拿到接口则是 `re-morphed … the vertices moved` |
| C2 | 看 kDataLoaded 时的 info 行 | `MorphRefresh: .tri morph targets are attached to every illusion clone; RaceMenu and OBody morph them in their own pass. The provider interface only adds the immediate re-morph.`；**不应**再出现 1.5.20 的 `.tri morph target support active/inactive` |
| D1 | 幻化一件**没有** `.tri` 的护甲（BodySlide 没做过），拖滑块 | 该件不跟随（预期行为，见 §7 限制）；其它带 `.tri` 的件照常跟随；日志 `N < M` |
| D2 | 幻化一件模型名与 `.tri` 推导不符的护甲（如自定义 meshes 路径） | 该件不跟随，`N < M`；其余件正常。把护甲 FormID 与模型路径发回可加白名单 |
| E1 | 反例检查（DLL 字符串） | 应有 `carries a .tri morph target on` / `no interface needed` / `stay armed` / `.tri morph targets are attached to every illusion clone` / `only adds the immediate re-morph` / `has no BodyMorph interface;`；**不应**有 `.tri morph target support` |
| F1 | 不装 RaceMenu（只装 OBody NG）重复 A1/B1 | 仍有 `carries a .tri morph target on N of M …`；OBody 换预设时幻化件跟随（OBody 自己会调 `ApplyBodyMorphs`） |

## 3.40 1.5.27 `.tri` 目标真的贴上了（模型路径改问引擎 + 候选名全试 + 武装后不再摘幻化）

针对 m02992：1.5.26 装上后日志里 `MorphRefresh: 00000014 has no BodyMorph interface; 0 .tri target(s) of 5 slot(s) stay armed …` 出现 5 次，一整局一个 `BODYTRI` 都没贴上。1.5.27 改为从 `biped->objects[i].addon`（引擎解析出的 ARMA）取模型路径、四个 `.tri` 候选名全试、`.nif` 自带的目标也计入，并在武装后停用幻化循环。

前置：与 §3.39 相同（RaceMenu + skee64、一套**带 `.tri`** 的护甲），`ExtraLogging = true`。

| 编号 | 操作 | 预期 |
| --- | --- | --- |
| A1 | 应用一套带 `.tri` 的幻化套装，看日志 | 每个幻化件一条 `MorphRefresh: <armor> slot model <path> (from the armour addon the engine attached) uses .tri <file>.`（debug） |
| A2 | 看该角色第一次的 `carries a .tri morph target on N of M cloned slot(s)` | `N` 应等于幻化覆盖的槽数（0x00010086 那套是 4），不再是 `0 .tri target(s)` |
| B1 | `showracemenu` 拖 Body Morphs 滑块 | 幻化件实时跟随；日志不再出现 `taking the illusion off for one engine pass` |
| B2 | OBody NG 换预设 | 幻化件随新预设变形；`the provider pass moved the meshes …` 或 `left … alone, but the illusion carries .tri targets`（都不再摘幻化） |
| B3 | SLIF NG 加膨胀 / 取消 | 幻化件跟着膨胀并复原 |
| C1 | 开 Verbose，拖滑块时看日志 | 最多出现 `the illusion of <FormID> carries .tri targets; leaving the clones to the next morph pass instead of cycling.`（debug）——**这是 1.5.27 想要的状态**，说明不再摘幻化 |
| C2 | 切套装（换一套同样带 `.tri` 的） | 新套装同样先打 `uses .tri <file>`，再打 `carries a .tri morph target on N of M`；不出现原版护甲闪现 |
| D1 | 幻化一件**没有** `.tri` 的护甲 | 日志 `MorphRefresh: <armor> slot model <path> (from …) has no .tri beside it, so RaceMenu cannot morph that slot. Probed: a, b, c, d.`；该件不跟随（预期），其它件正常 |
| D2 | 幻化一件 `.tri` 名不在四个候选里的护甲 | 同上；把该行发回（含模型路径与试过的四个名字）可加白名单 |
| E1 | 反例检查（DLL 字符串） | 应有 `uses .tri ` / `has no .tri beside it` / `Probed: ` / `the armour addon the engine attached` / `leaving the clones to the next morph pass instead of cycling` / `carries a .tri morph target on`；**不应**有 `.tri morph targets are attached to every illusion clone`（1.5.26 措辞） |
| F1 | 自定义种族角色（如自定义种族 mod）重复 A1/B1 | 1.5.27 的模型路径不再依赖 `GetArmorAddon(race)`，自定义种族下 `<path>` 仍应有值；若为 `has no armour addon yet`，说明引擎尚未解析该槽 ARMA |
## 3.41 1.5.29 场景结束后的残留幻化与主线程对账（m05502）

前置：OStim 或 SexLab 可用；准备两套幻化套装 —— A = `_sex` 场景套装（覆盖槽与普通套装不同，例如含 CK 槽 34），B = 普通套装；两者都用带 `.tri` 的护甲；日志开 Verbose。

- **A1 场景中幻化生效**：触发场景 → 场景开始时自动切到 A → 角色身上应显示 A 的幻化件（不是裸体）。日志：`Patches.cpp … Custom: attaching N outfit-only piece(s) of M displayed (mask 0x…)`，掩码里包含 A 独有的槽位。
- **A2 场景中打开快换窗不破坏幻化**：场景中按 Tab/Z 打开快速换装窗 → 关闭 → A 的幻化件仍在。
- **B1 场景结束自动恢复（本版重点）**：结束场景 → 自动切回 B → **不要按任何键**，模型应在一次刷新内变成 B 的外观；A 独有的槽（如 CK 槽 34）不得残留 A 的件。日志判定：出现 `Custom: detached N illusion clone(s) whose slot left the display set (mask 0x…)`（我们上一套的克隆被拆）或 `MorphRefresh: … lost the illusion in slot mask 0x…; rebuilding the actor's equipment.`（对账触发重建）之一。
- **B2 不再需要第二次按键**：B1 之后按 Tab/Z 打开快换窗再关闭，外观**不应**发生变化（说明第一次就已经对了）。
- **B3 反向验证：不得拆引擎的槽**：切到一套比当前**少**覆盖若干槽的幻化（不含身体槽）→ 那些少掉的槽里如果是真实穿戴的护甲，必须仍显示真甲；日志应是 `left to the engine` 而不是 `detached`。身体（CK 槽 32 / 掩码 `0x4`）在任何切换后都必须是正常身体，不得空模型（1.5.25 回归项）。
- **C1 对账不误伤**：正常换装、读档、反复切套装 20 次 —— 日志里不应出现 `lost the illusion in slot mask`（说明幻化没有掉），也不应出现 `after 3 rebuilds`。
- **C2 对账的收敛**：故意制造一次「幻化集有该槽但没有件」（例如套装里放一件模型已丢失的护甲）→ 应看到最多 3 次 `lost the illusion … rebuilding`，然后一条 `still holds no illusion geometry … after 3 rebuilds; it is left alone from now on.`，之后**不再刷屏**，游戏不卡不崩。
- **D1 框架无关**：不装 OStim/SexLab，用 SAM 或 OBody 换身形 + 手动切套装触发同样的「幻化集说有、biped 没有」→ 对账同样应生效（判据同 B1）。
- **E1 崩溃回归**：按 m05502 A 组顺序（场景中不改装 → 结束场景）连续 10 次，不应崩溃。若崩溃，提交 `crash-*.log` + 同一次会话的 `SkyrimOutfitSystemNG.log`；重点看崩溃前最后一次 `detached` / `rebuilding` 行的时间戳与崩溃时间是否同一帧。
- **F1 空幻化回归**：切到空套装 → 应显示裸体（1.5.25 已验收行为），不得出现任何幻化件残留，也不得出现空模型。

## 3.42 1.5.30 切换套装后自动补跑装备 pass（m06712）+ 模块改名（m06781）

前置：准备 A = `_sex` 场景套装（覆盖槽与普通套装不同）、B = 普通套装、C = 空套装；OStim 或 SexLab 至少一个；日志开 Verbose（debug 才看得到 `repeating the equip pass`）。

- **A1 场景开始不必弹窗**（本版重点）：触发场景 → 自动切到 A → **全程不按 Tab/Z**，角色身上应在 1 秒内显示 A 的幻化件。日志：三行 `MorphRefresh: repeating the equip pass for <FormID> (follow-up 1 of 3)` / `2 of 3` / `3 of 3`，每行紧跟 `OutfitSystem.cpp … Armor Refresh for <name>` 与 `Custom: attaching N outfit-only piece(s) …`。
- **A2 不能再出现「按一次窗才出来」**：A1 之后按 Tab/Z 开窗再关闭，外观**不应**变化（说明第一次就已经对了）。若仍要按键才更新，把该段日志发回：重点看三次 follow-up 是否都在场景切换后 1 秒内发生、以及当时 `Illusion display is active` 是否成立。
- **B1 场景结束自动回滚**：结束场景 → 自动切回 B → 不按键，模型一次刷新内变成 B；A 独有槽（如 CK 槽 34）不得残留（与 1.5.29 的 `detached` / `left to the engine` 判据叠加看）。
- **B2 三次 follow-up 是有限次**：日志里每个切换点最多 3 行 follow-up，**不得**出现刷屏或随时间无限重复；`follow-up 4 of 3` 之类不存在。
- **B3 幻化关闭时不补跑**：把 `Hooks` 那侧的幻化总开关打到关闭态（例如临时换回只有一半钩子的构建）→ 日志出现 `Illusion display is off for this session` → 此后不应有任何 `repeating the equip pass`（`DrainOutfitPasses()` 首行即返回）。
- **C1 菜单路径不重复加倍**：点快速换装窗切套装 20 次 —— 每次仍只有 3 行 follow-up（`Features::RefreshActor()` 是唯一入口，不得出现同一次切换两套 follow-up 交错）。
- **C2 无递归**：日志中 `Armor Refresh` 的间隔应稳定在 ~120/400/900 ms 三档；若出现「Armor Refresh → repeating → Armor Refresh → repeating …」无限链，说明有入口把 `refreshArmorFor` 又带回了 `Features::RefreshActor`，属回归。
- **D1 加载稳定**：装新 DLL（`OutfitSystemNG.dll`，**旧名 `SkyrimOutfitSystemNG.dll` 必须删除**）→ 日志仍写入 `SkyrimOutfitSystemNG.log`（插件身份不变），Papyrus 翻译文件名不变，存档可继续用。
- **D2 双加载反例**：故意把旧名与新名同时放进 `Data/SKSE/Plugins` → 应能观察到两份插件实例（日志重复初始化 / 菜单重复），这是**错误配置**；按 D1 删掉旧名后恢复正常。
- **E1 性能**：1000+ 套装列表下按 C1 反复切 20 次，帧率与 1.5.29 相比无可感知下降；三次 follow-up 不得在菜单打开时造成输入卡顿。
- **F1 基础幻化回归（1.5.25 验收项）**：普通换装 / 空幻化 C / 身体槽（CK 槽 32）均正常，不得出现空模型或残留件；`Custom: detached` 与 `lost the illusion in slot mask` 不应因本版而新增。

## 3.43 1.5.31 场景切换不再需要弹窗（模型重建 + 场景入口改写）（m07050）

前置：A = `_sex` 场景套装、B = 普通套装；OStim 或 SexLab 至少一个；开 Verbose（`Model Rebuild for` 与 `asking the game to rebuild` 是 debug）。1.5.30 的随访判据（`repeating the equip pass … follow-up N of 3`）仍然适用，但**只有它不够** —— 本版新增的判据是「随访链第 2 步会请求重建 3D」。

- **A1 场景开始不必弹窗**（本版重点）：触发场景 → 自动切到 A → **全程不按 Tab/Z**，角色应在 1 秒内出现 A 的幻化件。日志顺序：`Armor Refresh for …` → `Custom: attaching N outfit-only piece(s) …` → `MorphRefresh: asking the game to rebuild the model of <FormID> … (follow-up 2 of 3).` → `Model Rebuild for …`。
- **A2 重建之后不再需要第二次**：A1 之后按 Tab/Z 开窗再关，外观**不应**变化。若仍要按键才出来，说明重建时机不够（把该段 40 行日志发回，重点看 `Model Rebuild for` 是否真的出现在 `attaching` 之后）。
- **B1 场景结束自动回滚**：结束场景 → 自动切回 B → 不按键，模型一次刷新内变成 B；A 独有槽（如 CK 槽 34）不得残留（与 1.5.29 的 `detached` / `left to the engine` 判据叠加看）。
- **B2 场景入口走的是通用函数**：开 Verbose 后，每次场景开始/结束都应看到一次 `Armor Refresh` 与三次随访 —— 与「点快速选择窗口切同一套」的日志形态一致（本版把 `SceneOutfits` 改成调用 `Features::AssignOutfit()`）。
- **B3 套装不存在时不静默**：把 `_sex` 套装删掉再触发场景 → 日志 `SceneOutfits: '<outfit>' could not be assigned for the OStim scene; the current outfit is kept.`（warn），幻化保持原样、不反复重试。
- **C1 重建是有限次**：同一角色连续切换 20 次，每次切换最多一次 `Model Rebuild for`；不得出现 `Model Rebuild` 与 `Armor Refresh` 相互触发的无限链。
- **C2 幻化关闭时不重建**：幻化总开关关闭（日志 `Illusion display is off for this session`）时，不应出现 `Model Rebuild for`（随访在 `!IllusionDisplayEnabled()` 时就返回）。
- **D1 无 3D 时不报错**：切换发生在角色 3D 尚未加载时（例如快速读档瞬间）→ 日志 `Skipping model rebuild for <name>: no 3D yet.`（debug），不崩、不重试成风暴。
- **D2 第一人称**：切到第一人称再切套装，模型重建不得把 1P 手/身体弄成空模型（重建只动 3D 模型，不动 biped 槽）。
- **E1 性能**：`Update3DModel()` 相当于开关一次菜单的开销；场景切换时不得造成可感知卡顿（若明显卡顿，把切换时刻的帧时间与 3 行随访日志一起发回）。
- **F1 基础幻化回归（1.5.25 验收项）**：普通换装 / 空幻化 / 身体槽（CK 槽 32）均正常，不得出现空模型或残留件；`Custom: detached` 与 `lost the illusion in slot mask` 不应因本版新增。

## 3.44 1.5.32 场景套装只做切换（删除补跑与重建）（m07382 / m07424）【历史：1.5.33 已改为 setOutfitByName + refreshArmorFor】

前置：A = `_sex` 场景套装、B = 普通套装；OStim 或 SexLab 至少一个；开 Verbose。本版删除了 1.5.30 的补跑随访与 1.5.31 的模型重建，**判据是「这些行不应再出现」**，加上「切换本身仍然执行」。

- **A1 场景开始不必弹窗**（本版重点）：触发场景 → 自动切到 A → **全程不按 Tab/Z**，看 A 的幻化件是否出现。日志只应有 `SceneOutfits: … scene started; 'B' -> 'B_sex'` 紧跟 `Armor Refresh for …` 与 `Custom: attaching N outfit-only piece(s) …`。
- **A2 不再有补跑/重建**：A1 之后日志里**不得**出现 `repeating the equip pass for` / `follow-up N of 3` / `Model Rebuild for` / `asking the game to rebuild the model of`（字符串已从 DLL 删除）。出现即说明装的不是 1.5.32。
- **A3 场景结束自动回滚**：结束场景 → 自动切回 B → 不按键；若模型没变回，把日志发回（本版不再提供任何二次刷新兜底，这是有意的：用户要求只做切换）。
- **B1 名字查找**：把 `_sex` 套装改成大小写不同的写法（如 `X_SEX`）→ 仍应命中；把 `_sex` 套装删掉 → 日志 `SceneOutfits: no 'X' outfit for the … scene; create an outfit with exactly that name to use it here.`（info），幻化保持原样、不重试。
- **B2 同步执行**：场景开始/结束的 `SceneOutfits:` 行与随后的 `Armor Refresh` 应在**同一个日志时间戳毫秒**内（同步路径）；若两者相隔 >100 ms，说明走的是任务队列分支（非游戏线程事件），属预期降级。
- **B3 重入安全**：同帧内先 `ostim_end` 后 `ostim_start`（连续两次场景）→ 不得死锁、不得崩；日志应有两条 `SceneOutfits:` 行且各自带 `Armor Refresh`。
- **C1 无递归**：一次场景切换只产生**一轮** `attaching`（3P + 1P 各一次），不得出现切换→刷新→再切换的循环。
- **C2 幻化关闭时不动作**：日志 `Illusion display is off for this session` 时，场景事件仍会调 `AssignOutfit`，但不得贴件、不得崩。
- **D1 套装名带后缀套后缀**：当前套装本身叫 `X_sex` 时，进入场景**不应**去找 `X_sex_sex`（沿用 `EndsWithSexSuffix` 的早退），幻化保持不变。
- **E1 基础幻化回归（1.5.25 验收项）**：普通换装 / 空幻化 / 身体槽（CK 槽 32）均正常，无空模型、无残留件；`Custom: detached` 与 `left to the engine` 行为与 1.5.29 一致。
- **F1 菜单路径不变**：快速选择窗口按 E/Z 切同一套，日志形态应与场景路径完全一致（两者共用 `Features::AssignOutfit`）。

## 3.45 1.5.33 场景套装换成「状态切换那套方法」（m07666）

前置：装 `Data/SKSE/Plugins/OutfitSystemNG.dll`，旧名 `SkyrimOutfitSystemNG.dll` 已删除；准备一套与当前套装同名 + `_sex` 的套装（例如当前 `A` → 存在 `A_sex`）。

- **A1 场景开始不必按键**：进 OStim / SexLab 场景，**不要按 Tab / Z**，看角色是否直接换上 `_sex` 套装（含身体/手/脚槽，不只是头手）。
- **A2 日志依据（info，无需 Verbose）**：应看到 `SceneOutfits: OStim scene started; 'A' -> 'A_sex'.`，同一瞬紧跟 `Armor Refresh for A_sex` + `Custom: attaching N outfit-only piece(s) …`。
- **B1 场景结束不必按键**：场景结束，**不要按任何窗口**，看模型是否直接变回普通幻化；日志应有 `SceneOutfits: OStim scene ended; 'A' restored.` + 紧随的 `Armor Refresh for A`。
- **B2 不再有旧机件**：全程**不得**出现 `repeating the equip pass for` / `follow-up N of 3` / `Model Rebuild for` / `asking the game to rebuild the model of` / `scene event dropped`（这些字符串已从 DLL 删除）。出现即说明装的不是 1.5.33。
- **B3 线程守卫**：正常不应出现 `arrived off the game thread; the scene switch is skipped.`；若出现，记录发事件的 mod 与事件名。
- **C1 名字对不上**：删掉 `A_sex` 后进场景，应看到 `SceneOutfits: no 'A_sex' outfit for the … scene; create an outfit with exactly that name to use scene switching. The current outfit is kept.`，且当前幻化保持不变（不残留半套）。
- **C2 大小写不敏感**：把套装名改成 `a_SEX`（或其它大小写组合），场景仍应切换成功（日志里显示套装真实名）。
- **D1 基础幻化回归**：不进场景，直接用快速选择窗口（Tab/Z）与菜单点击切套装，外观必须与 1.5.25 实测结果一致（含空幻化套装）；日志正常出现 `Custom: attaching …`。
- **D2 菜单路径与场景路径一致**：同一套装的菜单切换与场景切换，日志里都只应出现一次 `Armor Refresh` + 一次 `Custom: attaching …`（不应出现重复的补跑链）。
- **E1 性能**：连续进出场景 10 次，切换后 1 秒内模型必须更新；内存与帧率无异常（场景入口不再有 SKSE 任务队列与 `Tick()`）。
- **F1 状态切换未受影响**：走「按地点自动切换」换一个地点，幻化应正常切换（该模块本版一行未改；若这里出问题说明不是本版引入）。

## 3.46 1.5.34 场景事件交接到游戏线程（删除线程门禁丢弃）

对应 m07842。判定前提：装 `Data/SKSE/Plugins/OutfitSystemNG.dll`（1.5.34.0），旧名 `SkyrimOutfitSystemNG.dll` 已删；看事件行需要开 Verbose（debug 级）。

- **A1 场景开始不按键就切**：进 OStim 场景，**不要按 Tab / Z**。日志应见 `SceneOutfits: 'ostim_start' came from thread <工作线程> (game thread <主线程>); the switch is handed to the game thread.`，随后（下一任务周期）`SceneOutfits: OStim scene started; 'A' -> 'A_sex'.`。
- **A2 场景结束不按键就回滚**：结束场景，`ostim_end` 同理交接，日志见 `SceneOutfits: OStim scene ended; 'A' restored.`。
- **A3 丢弃行不再出现**：全日志搜索 `arrived off the game thread; the scene switch is skipped.` 必须为 0 条；若命中说明装的不是 1.5.34。
- **B1 换装序列不变**：`scene started` 之后应有 `Armor Refresh for …` 与 `Custom: attaching N outfit-only piece(s) of M displayed (mask 0x…)`；不应出现 `repeating the equip pass for` / `follow-up` / `Model Rebuild for`（1.5.30/1.5.31 机件已删）。
- **B2 游戏线程事件仍内联**：若事件本身就来自主线程，不应出现交接 debug 行，直接看到 `scene started`；两条路径共用同一个 `Dispatch()`。
- **B3 名字对不上时不换装**：把 `_sex` 套装改名，日志应见 `SceneOutfits: no '…' outfit for the … scene; create an outfit with exactly that name to use scene switching. The current outfit is kept.`，当前套装保持不变。
- **C1 连续进出场景**：场景内反复开始/结束，每次各有一条 `scene started` / `scene ended`；`g_switchMutex` 保证不交叉执行，不得死锁或卡顿。
- **C2 场景中手动切套装**：场景进行中用快捷窗切到别的套装，场景结束仍按「场景开始时记录的那个套装名」恢复；恢复目标是开始时记录的名字。
- **D1 任务接口缺失的降级**：极端情况下只见 `… there is no task interface, so the switch cannot be handed over; the scene outfit is left as it is.`，不换装、不崩、其它功能不受影响。
- **E1 基础幻化回归**：菜单里切普通套装、空套装仍照 1.5.25 的表现（`Armor Refresh` + `attaching N outfit-only piece(s)`，身体槽不再空）。
- **F1 静态守卫**：`tests/scene_outfit_switch.ps1` 必须通过 —— 场景模块只做「名字查找 → `setOutfitByName()` → `refreshArmorFor()`」，`tasks->AddTask(` 恰好一处且内容是 `[eventName]() { Dispatch(eventName); }`，无队列 / 无 `Tick()` / 无补跑 / 无重建。

## 3.47 1.5.39 逐槽策略按套装独立 / 从 ESP 生成套装 / 金件页

对应 m09191、m09426。判定前提：装 `Data/SKSE/Plugins/OutfitSystemNG.dll`（1.5.39.0），旧名 `SkyrimOutfitSystemNG.dll` 已删；金件数据文件在 `Data\SKSE\Plugins\OutfitSystemGoldenItems.json`。本机未实机验证，下列用例待用户执行。

- **A1 逐槽策略只展开被选中的套装**：建 A、B 两套，选中 A 勾「逐槽策略编辑器」→ 编辑器展开；切到 B → **B 的编辑器应是收起的**（1.5.38 及以前会一起展开）；切回 A → 仍展开。
- **A2 逐槽策略不串数据**：A 里给某个槽设一条覆盖策略，切到 B 看同一槽的复选框 —— 应保持 B 自己的值；导出 JSON，`slotPolicies` 只出现在 A 的记录里。
- **A3 旧 JSON 兼容**：导入 1.5.34 时代导出的 JSON（含 `slotPolicy` / `slotPolicies`），套装列表与逐槽策略应与导出时一致。
- **B1 ESP 列表按加载顺序倒序**：幻化页点「从 ESP 生成套装」→ 列表里最后加载的 ESP 在最上面；搜索框输入插件名片段能过滤（大小写不敏感）。
- **B2 生成结果与文件名**：选一个含 3 件上衣（槽 32）+ 2 条裤子（槽 49）+ 1 个 FX 特效的 ESP → 生成 `<ESP名称>` 与 `<ESP名称>_1` 两套；第一套 3 件上衣配 2 条裤子（第 2 条裤子复用第 0 件），**FX 那条不进任何一套**。
- **B3 生成后焦点与策略**：生成完左侧列表焦点自动跳到第一套生成的套装；每套的槽位策略是「需要已装备（槽位有装备时才显示幻化）」；勾了「生成的套装自动收藏」时新套装带收藏星标。
- **B4 重复生成的覆盖行为**：同一个 ESP 再生成一次 → 同名套装原地覆盖（不产生 `_2` 之外的新重复），已手动改过的件被覆盖（这是设计行为，需在复测记录里注明）。
- **C1 服装过滤三条判据**：找一个含「只覆盖 FX 槽（CK 槽 61）」「没有 EditorID」「armor addon 没模型」记录的 ESP → 这些记录都不出现在生成的套装里；日志 `ESP outfits: …; N wearable of M slot record(s)).` 中 M > N。
- **C2 金件不过滤**：同一个 ESP 用金件页「添加某个 ESP 的全部装备」→ **FX 件与无 EditorID 的件都能加进来**；右侧每条显示 `plugin:localid` 与 `[槽位 …]`（FX 件显示槽位 61）。
- **D1 金件按 FormID 添加**：在选中组的输入框填 `000800`（或 `Plugin.esp:000800`）→ 添加成功并落盘；填不存在的 ID → 提示找不到记录；填非十六进制 → 提示 `That is not a hexadecimal form id.`。
- **E1 数据分离与迁移**：改金件后 `OutfitSystemGoldenItems.json` 变化，但 `OutfitSystemData.json`（幻化）不应因金件改动被迫重写；旧 `SkyrimOutfitSystemSE.ini` 里若有 `goldenItems`，首次加载后应迁移进 JSON 且 ini 写 `GoldenItemsMigrated`。
- **F1 基础幻化与场景套装回归**：菜单切普通套装 / 空套装仍按 1.5.25 表现（`Armor Refresh` + `attaching N outfit-only piece(s)`，身体槽不空）；进 OStim 场景仍按 1.5.34 表现（`came from thread … handed to the game thread` + `scene started`）。
- **G1 静态守卫**：`tests/esp_outfits_filter.ps1` 必须通过 —— 套装生成走 `PluginScan::IsOutfitArmor()` 且分桶剔除 `kFxBit`，金件路径不调用 `IsOutfitArmor()`。

## 3.48 1.5.40 ESP 列表为空 / 金件页闪退（两处回归）

> 目标：证明点「Choose a plugin...」能看到 ESP；点「Add every piece of a plugin」不再崩，且金件页左右两栏可用。静态侧 `tests/golden_menu_guards.ps1` 已覆盖占位符与判据，本表是实机步骤。

| 编号 | 步骤 | 期望 | 失败判据 |
| --- | --- | --- | --- |
| A1 | 幻化页点「Choose a plugin...」 | 出现 ESP 列表，条目数与你启用的插件数同量级（不只是几个） | 列表仍为空 ⇒ 把日志里 `PluginScan: N of M … count as clothing (U …, F …, A …, X …)` 整行发回，看 `X without a model` 是否接近 M |
| A2 | 选中一个只做女性服装的 ESP | 日志 `ESP outfits: … ; N wearable of M slot record(s)` 里 **N > 0**（1.5.39 这里是 0） | N 仍为 0 ⇒ 该 ESP 的记录名字与模型都不在判据内，附那条日志 |
| A3 | 选中含 3 件上衣 + 2 条裤子 + 1 个 FX 特效的 ESP | 生成 `<名称>` 与 `<名称>_1` 两套；FX 那条不进任何一套 | FX 件进了套装 ⇒ `mask & ~kFxBit` 或 `kFxOnly` 判据被绕过 |
| B1 | 金件页点「Add every piece of a plugin」→ 选一个 ESP | **不闪退**；状态行 `Golden items: N piece(s) of <plugin> added.` | 仍崩 ⇒ 抓 `crash-*.log`，看 RVA 是否还在 `RenderGolden` 内 |
| B2 | B1 之后看左侧组列表 | 能点中刚加的组（高亮），条目带 `(N)` 计数 | 点不中 ⇒ 组名大小写/失效条目导致选中态被清，附日志 |
| B3 | B2 之后看右侧编辑器 | 显示 `plugin:LOCALFORMID  名称`、`[slot 30/32/…]`、每件一个 Remove | 空白 ⇒ 同上，附 `Golden items: ` 行 |
| C1 | 界面语言中文，进游戏一次 | 日志 **无** `Language: … does not use the placeholders …`，也无 `N translated line(s) were ignored` | 出现 ⇒ 那条译文占位符与英文不一致（护栏已挡住崩溃），把译文行发回 |
| C2 | 手动改 `Data/SKSE/Plugins/SkyrimOutfitSystemNG_zh.txt`，某行把 `%zu` 与 `%s` 换序，重启 | 该行回退英文显示，日志出现护栏警告，**游戏不崩** | 崩 ⇒ 护栏没生效，附日志与那一行 |
| D1 | 基础幻化回归 | 菜单切套装外观立即变；空套装仍是裸；场景套装（1.5.34 已验收）照旧 | 任一回归 ⇒ 本版不该动引擎侧，逐条比对 `git diff v1.5.39 HEAD -- src/cpp/hooking src/cpp/MorphRefresh.cpp src/cpp/SceneOutfits.cpp` 应为空 |
| E1 | 加载稳定 | 进游戏 5 分钟、开合菜单 20 次无 CTD | CTD ⇒ 抓 `crash-*.log` |
| F1 | 性能 | `PluginScan` 索引只在首次进菜单建一次，之后开合菜单无可感卡顿 | 每次开菜单都卡 ⇒ `EnsureIndex()` 的缓存被反复 `Invalidate()` |
## 3.49 1.5.41 启动即闪退（金件存库锁自死锁）

> 目标：装 1.5.41 后**能进游戏**；即便金件库文件坏了，也只是金件列表为空，不再关掉游戏。静态侧 `tests/golden_lock_guards.ps1` 已覆盖锁类型、`TotalCountLocked` 用法、启动加载 try、消息壳；本表是实机步骤。

| 编号 | 步骤 | 期望 | 失败判据 |
| --- | --- | --- | --- |
| A1 | 直接启动游戏（保留 1.5.40 那次写出的 `Data/SKSE/Plugins/OutfitSystemGoldenItems.json`） | **不闪退**，进到主菜单、能读档 | 仍闪退 ⇒ 抓 `crash-*.log`，看 `[RSP+…]` 是否还有 `std::system_error` / `resource deadlock would occur` |
| A2 | 同一次启动看日志 | 出现 `Golden items: N entr(ies) loaded from …`（info） | 没这行也没崩 ⇒ 金件库文件不存在（首次运行属正常），不算失败 |
| A3 | 把 `OutfitSystemGoldenItems.json` 故意改成非法 JSON，重启 | 出现 critical `Golden items: the store could not be loaded (…); the golden list stays empty for this session.`，**游戏能进**，金件列表为空 | 崩 ⇒ 兜底 try 没生效，附 `crash-*.log` |
| B1 | 全日志搜 `Message handler: an exception escaped` | **0 条** | 有 ⇒ 还有别的抛点在消息阶段，把整行与前后 30 行发回 |
| B2 | 进金件页（1.5.40 的两处修复回归） | ESP 列表非空；点「Add every piece of a plugin」不崩，状态行 `Golden items: N piece(s) of <plugin> added.` | 崩或列表空 ⇒ 回退到 §3.48 排查 |
| B3 | 幻化页点「Choose a plugin...」 | 列表非空，日志 `ESP outfits: … ; N wearable of M slot record(s)` 且 N > 0 | N=0 ⇒ 附 `PluginScan: … count as clothing (…) ` 整行 |
| C1 | 菜单切普通套装 / 空套装（1.5.25 基线） | `Armor Refresh` + `Custom: attaching N outfit-only piece(s)`；身体槽不空；空套装仍是裸 | 任一回归 ⇒ 本版不该动引擎侧，比对 `git diff v1.5.40 HEAD -- src/cpp/hooking src/cpp/MorphRefresh.cpp src/cpp/SceneOutfits.cpp` 应为空 |
| C2 | 进 OStim / SexLab 场景（1.5.34 已验收） | `came from thread … handed to the game thread` → `scene started` → `Armor Refresh` | 不切换 ⇒ 附那 20 行 |
| D1 | 连续开合菜单 20 次 + 金件增删 10 次 | 无 CTD、无卡顿 | CTD ⇒ `crash-*.log` |
| E1 | 启动耗时 | 与 1.5.40 无明显差异（金件库解析在 `kDataLoaded`，只解析一次） | 启动显著变慢 ⇒ 看金件库条目数是否异常膨胀 |
| F1 | 静态守卫 | `tests/golden_lock_guards.ps1` 打印 `GOLDEN_LOCK_GUARDS_OK` | 失败 ⇒ 有人把锁改回 `std::mutex`，或在持锁路径加了加锁 getter |

## 3.50 1.5.42 删除幻化套装预设不再闪退（菜单分页切片夹紧）

> 目标：在第 2 页及以后删除套装时不再崩；第 1 页删除保持正常。静态侧由 `tests/menu_page_slice.ps1` 锁住夹紧写法；本表是实机步骤。

| 编号 | 步骤 | 期望 | 失败判据 |
| --- | --- | --- | --- |
| A1 | 进菜单 → 幻化栏，把套装列表翻到**第 2 页**，选中一套自建预设点删除 | **不闪退**；页面立即重画，条数少一条 | 崩溃 ⇒ 抓 `crash-*.log`，看是否仍是 `vector too long` + `RenderOutfits` |
| A2 | 同一次删除后的日志 | 出现 `Outfits: deleted '<名字>': yes.`，其后继续有正常渲染/菜单行 | 有删除行后立刻断日志 ⇒ 还在同一处崩 |
| A3 | 翻到第 3 页及更后面（≥2 套）再删一次 | 同样不崩、页码不会跳出范围（末页删空应回落到上一页） | 页码停在空页或崩 ⇒ 发回该段日志 |
| B1 | 在**第 1 页**删一套（改成 1 页能装下的数量或先筛到只有几套） | 正常删除，不崩 | 崩 ⇒ 说明不是这条切片，按 §8 发回崩溃栈 |
| B2 | 搜索框输入关键字把列表缩到 1 条，再点删除 | 不崩；过滤结果与页码同步（`Menu` 的筛选签名会重建） | 崩或列表卡住 ⇒ 发回日志 |
| B3 | 删除后立刻新建一套同名预设 | 正常创建、出现在列表里 | 异常 ⇒ 发回 |
| C1 | 连续删除 10 套（跨页） | 无 CTD、无卡顿 | CTD ⇒ `crash-*.log` |
| C2 | 删除 + 导入 JSON（`Imported N outfits …`）交替 5 次 | 数据一致、无崩 | 不一致或崩 ⇒ 发回 |
| D1 | 装 1.5.42 启动、读档、换装、进 OStim 场景 | 1.5.41 / 1.5.34 已验收行为不回归 | 任一回归 ⇒ 比对 `git diff v1.5.41 HEAD -- src/cpp/MorphRefresh.cpp src/cpp/SceneOutfits.cpp` 应为空 |
| E1 | 基础幻化（1.5.25 基线） | `Custom: attaching N outfit-only piece(s)`，身体槽不空 | 回归 ⇒ 本版不该动 `Patches.cpp`，先查是不是装错 DLL |
| F1 | 全日志搜 `Message handler: an exception escaped` | **0 条** | 有 ⇒ 还有别的异常逃出回调，发回整行 |

## 3.51 1.5.43 金件页三处：组行可点 / 取消自动导出 / light 插件 id 宽度（m10387）

| 编号 | 步骤 | 期望 |
| --- | --- | --- |
| A1 | 进菜单 →「金件」栏，鼠标点左栏第二、第三个插件组行 | 一点就选中，右侧编辑区立即切到该组（1.5.42 及更早：要反复点同一位置才偶尔命中） |
| A2 | 开 Verbose，点一次组行 | 出现 `Menu: golden group <名字> selected.`（debug）；**没有这行** = 命中矩形问题仍存在，把分辨率与 UI 缩放发回 |
| A3 | 把 `.ini` 里 `OutfitListWidth` 拖到很大再进金件页 | 左右两栏不重叠：左栏自动收窄（`[240, max(240, 可用宽-320)]`），右栏最小 320，两组行仍可点 |
| B1 | 加一件金件（或移除一件），**不点导出** | 数据路径下方出现 `[not saved] The changes live in memory only; press Export to write OutfitSystemGoldenItems.json.`；磁盘上 `OutfitSystemGoldenItems.json` 的修改时间**不变** |
| B2 | 承接 B1，直接退出游戏重进 | 那件改动**不在**（本版就是手动保存）；进去后提示是 `Changes reach the file only when you press Export.` |
| B3 | 再改一件，点「导出到 OutfitSystemGoldenItems.json」，然后重启 | 提示先变回 `Changes reach the file only…`，重启后改动仍在；文件 mtime 变化 |
| C1 | 选一个 light(ESL) 插件（例：`[SSE]Mini Bunny.esp`），点「Add every piece of a plugin」 | 右侧每行显示真实名字 + `[slot 30/32/…]`，**不再**是 `<当前加载顺序里没有>`；状态行 `Golden items: N piece(s) of <plugin> added.` |
| C2 | 承接 C1（用户旧档里那 61 条仍存在时直接启动） | 启动日志 `Golden items: N entr(ies) resolved, 0 missing.`（1.5.43 的 `Normalise()` 把 light id 自动归一化）；不应再有成片 `was not found in this load order` |
| D1 | 在「Add piece of this plugin」里手输同一个件的两种写法（`002E280C` 与 `E280C`） | 两种都能加进来；1.5.43 起存进文件的是归一化后的短 id（light 插件 3–4 位十六进制） |
| E1 | 往金件里加一件**非槽位护甲**（例如只有 FX 槽的记录） | 能加（金件不做服装过滤），名字正常显示，`[slot …]` 显示真实槽号或 `?`；不再提示 `No slot armour with that id…`（1.5.43 文案改成 `No armour with that id was found in that plugin.`） |
| F1 | 回归：切一次普通幻化套装、进一次 OStim 场景看 `_sex` 套装 | 幻化外观正常（1.5.25 行为）、场景套装自动切换正常（1.5.34 行为）；金件页改动不影响它们 |
## 3.52 1.5.44 金件组行点得中（控件 id 跨帧稳定）（m10689）

前置：装 1.5.44（`Data/SKSE/Plugins/OutfitSystemNG.dll`，FileVersion 1.5.44.0；旧名 `SkyrimOutfitSystemNG.dll` 必须已删），进游戏打开菜单 →「金件」栏。日志默认 info 级别即可，不需要开 Verbose。

- **A1 点得中**：左栏点任一插件组行 —— 该行立即高亮，右侧编辑区切换成该组，日志出现 `Menu: golden group <插件名> selected (1 of N).`。
- **A2 来回切**：依次点第 2、3、1 行，每次右侧编辑区都应跟着换，日志每次一行、括号里的位置跟着变。
- **A3 上一轮的旁证对照**：仍可用「添加某个 ESP 的全部装备」直接选中一个组（代码赋值路径），说明选中态本身没坏 —— 坏的只有点击。
- **B1 不许回退 1.5.43 的布局修复**：组行仍占满可见宽度（点行的右半部分、行尾空白处也要能选中）。
- **B2 两栏不重叠**：把左栏拖宽到接近窗口宽度，再点左栏靠右的行 —— 仍应点得中（右栏不许盖上来）。
- **B3 手动保存仍在**：随便增删一件，数据路径下面出现 `[not saved] The changes live in memory only; press Export to write OutfitSystemGoldenItems.json.`；不点导出直接退游戏再进，那件**不应**保存。
- **C1 light 插件**：给 light(ESL) 插件加一件，右侧显示真名与 `[slot …]`，日志 `Golden items: N entr(ies) resolved, 0 missing.`。
- **C2 组顺序**：左栏按插件名排序；新增一个插件组后行号会重新排，不影响点击。
- **D1 未装 SKSE 菜单框架以外的东西**：无报错、无 `golden items` 相关 critical。
- **E1 性能**：组数很多（>100）时滚动仍顺滑，每帧日志不刷屏（点击才有一行）。
- **F1 回归**：幻化栏列表点选、删除预设（第 2 页及以后）、场景套装切换均与 1.5.43 一致。
- **G1 失败分流**：若组行仍点不动 —— 看有没有 `Menu: golden group … selected (N of M).`：**完全没有** = 点击仍被吃掉（发回菜单窗口大小、分辨率、界面缩放）；**有但右侧不换** = 选中态没生效（发回该行前后 20 行）。

## 3.53 1.5.45 金件按 ESP 添加单件 + 套装按服装名分组（m10869）

前置：装 1.5.45（`Data/SKSE/Plugins/OutfitSystemNG.dll`，FileVersion 1.5.45.0，旧名 `SkyrimOutfitSystemNG.dll` 必须已删）；先导出一次金件做备份（`OutfitSystemGoldenItems.json`）。

1. **A1 页面级单件列表能开**：菜单 →「金件」→ 点 `Add one piece from a plugin... / 按 ESP 添加单件...` → 出现插件列表（按加载顺序倒序、有 `Search pieces...` 之外的插件搜索框）→ 点一个 ESP → 下方展开该 ESP 的装备列表，每行有名称、FormID 与 `[槽位 30/32/…]`。
2. **A2 点一件即加入**：在 A1 的列表里点任意一行 → 该行行首变 `[已加入]`，右侧/左侧对应组出现该件，日志（info）出现 `Menu: golden piece <plugin>:<id> <名称>`。
3. **A3 组内列表同样可用**：选中左侧某个金件组 → 右侧编辑器底部**没有** FormID 输入框，只有该组所属 ESP 的装备列表；点一件应加进**这个**组（不会跑到别的 ESP 的组里）。
4. **B1 不点导出不落盘**：改动后数据路径下显示 `[not saved] The changes live in memory only; press Export to write OutfitSystemGoldenItems.json.`；直接退游戏重进，改动**不在**（这正是期望）。点「导出」后再重进，改动应还在。
5. **B2 搜索过滤**：在装备列表的搜索框输入几个字 → 只剩匹配行；输入乱码 → 显示 `No piece of this plugin matches that search.`，不会崩。
6. **B3 已在组里的件**：对同一件再点一次 → 不应重复加入（行首已 `[已加入]`），JSON 里该 FormID 只出现一次。
7. **C1 单服装 ESP 命名不变**（回归对照）：选一个只有一套衣服的 ESP 生成套装 → 名字应与 1.5.44 完全一致（`<ESP名称>` / `<ESP名称>_1`…），件分布也一致。
8. **C2 多服装 ESP 分组建套装**：选一个含多套服装的 ESP → 左栏出现 `<ESP名称> <服装名>` 若干套；日志 `ESP outfits: … in K name group(s); …` 的 **K = 你数出的套数**；每套只含同一套衣服的件（上衣/下装/鞋/假发同套）。
9. **C3 槽位冲突仍拆分**：同一套衣服里两件同槽位（例如两件上衣）→ 应拆成 `<名称>` 与 `<名称>_1` 两套，其余件在两套里各出现一次（第 2 套用第 1 套的件补齐）。
10. **D1 上限与超限**：一个件数极多的 ESP（>96 套）→ 不崩，消息尾缀出现 `truncated`，已生成的套装可用。
11. **E1 基础幻化不回归**：换普通幻化套装、进/出 OStim 场景（`<当前套装>_sex`）各一次 —— 显示与场景切换仍正常（1.5.25 / 1.5.34 的验收项）。
12. **F1 组行仍点得中**：金件左栏组行仍一点就选中，日志 `Menu: golden group <名字> selected (N of M).`（1.5.44 的行为不能被本版破坏）。

## 4. 性能协议（1000+ 套装）

用 Papyrus 生成 1000 个套装后测：

```papyrus
Function MakeMany()
    Int i = 0
    While i < 1000
        CreateOutfit("Bench_" + i)
        i += 1
    EndWhile
EndFunction
```

指标：打开 Outfits 页 < 100 ms；搜索输入到刷新 < 50 ms；翻页 < 10 ms（当前页只绘制 50 行）。

### 3.21 1.5.8 身体槽位“移除”按钮

A. 行内确认

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 选中一套有多件护甲的套装，滚到右侧 Body slots，点任意一行的“移除” | 该行上方立刻出现“从幻化中移除…？”，同一行出现“移除 / 取消”，不再需要滚回顶部 |
| A2 | 再点“移除” | 该件从列表消失，编辑器状态显示“已移除 …”，日志 info 行 Outfit editor: removed ... from ... |
| A3 | 点“取消” | 不改动套装，两个按钮恢复成一个“移除” |
| A4 | 点开一行的确认后，切到另一套再切回来 | 不显示上一套的确认状态（Invalidate 清空 g_pendingRemoveSlot） |
| A5 | 用超长护甲名的行 | “移除”按钮始终留在子窗口可视区内；横向滚动条仍可用 |

B. 回归

| # | 用例 | 期望 |
| --- | --- | --- |
| B1 | 点“移除”确认后立刻应用该套装 | 幻化外观与套装数据一致（删除后重刷） |
| B2 | 在确认状态下关闭再打开菜单 | 无残留确认、无崩溃 |
| B3 | 中文界面 | 提示与两个按钮均显示中文（Remove ... from the outfit? / 移除 / 取消） |

### 3.22 1.5.9 OBody 换身形后的多件刷新

**A. 多件与真实穿戴**

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 套装 = 带 .tri 的上衣（32 槽，带身体参照）+ 幻化裤子；上衣在背包里真实穿戴，裤子只由幻化提供；在 OBody 换身体预设 | 上衣和裤子都按新身形刷新，不再出现裤子保持旧模型、与身体穿模 |
| A2 | 套装全部为幻化件（都不在背包里穿戴），换预设 | 所有带 .tri 的显示件都重建，不只是一件 |
| A3 | 套装同时含真实穿戴的裤子与只幻化的上衣，换预设 | 两类件都刷新；PAD 开启时真实护甲仍能被 PAD 识别（不因强制重建丢失诚实还原） |
| A4 | 换预设后切第一人称再切回第三人称 | 两套 biped 都使用新形变，不出现一半旧模型 |
| A5 | 只幻化一件带 .tri 的上衣，换预设 | 正常刷新，日志件数与掩码只含该件槽位 |

**B. 日志与回归**

| # | 用例 | 期望 |
| --- | --- | --- |
| B1 | Settings 勾选 Verbose logging 后换预设 | 出现 `Custom: force-rebuilding N display pieces (mask 0x........) for ........ after a body morph change.`；N 与显示集件数一致 |
| B2 | 默认日志级别换预设 | 不刷屏；`.tri` 刷新照常工作 |
| B3 | 不装 RaceMenu 或 BodyMorph < 4，但装 OBody 4.4.0+ | 走 OBody 后端；有预设变化时出现强制重建日志，换装/幻化/取消幻化不受影响、不崩溃 |
| B4 | 普通换装（不换身形） | 仍只处理 applySet，不出现重复贴件或外观闪烁 |
| B5 | 换预设后打开套装编辑器改槽位策略再应用 | 新策略生效，外观与策略一致 |

### 3.23 1.5.10 OBody-only 预设刷新

前置：只装 OBody 4.4.0+，不装 RaceMenu（或确认 skee 不可用）；OBody 已给玩家/目标 actor 分配预设。

**A. 基础刷新**

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 幻化一套带 .tri 的服装，在 OBody 换另一个身体预设 | 约 1 秒内整套幻化件按新身形刷新，无需取消/重穿 |
| A2 | 同一套装里既有真实穿戴件又有纯幻化件，换预设 | 两类件都重建，不再出现只刷新 32 槽上衣、裤子穿模 |
| A3 | 换预设后保持静止（不按键）10 秒，再切第一人称/第三人称 | 两套 biped 都使用新形变；若未刷新，记录是否有输入心跳（已知轮询挂在输入心跳上） |
| A4 | 不装 OBody、只装 RaceMenu BodyMorph v4+ | 仍走 RaceMenu 后端，行为与 1.5.9 相同 |
| A5 | OBody 与 RaceMenu 同时安装 | 不崩溃、不重复重建；日志同时有 RaceMenu 与 OBody 获取行，实际以 RaceMenu BodyMorph 为权威 |

**B. 日志与边角**

| # | 用例 | 期望 |
| --- | --- | --- |
| B1 | 启动后看日志 | 有 MorphRefresh: OBody plugin API v1 acquired 与 MorphRefresh: OBody is ready；不再有 RaceMenu is not installed 作为唯一后端信息 |
| B2 | 默认日志换预设 | 不刷屏；开 Verbose logging 后出现 MorphRefresh: the body morphs of <FormID> changed; rebuilding the illusion. 与 Custom: force-rebuilding ... |
| B3 | OBody 版本低于 4.4.0 或未安装 | 只记 debug OBody plugin API is unavailable；换装/幻化/取消幻化不受影响、不崩溃 |
| B4 | 连续快速换 5 个预设 | 不崩溃、不卡死；最终外观对应最后一个预设 |
| B5 | 换预设后改槽位策略/换套装再换回 | 外观与策略一致；无残留旧 partClone |

### 3.24 1.5.11 OBody 换预设后身体/衣服整体消失

前置：带 `.tri` 的幻化套装；OBody 4.4.0+；至少一件真实穿戴件与一件纯幻化件。

**A. 外观**

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 幻化后 OBody 换预设 | 身体与全部幻化件都按新身形显示，不再出现身体/衣服消失、只剩头手 |
| A2 | 换预设后保持静止约 1 秒，看第三人称 | 第三人称身体与衣服完整，不出现空白模型 |
| A3 | 再切第一人称、切回第三人称 | 两套 biped 都完整，不互相清空 |
| A4 | 换预设后按 Z 重新点当前套装 | 作为对照，外观应与 A1 一致（不应成为必须操作） |
| A5 | 同时有真实穿戴件与纯幻化件 | 两类件都显示；PAD 开启时真实护甲仍能被识别 |

**B. 日志与回归**

| # | 用例 | 期望 |
| --- | --- | --- |
| B1 | Verbose logging 换预设 | 出现两条 `Custom: force-rebuilding N display pieces ...`（第一/第三人称各一条），之后外观完整 |
| B2 | 默认日志换预设 | 不刷屏，外观照常刷新 |
| B3 | 换预设后取消幻化、再重新应用 | 与 1.5.10 一致，不残留空白模型 |
| B4 | 不换预设，普通换装 | 不触发 `force-rebuilding`，外观正常 |

### 3.25 1.5.12 OBody 换预设后其余幻化件跟随

前置：装 OBody NG（Nexus 77016）4.4.0+，一套带 `.tri` 的服装（上衣 + 裤子 + 鞋子/头发/发饰），其中至少一件是真实穿戴、至少一件是纯幻化；Settings 勾选 Verbose logging。

**A. 重建**

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 幻化整套后，在 OBody 换身体预设 | 约 1 秒内**全部**幻化件（含裤子、鞋子、头发、发饰）一起按新身形刷新，不再只有上衣变化 |
| A2 | 保持静止 2 秒，看第三人称 | 不出现穿模；不出现模型消失 |
| A3 | 切第一人称再切回 | 两套 biped 都按新身形显示 |
| A4 | 连续换 3 个不同预设 | 每次都完整跟随；不残留上一个预设的部件 |
| A5 | 换预设后按 Z 重新点当前套装 | 结果应与 A1 一致（不应成为必须操作） |
| A6 | 关掉 Verbose logging 再换预设 | 外观照常刷新，日志不刷屏 |

**B. 日志与后端**

| # | 用例 | 期望 |
| --- | --- | --- |
| B1 | 换预设（Verbose） | 出现 `MorphRefresh: rebuilding the illusion of ... after the body morph provider settled.`；其前后各有一次 `Custom: force-rebuilding ...` |
| B2 | 只有 OBody（无 RaceMenu） | 不应出现 `MorphRefresh: no body morph provider is available`；应有 `OBody plugin API v1 acquired` / `OBody is ready` |
| B3 | RaceMenu 在 kDataLoaded 未应答 | 日志先是 `RaceMenu did not answer the interface exchange`（info）；随后约 2 秒内最多重试 30 次，若接上会出现 `RaceMenu BodyMorph vN acquired` |
| B4 | 换预设后取消幻化 / 重进游戏 | 与 1.5.11 一致，不残留空白模型 |

### 3.26 1.5.13 手动重穿与 OBody 跟随

**A. 手动重穿刷新**

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 幻化一套带 `.tri` 的服装，OBody 换一个明显不同的预设后按 Z 重穿当前套装 | 幻化件立刻按新身形重建，不再需要「取消幻化再切回」 |
| A2 | 同一动作改为快选窗里选中当前套装按 E | 与 A1 相同 |
| A3 | 编辑当前套装（增删一件）后关闭菜单 | 幻化立刻反映编辑结果（revision 触发全量重建） |
| A4 | Verbose 日志 | 出现 `Custom: rebuilding ... outfit revision true`，随后每件护甲一行 `Custom: <Armor> slot .. clone 0x...` |

**B. OBody 换预设后全部件跟随**

| # | 用例 | 期望 |
| --- | --- | --- |
| B1 | OBody 换预设，观察 2 秒内 | 上衣、裤子、鞋子、头发、发饰一起跟随新身形，无穿模 |
| B2 | 同一次换预设的日志 | 400 ms 与 1400 ms 各有一轮 `Custom: rebuilding ...`，第二轮附近有 `MorphRefresh: final rebuild of the illusion of ...` |
| B3 | 切第一人称再切回第三人称 | 两套 biped 都完整，不出现只剩头手有模型 |
| B4 | 连续快速换 3 个预设 | 最终外观与最后一个预设一致，不残留中间态 |

**C. clone 指针诊断与降噪**

| # | 用例 | 期望 |
| --- | --- | --- |
| C1 | 对比同一次换预设两轮重建的同一槽位 clone 指针 | 应发生变化；若完全不变，请回传这段日志（说明 `ApplyArmorAddon` 复用了旧几何） |
| C2 | 未接上 RaceMenu 时的日志量 | `Failed to dispatch message to skee` 约 12 条（5 秒一次），不再每 2 秒刷屏 |

### 3.27 1.5.14 幻化循环（换身形不再穿模）

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 幻化带 `.tri` 的一套，OBody 换预设，不做任何操作 | 2 秒内全部部件（上衣/裤子/鞋子/头发/发饰）一起跟随新身形，不穿模 |
| A2 | 同上，看日志 | 出现 `taking the illusion of ... off for one pass (body morph changed)`，约 0.25 s 后 `illusion of ... restored with a fresh morph target` |
| A3 | 连续快速换 3 个预设 | 最终外观与最后一个预设一致；循环不会无限重排（每角色同时只有一次） |
| B1 | 按 Z 打开快选窗、点当前正在穿的那一套 | 立刻刷新（日志 `... (the outfit was re-applied)`），不再需要先取消幻化 |
| B2 | 按 Z 后再看日志里的 Custom 行 | 出现 `outfit revision true`（1.5.13 里恒为 false） |
| B3 | 编辑当前套装增删一件 | 幻化立刻反映编辑结果 |
| C1 | 换第一/第三人称、开关菜单 | 不出现只剩头手、也不出现幻化件丢失 |
| C2 | 幻化带披风/长裙等大体量件换预设 | 同样跟随，不残留旧形变 |

### 3.31 1.5.18 身形变化三条症状（RaceMenu 滑块 / OBody 变卡 / SLIF 膨胀）

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 幻化一套**空套装**（裸体外观），拖 RaceMenu Body Morph 滑块 | 外观跟随（1.5.18 之前这个场景本来就看不出问题，因为它直接显示真实裸体） |
| A2 | 幻化一套带 `.tri` 的衣服，再拖同一个滑块 | 幻化件一起变形；开 Verbose logging 能看到 `taking the illusion of ... off for one pass (body morph changed)` |
| A3 | 日志确认三路 provider | 即使只有 `MorphRefresh: no body morph provider answered; morph changes follow the geometry probe only.`，A1/A2 也必须生效（几何签名兜底） |
| B1 | OBody NG 连续换 3 个身形预设，数日志 | 每个预设 2 条 `taking the illusion` + 1 条 `rebuilding ... after the body morph provider settled`（共 3 次刷新；1.5.17 是 4 次） |
| B2 | 换完预设后静置 5 秒 | 不再出现自发的 `taking the illusion`（2.5 秒 quiet 窗口生效）；成串出现说明几何签名不稳定或被别的插件持续写顶点 |
| B3 | 连续快速换 5 个预设 | 画面明显比 1.5.17 轻，最终外观等于最后一个预设，不残留中间态 |
| C1 | SLIF NG / Fill Her Up 把肚子膨胀到一半停住 | 幻化件跟随膨胀（`morph:PregnancyBelly` 一类滑块驱动） |
| C2 | 膨胀 ramp 全过程 | 一轮只重建一次（350 ms 去抖 + 1200 ms 最小间隔），画面不被刷爆；ramp 落定后最多再补一次 700 ms 重建 |
| C3 | 没装 SLIF NG | 日志出现 `SLIF NG is not installed ... inflation follows the geometry probe only`（debug），身形变化仍由几何签名跟随 |
| D1 | 快选窗/菜单里来回切第一与第三人称 | 两套 `GetBiped()` 都算进签名，单纯切视角不应触发幻化循环 |
| D2 | 幻化件里有带物理（SMP/HDT）的披风或长裙 | 循环期间不闪出原版装备、不出现只剩头手；物理件重建后不塌陷 |

### 3.32 1.5.19 三条症状复测（skee 握手 / 几何探针 / provider-first）

先确认跑的是 1.5.19：日志加载期应出现 `Skyrim Outfit System NG 1.5.19` 一类版本行，且**不应**再出现 `rebuilding the illusion of ... after the body morph provider settled`。

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 装包后**在主菜单停 2 分钟**再进游戏（复现 m21255 的时间线：23:57 加载、00:00:16 进游戏） | 主菜单期间只看到一次 `MorphRefresh: RaceMenu did not answer the interface exchange`；进游戏后若 RaceMenu 应答，出现 `MorphRefresh: RaceMenu BodyMorph v4 acquired`（1.5.18 在这里就永远不会再试了） |
| A2 | 装了 RaceMenu 但握手始终不成 | 进游戏约 5 分钟后出现 `MorphRefresh: skee64.dll is in the process but RaceMenu still has not handed out the BodyMorph interface; kept asking (N tries).`；N 会随时间增长（说明还在试） |
| A3 | 没装 RaceMenu | 出现 `MorphRefresh: no skee module in the process - RaceMenu is not installed.`（区分「没装」与「装了不答」） |
| B1 | 幻化一套带 `.tri` 的衣服，拖 RaceMenu Body Morph 滑块 | 幻化件实时跟随；Verbose 下看到 `the body morphs of <FormID> changed; following them.` + `the provider pass moved the meshes of <FormID>; the illusion followed without a cycle.` |
| B2 | 同 B1，但日志出现 `the provider pass left the meshes of <FormID> alone` | 说明 RaceMenu 没有重建我们的克隆 → 自动回退 `taking the illusion of <FormID> off for one pass`；外观仍应跟随（只是回到 1.5.18 的观感） |
| B3 | 空套装（裸体外观）拖滑块 | 与 1.5.18 相同（引擎自己刷真实裸体）；本条只用于对照，不是回归判据 |
| C1 | OBody NG 连续换 3 个身形预设，数日志 | 每个预设**不应**出现 `taking the illusion ... off`（除非回退），观感应明显轻于 1.5.18；出现 `ApplyOBodyMorphsToActor`（trace）说明走的是 provider-first |
| C2 | 换完预设静置 5 秒 | 不应再出现任何 `taking the illusion` 与 `rebuilding the illusion`（1.5.19 已删除 700 ms 补建） |
| C3 | 把 `[Compatibility] MorphProviderFirst` 改成 0 重进 | 恢复 1.5.18 的「每次都走幻化循环」；用于确认问题是否由 provider-first 引入 |
| D1 | SLIF NG / Fill Her Up 膨胀到一半停住 | 幻化件跟随；日志应只有 `changed; following them.`（+ 可能的回退行） |
| D2 | 连续膨胀 ramp | 一轮只触发一次 provider-first（350 ms × 最多 4 次校验），ramp 落定前不刷爆主线程（2500 ms quiet 窗口） |

### 3.33 1.5.20 幻化件跟随身形（`.tri` 目标 + morph 落地即重放）

先确认跑的是 1.5.20：日志版本行应为 `1.5.20`，且能看到 `MorphRefresh: .tri morph target support active|inactive ... for illusion slots.`（1.5.19 及以前没有这一行）。

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 装包后看握手通道 | `MorphRefresh: RaceMenu BodyMorph v5 acquired through skee64`（通道名可能是 `skee64` / `skee` / `(broadcast)`，只要不是反复失败即可）。若显示 `v4` 且紧跟 `has no MorphShapeCallback (v5 added it)`，说明 skee 老于 v5，退回轮询 —— 记录版本号反馈 |
| A2 | 幻化一套带 `.tri` 的紧身衣，静置 3 秒 | 日志（debug 以上）应出现一次 `re-morphed <FormID> on N node(s), M new .tri target(s) (...); the vertices were already current.`；`M > 0` 说明 `.tri` 目标挂上了。若 `M = 0` 且该件确实有 `.tri`，把护甲 EditorID 与 `meshes...` 路径发来 |
| B1 | 开 RaceMenu，在 Body Morphs 页签拖体重滑块（从 0 拖到 1，慢拖 5 秒） | 幻化件轮廓**连续**跟着变（不是松手后才跳变）；拖动期间**不应**出现 `taking the illusion ... off`；日志出现若干 `re-morphed ...; the vertices moved.`（每角色 5 秒最多一条，属正常限流） |
| B2 | 同 B1，但把 `[Compatibility] MorphProviderFirst` 设 0 | provider-first 关掉后本修复仍然生效（推送路径与该开关无关）；用于区分两条链路 |
| B3 | 空套装（裸体外观）拖滑块 | 与以前一致，仅作对照 |
| C1 | OBody NG 连续换 3 个身形预设（间隔 2 秒） | 每次换完 ≤ 1 帧内幻化件身形变化；`Obody_ApplyMorph` 事件驱动，日志每个预设一条 `re-morphed ...; the vertices moved.`；**不得**闪原版护甲 |
| C2 | 换完预设静置 5 秒 | 不应出现 `taking the illusion ... off` / `rebuilding the illusion`（AbsorbOwnWrite 的 1.2 s 静默窗应吸收掉自家写入） |
| D1 | SLIF NG 膨胀到一半停住 | 幻化件跟随；SLIF 无事件，走的是原有 250 ms 签名轮询（`changed; following them.`），属预期 |
| E1 | 幻化一件**没有** `.tri` 的护甲，拖滑块 | 该件不动（与不装幻化时一致），日志 `0 new .tri target(s)`；不得报错、不得卡顿 |
| E2 | 幻化多件混合（部分有 `.tri`、部分没有）+ 换预设 | 有 `.tri` 的跟随、没有的不变；不出现整槽丢失或露原版 |
| F1 | 反复进出 RaceMenu 5 次 | 回调只注册一次（日志只有一条 `hands out MorphShapeCallback`），无崩溃、无内存增长异常 |

### 3.34 1.5.21 幻化不再全裸（自定位 + fail-closed 降级）

> 本节是 **1.5.21 专有**用例，留作历史对照：1.5.22 起 `SiteOffset` 与 ±0x800 自定位已删除，`moved by N bytes` / `refusing to guess` / `candidate at rva` 这些日志不会再出现，日常回归请用 §3.35。1.5.21 的 fail-closed 行为（幻化关闭但显示真实护甲）仍是当前版本的兜底语义，因此 B2/C1/C2 仍有参考价值。

先确认跑的是 1.5.21：日志版本行 `1.5.21`，并应出现 `Skinning hooks: SkyrimSE.exe ...`。1.5.20 的症状是「换任何套装都只看到裸体」，1.5.21 的判定核心是：**要么幻化正常显示，要么显示真实护甲，永远不再裸体**。

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 装包启动，看日志头部 | 出现 `Skinning hooks: SkyrimSE.exe 1-5-97-0, N bytes, written YYYY-MM-DD HH:MM:SS, base 0x..., .text rva 0x... + 0x...`，随后五条 `hook installed`；末尾 `Illusion display is active: the player skinning hooks are installed.` |
| A2 | 切任意一套幻化 | 幻化外观正常显示（**不是裸体**）；换套、取消幻化、再切回都正常 |
| B1 | 本机正是 1.5.20 出问题的那份 exe（site rva 0x364301 对不上） | 日志出现 `CustomSkinPlayer: the call to 0x... sits at rva 0x... instead of 0x... (moved by N bytes); hooking the relocated site.`（或 `.text holds exactly one call ... hooking that one.`），且幻化正常 |
| B2 | 同 B1，拖动身形滑块 | 1.5.20 的 `.tri` 跟随仍然工作（§3.33 的 B1/C1 通过），自定位不影响 morph 链路 |
| C1 | 故意制造失败（例如把 `SkyrimSE.exe` 换成其它小版本，或（1.5.22 起）临时把某个钩子的宿主函数 ID 改错后编译） | 日志出现 `critical` 的 `Illusion display is off for this session: ... (vanilla pass true, display pass false). The real equipped armour stays visible.`；角色**穿着真实护甲**、不裸体；菜单/换装/存档/其它功能正常 |
| C2 | 同 C1，检查是否有二次伤害 | 不应出现崩溃、闪退、卡死；`ShouldOverrideSkinning` 不应高频刷日志（失败时它恒 false，日志最多一次装钩子汇报） |
| D1 | 多候选场景（若日志出现 `N calls to 0x... sit inside the +/-0x800 window; refusing to guess`） | 每个候选行带 `candidate at rva 0x... (N bytes from the nominal site)`；不 patch、不崩；幻化本局关闭（真实护甲可见）；把候选行发回即可补偏移 |
| D2 | 同 D1，确认全 `.text` 统计行 | 有 `... .text holds N call(s) to 0x....` 与最多 5 条 `nearest call at rva 0x... (N bytes from the nominal site)` |
| E1 | 长时间游玩（≥ 30 分钟），含进出室内外、开 RaceMenu、换预设 | `ShouldOverrideSkinning: 0x... is not a readable object` 若出现应为偶发（≤ 个位数）；持续刷说明钩错位置，需反馈 |
| E2 | 检查 `Illusion display` 状态是否被误关 | 一次会话内状态固定（装钩子时定），不应中途翻转；日志里 `Illusion display is active` 只出现一次 |

### 3.35 1.5.22 幻化恢复 + 钩子站点自解析（地址库定函数 / 函数体内扫 `E8`）

先确认跑的是 1.5.22（日志版本行 `1.5.22`，DLL 1,859,584 字节）。本版不再有任何字节偏移：站点由「宿主函数入口 + 扫描 `E8`」决定，因此判定核心是**每条钩子都能自报站点**，且 `CustomSkinPlayer` 即使钩不到也不影响幻化。

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 装包启动，开 Verbose logging，看日志头部 | 每个钩子一行 `<Hook>: the call to 0x... sits at rva 0x... (N byte(s) into the function at rva 0x...).`；随后 `hook installed at rva 0x...`；末尾 `Illusion display is active: ...`（两条 info 任一） |
| A2 | 对照 §2.3 的旧偏移 | `DontVanillaSkinPlayer` 的 N ≈ `0x302`、`ShimWornFlags` ≈ `0x7C`（AE `0x80`）、`FixEquipConflictCheck` ≈ `0x97`、`FixSkillLeveling` ≈ `0x52`（AE `0x373`）；`CustomSkinPlayer` 在 1.5.97 上可能完全不同（函数体只约 `0xD0`，可能 0 候选） |
| B1 | 切任意一套幻化 | 幻化正常显示（不是裸体）；换套、取消、再切回都正常。**这是 m01170 的验收点** |
| B2 | 若日志是 `Illusion display is active: the hiding hook is installed and the display set is attached from the deferred pass.` | 幻化仍应正常显示（延迟附加生效）；首次延迟贴件时有一条 `Custom: the skinning routine of this game build has no call site to hook; attaching the display set from the deferred pass.`（debug） |
| B3 | 同 B2，观察延迟 | 切套装后幻化应在**一帧内**出现（皮肤更新会立刻触发 `MarkAttachPending`）；同一 actor 连续切换时最多每 100 ms 一次附加，不应卡顿或刷屏 |
| C1 | 若日志是 `...: the function at rva 0x... does not call 0x... within 0x800 bytes; the hook stays disabled ...` | 后面应有 `the function at rva 0x...` + `.text holds N call(s) to 0x...` + 最多 5 条 `call at rva 0x... (N bytes from the function entry)`；不 patch、不崩；`CustomSkinPlayer` 缺这条不影响幻化，其它钩子缺则对应功能降级 |
| C2 | 若日志出现 `... calls 0x... N times; hooking the first one at rva 0x....` | 幻化正常；若某些路径（第一人称 / 狼人 / 吸血鬼）不刷新，把这行发回 |
| D1 | 第一/第三人称各看一次（`Custom` 走延迟路径时对 `{false,true}` 各贴一次） | 两个视角都有幻化，不应出现「第三人称有、第一人称裸」或反之 |
| D2 | 打开 RaceMenu 拖 Body Morphs 滑块（`.tri` 跟随回归） | 1.5.20 的行为不受影响：幻化件随滑块实时变形、不闪原版、日志仍有 `re-morphed ...`（§3.33 B1） |
| E1 | 检查是否还有旧版本字符串 | 1.5.22 的 DLL 里**不应**再出现 `moved by N bytes; hooking the relocated site` / `refusing to guess`（那是 1.5.21 的路径）；出现说明装的还是旧 DLL |
| E2 | 连续游玩 ≥ 30 分钟，含频繁换装、开菜单、进出室内外 | 无崩溃/卡死；`ShouldOverrideSkinning: 0x... is not a readable object` 若出现应极偶发；延迟队列不应堆积（日志无重复 `attaching the display set from the deferred pass` 刷屏） |

### 3.36 1.5.23 恢复 1.5.17 的贴件站点（历史偏移回退 + 成对门禁）

先确认跑的是 1.5.23（日志版本行 `1.5.23`，DLL 1,863,168 字节，sha256 `be282e7a…`）。本版只改两件事：`CustomSkinPlayer` 扫不到调用点时回退到 1.0.0 起的历史站点；幻化总开关重新要求两个皮肤钩子都在。判定核心是**幻化恢复显示**且**日志能说清站点是扫描命中还是历史回退**。

| # | 用例 | 期望 |
| --- | --- | --- |
| A1 | 装包启动，开 Verbose logging，看日志头部 | 四条钩子各一行 `<Hook>: the call to 0x... sits at rva 0x... (N byte(s) into the function at rva 0x...).`；`CustomSkinPlayer` 若是历史回退，则是一条 warn `CustomSkinPlayer: the function at rva 0x364280 has no call to 0x... within 0x800 bytes, but the site this hook has used since 1.0.0 - rva 0x364301 (+0x81 into the function) - is still a 5-byte call; the hook is installed there.` |
| A2 | 看钩子安装末尾 | `Illusion display is active: the player skinning hooks are installed.`（1.5.23 起这是唯一的 active 文案；1.5.22 的 `... attached from the deferred pass` 已被删除） |
| B1 | 切任意一套幻化 | 幻化正常显示（**不是裸体**）。这是 m01836 / m01837 的验收点：应与 1.5.17 的表现一致 |
| B2 | 换一套、取消幻化、再切回；换同槽真实护甲 | 外观都正确；真实护甲仍被藏、幻化件仍在；取消后立刻回到真实装备 |
| B3 | 若日志是 `Illusion display is off for this session: the player skinning hooks are not both installed (vanilla pass hooked\|missing, display pass hooked\|missing). The real equipped armour stays visible. Nothing else is disabled.` | 角色应穿着**真实护甲**（不是裸体），其它功能（菜单、Papyrus、PAD/AND）正常；把上面 `CustomSkinPlayer` 的失败行一起发回 |
| C1 | 若日志是 `CustomSkinPlayer: ... and the legacy site rva 0x... is not a 5-byte call either; the hook stays disabled ...` | 后面应有 `the function at rva 0x...` + `.text holds N call(s) to 0x...` + 最多 5 条 `call at rva 0x...`；不 patch、不崩；幻化本局关闭但显示真实护甲 |
| C2 | 若出现 `... calls 0x... N times; hooking the first one at rva 0x....`（四个纯扫描钩子） | 幻化正常；若某些路径（第一人称 / 狼人 / 吸血鬼）不刷新，把这行发回 |
| D1 | 第一/第三人称各看一次 | 两个视角都有幻化，不应出现「第三人称有、第一人称裸」或反之 |
| D2 | RaceMenu 拖 Body Morphs 滑块 / OBody 换预设 / SLIF 膨胀（`.tri` 跟随回归） | 1.5.20 的行为不受影响：幻化件随形变实时变化、不闪原版、日志仍有 `re-morphed ...`（§3.33 B1） |
| E1 | 反例检查 | 1.5.23 的 DLL 里**不应**再出现 `moved by N bytes; hooking the relocated site`（1.5.21）与作为 active 判定的 `the deferred pass`（1.5.22）；出现说明装的还是旧 DLL。应出现 `the site this hook has used since 1.0.0` / `legacy site rva` |
| E2 | 连续游玩 ≥ 30 分钟，含频繁换装、开菜单、进出室内外 | 无崩溃/卡死；无 `is not a readable object` 刷屏；`Custom` 相关 trace 不无限增长 |

## 5. 回归清单（每次改钩子后）


- 五条 `hook installed` 都在；
- 戴帽子不秃头（`ShimWornFlags` 并集生效）；
- 装备第二件同槽位护甲不会把第一件卸掉（`FixEquipConflictCheck`）；
- 穿重甲练重甲技能（`FixSkillLeveling`）；
- 数值面板与真实护甲一致；
- 存档往返一致。

## 6. 已知未验证项
- **1.5.45 的两处改动未实机验证**：本机没有游戏。金件两级列表依赖 `PluginScan` 既有索引与 ImGui 列表语义（行号 id、`PushID(scopeId)` 分栈）；「按名字最长共享前缀分组」是从用户描述的启发式，未在真实 ESP 上跑过，两种偏差（两套前缀相同被并组 / 某件第一个词不同掉进「其余」组）只能靠 `K name group(s)` 与自数套数比对发现。
- **1.5.44 的金件组行修复未实机验证**（m10689）：本机无游戏。结论来自 ImGui 点击语义（`ActiveId` 必须与松开时的控件 id 一致）+ 同一函数里「指针 id 坏、整数 id 好」的对照事实，不是实机复现。唯一的判据是 `Menu: golden group <名字> selected (N of M).` 这行 info 日志的有无。

- **1.5.43 的三处修复全部未实机验证**（m10387）：本机无游戏。①「点不中」是按 ImGui 语义推断 —— `Selectable(label, selected)` 的 `(0,0)` 尺寸 + 右栏 `ImVec2(0, …)` 在剩余宽度为负时覆盖左栏，对照是 `MenuUI.cpp:737-738` 那处显式给 `ImVec2(rowWidth, 0)` 才点得中的写法；②「手动导出」是本地可静态确认的（`RenderGolden()` 内只剩一处 `SaveToDisk()`），但「不点导出重启后真的没保存」要在游戏里看；③light 插件 id 宽度是从日志里 `002E280C` / `002E1D66` / `002E0823` 的高 12 位连续递增加上 CL-NG `GetFormID` 不掩码的事实推出，`ResolveForm()` 还留了 24 位兜底，所以最坏情况是「仍旧 missing 但不崩」。
- **1.5.42 的删除崩溃修复未实机验证**（m10183）：本机无游戏。崩溃点是用自写 PE 解析把栈帧对位到 `MenuUI.cpp` 的 `RenderOutfits` 并复核 `begin` / `end` 的 `size_t` 下溢得出的，不是实机复现。判据是「第 2 页及以后删除不崩 + 删除行之后仍继续渲染」；若仍崩，先看崩溃栈里是否还是 `vector too long`。
- **1.5.41 的启动崩溃修复未实机验证**（m10014）：本机无游戏，判据是崩溃栈字符串 `resource deadlock would occur` + 自写 PE 解析的帧对位（`MessageHandler` → `Golden::LoadFromDisk` → `Golden::ImportText` → 计数 getter）+ 源码核对；不是实机复现。若实机仍闪退，按 §3.49 的 A1/A3 分流。
- **1.5.40 的两处修复未实机验证**（m09740）：ESP 列表是否非空、金件页是否还崩、`PluginScan` 四类跳过计数在你环境（890 插件 / 13,909 条槽位护甲）上的分布，全部没在游戏里看过。崩溃点由自写 PE 解析对位到 `RenderGolden` 内的 `snprintf`（读 `0x8`），非实机复现。
- **1.5.40 的服装判据仍可能漏件**：模型判据现在看男女两人的三/一人称 `bipedModels[sex]` / `bipedModel1stPersons[sex]`，但模型靠运行时附加数据、不用 `TESModel` 的护甲仍会被判「without a model」；是否放宽等实机 `PluginScan:` 统计。
- 1.5.39 的三项菜单功能全部未实机验证：逐槽策略按套装独立（`g_slotPolicyOpen`）、「从 ESP 生成套装」（`PluginScan` + `EspOutfits`）、「金件」页（`GoldenItems` + `OutfitSystemGoldenItems.json`）。静态侧只有 `BUILD_EXIT=0`（0 error / 0 warning）与 `tests/esp_outfits_filter.ps1` 等 7 项守卫测试通过；菜单交互、ESP 扫描结果、生成出的套装外观都没人看过。
- 1.5.39 的服装过滤（`IsOutfitArmor()` 三条判据）是从引擎数据结构推出来的：至少一个 armor addon 在 `bipedModels[SEXES::kMale]` 或 `bipedModel1stPersons[SEXES::kMale]` 上给了模型才收。只挂女性模型的 addon 通常在男性槽也会给模型（否则男性穿模），但没有实机样本证明「所有正常服装都能过」；若用户发现某件正常衣服被跳过，把那件记录的 EditorID / 槽位掩码 / addon 模型发回，按样本放宽判据，**不要**直接把 `IsOutfitArmor()` 改成恒真（那样 FX 特效会重新混进套装）。
- 1.5.39 的金件不过滤是刻意行为（m09426）：FX 件、无 EditorID 件都能加进金件列表。风险是用户可能加进「其实没有任何模型」的记录，那些件在贴件时不会产生可见几何 —— 属于用户输入错误，不做额外拦截（`AddLocal()` 仍要求记录存在且有槽位掩码）。
- 1.5.32 的「只切套装」未实机验证：本版删掉补跑与重建后，场景切换就是一次 `Features::AssignOutfit()`（一次 `refreshArmorFor`）。1.5.31 的日志证明「切换其实执行了、biped 里也有几何」，所以本版不再假设「模型没上屏」是缺少刷新次数；若实机后场景里身体/手/脚槽仍为空，问题在「引擎 Skin 覆盖槽（3P slot 2/4/7）与幻化克隆争同一槽」，需要按 `HairTintDiagnostics` 的 3P/1P 对比单独定位，**不要**再加回补跑或重建。
- 1.5.33 的场景套装换装未实机验证：场景开始/结束现在与「按地点/状态切换」完全同两步（`OutfitSystem::setOutfitByName()` + `OutfitSystem::refreshArmorFor()`），事件在 `ModCallbackEvent` sink 里同步执行。1.5.29–1.5.32 的日志已证明装备 pass 在场景开始时确实执行过（`Armor Refresh for …` + `Custom: attaching 5 outfit-only piece(s) …`），因此若本版模型仍不出现，需按新日志另找方向，不得再加「多刷新几次」类补丁。
- 1.5.34 的「离线程场景事件用一次 `AddTask` 交接」未实机验证：本版只保证事件不再被 `OnGameThread()` 丢掉、开关会在游戏线程执行；`AddTask` 交接会晚一个任务周期（感知不到）。若日志出现 `scene started; 'A' -> 'A_sex'.` 与随后的 `attaching …` 而模型仍不出现，那属于「切了但被绘制的 3D 不更新」的另一层（1.5.30/1.5.31 的方向已被实机否掉），需按新日志再定位。
- （历史）1.5.31 的「模型重建让幻化上屏」未实机验证：`Actor::Update3DModel()` 是否真会让引擎重走 biped→3D 并采用刚写入的克隆，依据是「开合菜单时引擎做的正是这件事」这一观察；若某 load order 里重建后仍不显示，说明还有一层缓存，需要按新日志再定位。`Model Rebuild for` 与 `Skipping model rebuild` 两条都只在 debug 级可见。

- 1.5.29 的「残留克隆窄清 + 幻化几何对账」未实机验证：拆克隆依赖 `ownedMask`（只有"贴进去时没有真实穿戴物打底"的槽才计入），若某套幻化件与真实穿戴件同槽，退出时不会拆、表现为残留旧件；对账依赖 `objects[bit].partClone` 非空判断"幻化在不在"，若引擎保留 `partClone` 但几何失效则不触发。两者都只做静态推理。
- 全部游戏内行为（无游戏环境）；
- 1.5.20 的 `.tri` 目标挂载与 `ApplyVertexDiff(…, false)` 重放全部未实机验证；特别是「skee 自己写顶点」与「我们再重放一遍」在同一帧内叠加是否会双写（理论上 `erase=false` 先用 `SHAPEDATA` 还原再重放，幂等），需按 §3.33 的 B1/C1 观察；
- SKSE Menu Framework 3.x 的 cimgui 符号在真实运行环境下的绑定（只做了符号存在性核对）；
- 1.6.1170 实机的钩子站点字节未验证：ID 来自 Address Library；1.5.22 起站点由宿主函数体内扫 `E8` 决定，代码里不再保存任何字节偏移（`0x302`/`0x7C`/`0x81`/`0x97`/`0x52` 只是历史对照值，仅用于核对日志里的 `sits at rva`）。
- **PAD / AND 兼容的实际效果**（本机没有这两个 mod 的运行环境）：阵营等级写入、心跳补写、PAD 撕件与被撕件 id 的对应关系都只做了静态推理；
- 引擎内对话条件（TESCondition 的 `WornHasKeyword` 一类）不受幻化影响，本版未做也无法安全介入；
- VR 未构建；
- 1.5.7 的 RaceMenu BodyMorph ABI 为手写最小声明，`.tri` 实时刷新与快选窗 X 键待实机验证。
- 1.5.9 的 OBody 全 displaySet 重建待实机验证；1.5.10 起不再要求 RaceMenu，改为 OBody 4.4.0+ 插件 API。
- 1.5.10 的 OBody 预设轮询与 ApplyOBodyMorphsToActor 时序待实机验证；OBody API 为手写最小 ABI 声明。
- 1.5.11 的 OBody 换预设后身体/衣服消失修复待实机验证（重点：第一/第三人称两套 biped 是否都完整）。
- 1.5.12 的 500 ms 延迟二次重建与 RaceMenu 接口重试待实机验证；OBody 侧 `ApplyOBodyMorphsToActor` 仍为手写 ABI 声明。
- 1.5.13 的 revision 触发全量重建、400/1400 ms 两段重建与 clone 指针诊断待实机验证。
- 1.5.19 的 skee 握手「永不放弃」、几何探针递归与 provider-first 待实机验证：RaceMenu 是否真的会在进游戏后应答、`ApplyBodyMorphs(deferUpdate=true)` 的落地是否落在 4 × 350 ms 内、provider 是否会重建我们的克隆（不重建则回退循环，功能不受影响）。
- ~~1.5.21 的钩子自定位~~（已被 1.5.22 取代，仅作历史）：±0x800 自定位与 `SiteOffset` 在 1.5.22 中全部删除，改由地址库定函数 + 函数体内扫 `E8`；当前待验证项见上面 1.5.22 条。
- 1.5.22 的「地址库定函数 + 函数体内扫 `E8`」待实机验证：扫描窗口固定 `0x800`（已知最深 `0x373`），若某运行时把调用点挪出窗口或改成跳转/内联，该钩子会放弃并打印全 `.text` 调用统计；`CustomSkinPlayer` 放弃时幻化改走 100 ms 节流的延迟附加（功能可用，但每次皮肤更新后晚约一帧贴件）；多候选时只钩第一条（可能漏掉 1P / 狼人 / 吸血鬼路径）。`DontVanillaSkinPlayer` 仍假定 actor 在 `r13`、`FixEquipConflictCheck` 仍假定 `rdi`+`rbx`（调用约定固定）。
- 1.5.23 的「历史站点回退」未实机验证：`CustomSkinPlayer` 扫不到调用点时回退到 `entry + 0x81`（AE `+0x1EF`），只校验那里是 5 字节 `E8`、不校验目标——这正是 1.5.17 的行为（用户在 1.5.97 上实测全功能正常）。若某个运行时把该位置改成别的 call 或非 call，日志会打 `is not a 5-byte call either` 并关闭幻化（显示真实护甲、不裸体）。
- 1.5.24 的「用 `rdi` 的 `ActorWeightModel` 贴件、只贴未穿戴件」部分实机：用户已在 1.5.24 看到幻化件显示（m02235 / m02242），但「切换套装后同一槽的克隆是否每次都重建」仍需按 §3.37 B1 观察；1.5.24 的「离开幻化集清空槽」已被证实会把身体（32 槽）拆空，1.5.25 删除。
- 1.5.25 的「离开幻化集不清空」未实机验证：依据是上游 1.5.73 与 1.5.17 从不清空任何槽。已知副作用：切到更小的套装后，不再被接管的槽会短暂保留上一套的克隆，直到引擎因换装/脱装重贴该槽（不会出现空模型）。
- 1.5.26 的「无接口也贴 `.tri`、由 RaceMenu 自己的 `ApplyBodyMorphs` 变形」未实机验证：依据是 skee `BodyMorphInterface.cpp` 690–741 行（`UpdateMorphs` 遍历每个 biped 槽的 `data.object`，`ApplyMorphs` 在子树里找 `NiStringExtraData "BODYTRI"`）。若某个 skee 版本改成只认引擎自己挂的目标，幻化件仍不跟随（症状与修前相同，但日志里 `carries a .tri morph target on N of M` 会显示目标已贴，可用于区分）。另外 `HashSkinnedShape()` 读的 `rawVertexData` 是 GPU 上传前的绑定姿势顶点，若被驱动/ENB 清空会退化为「读不到」并跳过该形状（只影响 250 ms 轮询兜底，不影响贴件与 `.tri` 跟随）。
- 1.5.27 的 `.tri` 目标推导未实机验证：模型路径改问 `biped->objects[i].addon`、候选名扩到四个（`<stem>.tri` / `<stem>_0.tri` / `<stem>_1.tri` / 原样）。已知边界：`.tri` 与 `.nif` 不在同目录、或名字完全不在这四个候选里的件仍不会跟随（日志 `has no .tri beside it … Probed: …` 会列出试过的名字与真实模型路径）；`g_armed` 只按「本角色最后一次 bake 是否有目标」判定，若某角色先武装后失去目标（换了没 `.tri` 的套装）会退回幻化循环兜底。
- 1.5.18 的几何签名（partClone 顶点缓冲 FNV 采样）与 quiet 窗口待实机验证：采样参数按顶点量级估算，若同一身形两次采样哈希不一致会出现多余重建（先调大 `kPostCycleQuiet` 与 `kSignatureSettle` 再缩小采样范围）；SLIF 10 个滑块名按默认配置抄写，改名后靠几何签名兜底。
