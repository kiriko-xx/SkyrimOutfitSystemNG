# 原版 MCM 功能对照表（1.3.0）

原版 `SkyOutSysMCM.psc`（1,099 行，SKI_ConfigBase）里每一个控件，现在在哪个 SMF 菜单里、行为是否一致。
凡是写着「等价」的，都是同一份原生实现被换了界面，不存在两套数据。

## 1. Options 页

| 原版控件（脚本状态名） | 原版行为 | SMF 菜单位置 | 说明 |
| --- | --- | --- | --- |
| `OPT_Enabled` | `IsEnabled()` / `SetEnabled()` 总开关 | Settings → General → *Enable outfit overrides* | 等价：同一个 `OutfitService::set_enabled`，另外同步写 ini |
| `OPT_SelectActorSelection` | 选一个「当前操作的角色」，之后的套装操作都作用在它身上 | Outfits → 工具条 *Apply to* | 等价：下拉里是「玩家」+ 已配置角色；列表的 Wear / 双击、编辑器的 Wear / Take off 都跟着它走 |
| `OPT_QuickslotsEnabled` | 通过 `SkyOutSysQuickslotManager.SetEnabled()` 给玩家加/去 `SkyrimOutfitSystemQuickslotSpell` | Settings → General → *Give the player the "Change Outfit" power* | 等价，但改成原生实现（`Features::ApplyQuickslotSpell`），不再依赖那个 quest 脚本；解析顺序：EditorID → `SkyrimOutfitSystem.esp:0x000182F` |
| `OPT_AddActorSelection` | 从「玩家附近的 NPC」里挑一个加入管理列表 | Actors → *Add a nearby actor...* | 等价：`Features::ActorsNearPlayer()`（同 cell、跳过尸体与不允许的 FormID） |
| `OPT_RemoveActorSelection` | 从列表里移除角色（玩家除外） | Actors → 每个角色的 *Remove* | 等价，加了确认提示（原版也确认） |
| `OPT_ExportSettings` | `ExportSettings()` | Settings → *Export outfits to OutfitSystemData.json* | 等价（同一原生函数）；1.4.3 起写出的是与原版**完全相同结构**的 `Data\SKSE\Plugins\OutfitSystemData.json`（策略字段是数字枚举）（prost protobuf-JSON 命名），可与原版互相导入 |
| `OPT_ImportSettings` | `ImportSettings()` + 全量刷新 | Settings → *Import outfits from OutfitSystemData.json* | 等价；可直接吃原版导出的文件（`enabled` / `outfits` / `actorOutfitAssignments` 及 OBSOLETE 数字策略字段） |
| `OPT_AutoswitchEnabled` | `Get/SetLocationBasedAutoSwitchEnabled(actor)` | Actors → *Location/state based switching* | 等价 |
| `OPT_AutoswitchEntry<state>` | 13 个状态的套装下拉，空 = 未设置 | Actors → *State outfits* 下的 13 个下拉 | 等价：索引就是 `StateType` 0..12；选 `<inherit>` 等于原版清空该状态 |
| （原版默认键 `OnDefaultST` 的「清空状态套装」确认） | 确认后 `UnsetStateOutfit` | 同上，选 `<inherit>` | 少了确认框，但当前值就显示在下拉里，误点可立刻改回 |

## 2. Outfit list 页

| 原版控件 | 原版行为 | SMF 菜单位置 | 说明 |
| --- | --- | --- | --- |
| `OutfitList_Item_<name>` | 套装列表，带 ActiveMark（当前穿戴）与 ContextMark 标记 | Outfits → 左栏列表 | 当前穿戴的套装前加 `* `；选中项高亮 |
| `OutfitBrowser_Prev` / `_Next` + 页码 | 每页 11 条翻页 | Outfits → 工具条 `\|< < page > >\|` + *per page* | 等价，且每页条数可调（5–500） |
| `OutfitContext_New` | 输入名字新建空套装，校验超长/重名 | Outfits → 底部 *Create empty* | 等价；超长与重名分别提示（`max_outfit_name_len` 与 `add_outfit` 返回值） |
| `OutfitContext_NewFromWorn` | `GetWornItems(player)` → `OverwriteOutfit` | Outfits → 底部 *Create from worn* | 等价 |
| `OutfitContext_Toggle` | 给当前角色穿上 / 脱下选中套装 | Outfits → 行内 *Wear*、编辑器 *Wear* / *Take off* | 等价（`SetSelectedOutfit(name/"") `） |
| `OutfitContext_Favorite` | 收藏开关 | Outfits → 行内 `*` / `-`、编辑器 *Favourite* | 等价 |
| `OutfitContext_Edit` | 进入套装编辑器 | Outfits → 右栏编辑器（选中即进入） | 等价 |
| `OutfitContext_Rename` | 改名，校验超长/重名 | Outfits → 底部改名输入框 + *Rename* | 等价 |
| `OutfitContext_Delete` | 确认后 `DeleteOutfit` | Outfits → 行内 *Delete* → 确认条 | 等价（确认条是内联的：SMF 没有弹窗 API） |

## 3. 套装编辑器

| 原版控件 | 原版行为 | SMF 菜单位置 | 说明 |
| --- | --- | --- | --- |
| `OutfitEditor_Back` | 返回列表 | 编辑器 → *Back to the list* | 等价 |
| `OutfitEditor_AddFromCarried` | 菜单列出背包里的护甲 | 编辑器 → *Add armour* → *From the inventory* | 等价（`Features::CarriedArmors`） |
| `OutfitEditor_AddFromWorn` | 菜单列出当前穿戴 | 编辑器 → *Add armour* → *From the worn armour* | 等价（`Features::WornArmors`） |
| `OutfitEditor_AddByID` | 输入十六进制 FormID，校验存在且是护甲 | 编辑器 → *Add armour* → *By form ID* | 等价，提示文案与原版一致（`No form with that ID is loaded.` / `That form is not armour.`） |
| `OutfitEditor_AddFromList_Menu` / `_Filter` / `_Playable` | 全护甲表搜索（名字或插件名）+ 仅可玩 | 编辑器 → *Add armour* → *From the full armour list* | 等价；结果上限 400 条并提示「缩小过滤」 |
| 槽位冲突确认（`$SkyOutSys_Confirm_BodySlotConflict_Text`） | 加护甲时若槽位已被占用，问「替换冲突件?」 | 编辑器 → 内联确认条：*Replace the conflicting pieces* / *Cancel* | 等价（`Outfit::conflicts_with` + 按槽掩码交集移除） |
| `OutfitEditor_BodySlot_<i>` + 翻页 | 只列该套装用到的槽位，每件护甲一行 | 编辑器 → *Body slots*（可滚动列表，每行 *Remove*） | 等价；移除护甲也加了确认（原版 `$SkyOutSys_Confirm_RemoveArmor_Text`） |
| `$SkyOutSys_OutfitEditor_OutfitIsEmpty` | 空套装提示 | 编辑器 → *This outfit is empty.* | 等价 |
| `OutfitEditor_EditAllSlotPolicy` | 选 blanket 策略 | 编辑器 → *Slot policies* → *Base policy* | 等价（12 个策略码，见下） |
| `OutfitEditor_ToggleEditSlotPolicy` | 打开 32 槽逐槽编辑器 | 编辑器 → *Per-slot policy editor* 复选框 | 等价 |
| `OutfitEditor_BodySlotPolicy_<slot>` + 翻页 | 32 槽各一个策略下拉，含「Use base policy」 | 同上，32 个下拉（可滚动子窗口） | 等价；槽名沿用 BOD2 30..61 |
| `OutfitEditor_SlotPolicyHelp` | 三条帮助文本 | 编辑器 → *Policies decide what is drawn…* 一行说明 | 简化为一行；策略名本身已经是「Always use outfit」这类自解释文案 |

## 4. 策略码

原版下拉里的 12 个策略码（`$SkyOutSys_Desc_EasyPolicyName_*`），SMF 菜单用同一套码、同一套语义：

| 码 | 含义（原版译文） |
| --- | --- |
| XXXX | Never show anything |
| XXXE | If outfit and equipped, show equipped |
| XXXO | Require equipped |
| XXOX | If only outfit, show outfit |
| XXOE | If both, show equipped |
| XXOO | Always use outfit（默认） |
| XEXX | If only equipped, show equipped |
| XEXE | If equipped, show equipped |
| XEXO | Passthrough（盾牌默认） |
| XEOX / XEOE / XEOO | 其余三种组合 |

## 5. 已知差异（有意为之）

1. **不再有右键上下文菜单**：SMF 只提供平面控件，原版「右键点套装出菜单」改成每行常驻按钮 + 右栏编辑器。
2. **没有 MCM 的 InfoText / HelpText 面板**：说明文字直接画在控件下方。
3. **没有逐项 "Restore defaults"**：策略有 *Reset all policies to the defaults*，套装内容只能手动删。
4. **快捷槽菜单**：原版是 UIExtensions 列表（>100 页会栈溢出），现在是原生 ImGui 快速选择窗口（搜索 + 收藏过滤 + 分页 + 双击应用）。
5. **Export/Import 只导出套装与角色分配**（与原版 `ExportSettings` 的 JSON 一致），不含 ini 里的插件设置。

## 6. 没有实现的两项及原因

| 原版 | 原因 |
| --- | --- |
| `SkyOutSysQuickslotManager` 的 `pbEnabled` 属性 | 那个属性只服务于原版 MCM 的显示；现在开关直接作用于玩家法术，quest 脚本即使在场也不会把状态改回去 |
| MCM 的 `GetAutoSwitchStateArray()` 动态列表 | 我们用编译期固定的 13 个 `StateType`，顺序与原生 `StateType` 枚举一致，不会随存档变化 |

原版 MCM 脚本对外暴露的三个接口没有丢：`SkyOutSysMCM.GetModVersion()`（0x01030000）、`GetVersion()`、`GetQuickslotManager()` 仍可在空壳脚本里调用，第三方 mod 的旧代码不用改。
