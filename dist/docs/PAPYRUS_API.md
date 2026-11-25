# Papyrus API（PAPYRUS_API）

所有函数都是 `SkyrimOutfitSystemNativeFuncs` 上的 **Global Native** 静态函数，调用前需要：

```papyrus
import SkyrimOutfitSystemNativeFuncs
```

约定：

- **只支持位置参数**，没有命名参数（CommonLibSSE-NG 注册时不写参数名，与原版一致）。
- 套装名大小写不敏感；空字符串表示“不使用套装”。
- 名字长度上限由 `GetOutfitNameMaxLength()` 给出（当前 256），超长会被拒绝。
- 返回数组没有排序保证；需要排序时用 `NaturalSort_ASCII` / `NaturalSortPairArmor_ASCII`。

## 1. 套装管理

| 函数 | 说明 |
| --- | --- |
| `Int GetOutfitNameMaxLength()` | 名字长度上限（256） |
| `CreateOutfit(String)` | 创建空套装（重名则什么都不做） |
| `DeleteOutfit(String)` | 删除套装，并把引用它的指派清空 |
| `Bool OutfitExists(String)` | 是否存在 |
| `Bool RenameOutfit(String, String)` | 改名；返回 false 表示不存在/重名/超长 |
| `OverwriteOutfit(String, Armor[])` | 用给定护甲重建该套装（不存在则新建） |
| `Armor[] GetOutfitContents(String)` | 套装内容 |
| `String[] ListOutfits(Bool favoritesOnly = false)` | 套装名列表 |
| `Bool GetOutfitFavoriteStatus(String)` / `SetOutfitFavoriteStatus(String, Bool)` | 收藏标记 |
| `AddArmorToOutfit(String, Armor)` | 加一件（同槽位冲突的会被移除） |
| `RemoveArmorFromOutfit(String, Armor)` | 移除一件 |
| `Bool ArmorConflictsWithOutfit(Armor, String)` | 是否会与套装冲突 |
| `RemoveConflictingArmorsFrom(Armor, String)` | 移除所有冲突件 |

## 2. 指派与启用

| 函数 | 说明 |
| --- | --- |
| `Bool IsEnabled()` / `SetEnabled(Bool)` | 全局开关 |
| `String GetSelectedOutfit(Actor)` | 该 actor 当前套装名（空 = 无） |
| `SetSelectedOutfit(Actor, String)` | 设置当前套装（空字符串 = 清除） |
| `AddActor(Actor)` / `RemoveActor(Actor)` | 加入/移出指派表（玩家不可移除） |
| `Actor[] ListActors()` | 指派表里的 actor |
| `Actor[] ActorNearPC()` | 玩家附近的 actor |

## 3. 状态自动换装

> 1.4.4 起地点/状态检测由插件原生事件完成（`AutoSwitch`），Papyrus 只负责查询和配置，不再需要 `SkyOutSysAutoSwitchTrigger` 的 10 秒轮询。

| 函数 | 说明 |
| --- | --- |
| `SetLocationBasedAutoSwitchEnabled(Actor, Bool)` | 开关 |
| `Bool GetLocationBasedAutoSwitchEnabled(Actor)` | 读取 |
| `Int[] GetAutoSwitchStateArray()` | 13 个状态对应的套装（空为 `""`） |
| `Int IdentifyStateType(Actor)` | 当前状态（0..12，见下表） |
| `SetOutfitUsingState(Actor)` | 立刻按当前状态切换 |
| `SetOutfitUsingStateForAllConfiguredActors()` | 对所有配置过的 actor 做同上 |
| `SetStateOutfit(Actor, Int state, String)` / `UnsetStateOutfit(Actor, Int)` | 配置某状态；传入 `"<random>"` 表示进入该状态时随机抽一套 |
| `String GetStateOutfit(Actor, Int state)` | 读取某状态 |
| `NotifyCombatStateChanged(Actor)` | 兼容旧脚本（原版战斗检测用） |

状态数值（与原版一致）：

| 值 | 状态 | 判定条件 |
| --- | --- | --- |
| 12 | Combat | 战斗中（优先级最高） |
| 9 | City | Location 有 `LocTypeCity` |
| 10 | CitySnowy | 同上 + 下雪 |
| 11 | CityRainy | 同上 + 下雨 |
| 1 | Town | 有 `LocTypeTown` 且 `LocTypeCity` |
| 4 | TownSnowy | 同上 + 下雪 |
| 7 | TownRainy | 同上 + 下雨 |
| 2 | Dungeon | 有 `LocTypeDungeon` |
| 5 | DungeonSnowy | 同上 + 下雪 |
| 8 | DungeonRainy | 同上 + 下雨 |
| 0 | World | 其它（兜底） |
| 3 | WorldSnowy | 下雪 |
| 6 | WorldRainy | 下雨 |

注意：只有该 actor **已经给该状态配了套装**才会命中，否则继续往下找，最后回落到 `World` / 基础套装。

## 4. 外观刷新

| 函数 | 说明 |
| --- | --- |
| `RefreshArmorFor(Actor)` | 立刻重算该 actor 的模型 |
| `RefreshArmorForAllConfiguredActors()` | 对全部 actor |
| `Armor[] GetCarriedArmor(Actor)` | 背包里的护甲 |
| `Armor[] GetWornItems(Actor)` | 已穿护甲（真实） |

## 5. 槽位策略

| 函数 | 说明 |
| --- | --- |
| `String[] BodySlotPolicyNamesForOutfit(String)` | 33 项：32 个槽位 + 末尾 blanket，值为翻译键 |
| `SetBodySlotPoliciesForOutfit(String, Int slot, String code)` | 设置某槽策略；空串 = 继承 |
| `SetAllBodySlotPoliciesForOutfit(String, String)` | 设置所有槽 |
| `SetBodySlotPolicyToDefaultForOutfit(String)` | 恢复默认（清空槽位覆盖） |
| `String[] GetAvailablePolicyNames()` / `GetAvailablePolicyCodes()` | 可选策略（翻译键 / 4 字符码） |

策略码含义见 ARCHITECTURE.md §4.1。

## 6. 护甲搜索（菜单/脚本用）

| 函数 | 说明 |
| --- | --- |
| `PrepArmorSearch(String nameFilter = "", Bool mustBePlayable = true)` | 准备搜索结果 |
| `Armor[] GetArmorSearchResultForms()` / `String[] GetArmorSearchResultNames()` | 读取结果 |
| `ClearArmorSearch()` | 释放 |
| `PrepOutfitBodySlotListing(String)` + `GetOutfitBodySlotListingArmorForms/Names/SlotIndices()` + `ClearOutfitBodySlotListing()` | 某套装占用的槽位明细 |

## 7. 字符串工具

| 函数 | 说明 |
| --- | --- |
| `String[] NaturalSort_ASCII(String[], Bool descending = false)` | 自然序排序 |
| `Armor[] NaturalSortPairArmor_ASCII(String[], Armor[], Bool)` | 按名字数组排序护甲数组 |
| `Int HexToInt32(String)` / `String ToHex(Int, Int digits)` | 进制转换 |

## 8. 存档

| 函数 | 说明 |
| --- | --- |
| `Bool ExportSettings()` | 写到 `Data/SKSE/Plugins/OutfitSystemData.json` |
| `Bool ImportSettings()` | 从该文件读回（会重刷所有 actor） |

## 9. NG 新增

| 函数 | 说明 |
| --- | --- |
| `String RandomOutfit(Actor, Bool favoritesOnly = false)` | 随机换装，返回被选中的套装名（无可选时返回空） |
| `Int GetOutfitCount()` | 套装总数 |
| `String GetOutfitNameByIndex(Int index, Bool favoritesOnly = false)` | 按索引取名字（配合分页） |
| `String[] SearchOutfits(String query, Bool favoritesOnly = false)` | 子串搜索（大小写不敏感） |
| `Bool OpenQuickPicker()` | 打开快速选择窗口；返回它是否真的可见（未安装 SKSE Menu Framework 时返回 False，并弹 HUD 提示） |
| `CloseQuickPicker()` | 关闭快速选择窗口 |
| `Bool IsQuickPickerVisible()` | 窗口当前是否可见（可用于脚本轮询） |
| `Bool IsQuickSlotRandom()` | “Change Outfit” 法术当前是否走随机分支（读 ini `[RandomOutfit] QuickSlotRandom`，默认 False = 弹快速选择窗口） |
| `SetQuickSlotRandom(Bool)` | 改这个行为并立即写回 ini |

### 9.1 关于「让 NPC / 其它 mod 察觉幻化」

1.1.0 **删除了**感知层：`IsPerceptionEnabled` / `SetPerceptionEnabled` / `GetPerceivedArmor` 三个函数不再存在，`Actor.GetWornForm` / `Actor.WornHasKeyword` 也不再被重绑。原因是实机日志里判定方从未走 Papyrus 入口（它们读引擎 TESCondition 的 `ExtraWorn` 标记），重绑覆盖不到。

替代方案是针对具体 mod 的数据级兼容（见 `docs/ARCHITECTURE.md` §6 与 `docs/HOOKS.md` §4），脚本侧可以这样验证：

```papyrus
ActorNude = AND_ModEventListener.GetNude(Game.GetPlayer())    ; 幻化成空套装后应为 True
Int integrity = ArmorTear.GetOverallIntegrity(Game.GetPlayer())
```

- **仍然覆盖不到**：直接调用 `Actor.GetWornArmor` / `GetInventory` 的代码（CommonLibSSE-NG 里是内联实现），以及引擎内部 TESCondition（`WornHasKeyword`=682、`WornApparelHasKeywordCount`=722 一类）。这类判定在引擎里看的是真实装备，幻化不改变它们。
- 开关：ini `[Compatibility] AdvancedNudityDetection` / `ProceduralArmorDamage`，菜单 Settings 页也可改。
## 10. 完整注册名清单（66 个）

```
GetOutfitNameMaxLength            GetCarriedArmor                   GetWornItems
RefreshArmorFor                   RefreshArmorForAllConfiguredActors ActorNearPC
PrepArmorSearch                   GetArmorSearchResultForms         GetArmorSearchResultNames
ClearArmorSearch                  PrepOutfitBodySlotListing         GetOutfitBodySlotListingArmorForms
GetOutfitBodySlotListingArmorNames GetOutfitBodySlotListingSlotIndices ClearOutfitBodySlotListing
NaturalSort_ASCII                 NaturalSortPairArmor_ASCII        HexToInt32
ToHex                             AddArmorToOutfit                  ArmorConflictsWithOutfit
CreateOutfit                      DeleteOutfit                      GetOutfitContents
GetOutfitFavoriteStatus           SetOutfitFavoriteStatus           BodySlotPolicyNamesForOutfit
SetBodySlotPoliciesForOutfit      SetAllBodySlotPoliciesForOutfit   SetBodySlotPolicyToDefaultForOutfit
GetAvailablePolicyNames           GetAvailablePolicyCodes           GetSelectedOutfit
IsEnabled                         ListOutfits                       RemoveArmorFromOutfit
RemoveConflictingArmorsFrom       RenameOutfit                      OutfitExists
OverwriteOutfit                   SetEnabled                        SetSelectedOutfit
AddActor                          RemoveActor                       ListActors
SetLocationBasedAutoSwitchEnabled GetLocationBasedAutoSwitchEnabled GetAutoSwitchStateArray
IdentifyStateType                 SetOutfitUsingState               SetOutfitUsingStateForAllConfiguredActors
SetStateOutfit                    UnsetStateOutfit                  GetStateOutfit
ExportSettings                    ImportSettings                    NotifyCombatStateChanged
RandomOutfit                      GetOutfitCount                    GetOutfitNameByIndex
SearchOutfits                     OpenQuickPicker                   CloseQuickPicker
IsQuickPickerVisible              IsQuickSlotRandom                 SetQuickSlotRandom
```

## 11. 示例

```papyrus
import SkyrimOutfitSystemNativeFuncs

Function Test(Actor akActor)
    String[] names = SearchOutfits("daedric", true)
    If names.Length > 0
        SetSelectedOutfit(akActor, names[0])
        RefreshArmorFor(akActor)
    EndIf

    String picked = RandomOutfit(akActor, true)
    Debug.Trace("random outfit: " + picked)
EndFunction
```
