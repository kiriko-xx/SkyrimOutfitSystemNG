# 钩子说明（HOOKS）

本文记录所有引擎介入点、代码生成方式、语义与排查方法。**1.5.22 起没有「偏移」这回事**：改版本时只动各钩子的两个 ID（宿主函数、被调函数），站点由 `FindCallInFunction` 在宿主函数体内运行期扫描确定。**唯一例外是 `CustomSkinPlayer`**（1.5.23）：扫不到时回退到 1.0.0 起一直使用的历史站点（SE `+0x81` / AE `+0x1EF`），只校验那里是 5 字节 `E8`，见 §1.2 与 §2.3。

## 0. 单套实现

1.0.1 起所有钩子只有一份实现（`src/cpp/hooking/Hooks.cpp`），SE/AE 的地址差异全部写在 `REL::RelocationID(seID, aeID)` 里，CommonLibSSE-NG 按当前运行时的 Address Library 选表。只有「注入的机器码本身不同」的地方才用 `REL::Module::IsAE()`：`DontVanillaSkinPlayer` 里取 actor 的寄存器（actor 在 `r13`）与 `FixSkillLeveling` 的 AE 内联分支。`ShimWornFlags` / `CustomSkinPlayer` 的壳代码只碰 `rcx`（成员调用点必然是 `InventoryChanges*`），所以两者没有 SE/AE 差异。

## 1. 基础设施

### 1.1 trampoline

`main.cpp` 在 **kDataLoaded** 才分配 trampoline（不在 `SKSEPlugin_Load` 里，因为通过 SKSE branch pool 提前申请会让 1.5.97 在启动阶段直接终止进程）：

- 用 `REL::Module::get().segment(REL::Segment::textx)` 取 .text 范围，在 ±1 GB 内用 `VirtualQuery` 找 `MEM_FREE` 区域，`VirtualAlloc(MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE)` 64 KB；
- 通过 `Trampoline::set_trampoline(mem, size, deleter)` 交给一个 `new SKSE::Trampoline("local")`；
- `g_branchTrampoline = &SKSE::GetTrampoline()`（SKSE 默认 trampoline，用于 `write_branch`），`g_localTrampoline` 用于 `allocate(gen)` 存放生成的代码。

**为什么必须自己分配**：`write_5branch` 需要 14 字节跳板，`do_allocate` 空间不足时会 `stl::report_and_fail`，那是 `TerminateProcess`，不是异常，`__try/__except` 抓不住。所以任何 `write_branch` 之前都要确认 trampoline 非空且 `free_size() >= 8`。

### 1.2 安装预检（HookSupport）

每个钩子安装前都走同一套检查，任一步失败只写日志并跳过该钩子：

```cpp
if (!Hooking::RequireIds("DontVanillaSkinPlayer", { DontVanillaSkinPlayer_Hook_ID, TESObjectARMO_ApplyArmorAddon })) return;
DontVanillaSkinPlayer_Hook = Hooking::FindCallInFunction("DontVanillaSkinPlayer", DontVanillaSkinPlayer_Hook_ID,
    TESObjectARMO_ApplyArmorAddon);
if (!DontVanillaSkinPlayer_Hook) return;

// 1.5.23：CustomSkinPlayer 多一个历史站点兜底
const std::ptrdiff_t kLegacyOffset = REL::Module::IsAE() ? 0x1EF : 0x81;
CustomSkinPlayer_Hook = Hooking::FindCallInFunctionOrOffset("CustomSkinPlayer", CustomSkinPlayer_Hook_ID,
    InventoryChanges_ExecuteVisitorOnWorn, kLegacyOffset);
if (!CustomSkinPlayer_Hook) return;
```

- `IdExists(REL::RelocationID)`：用 `REL::Offset2ID` 遍历全表建立集合（`REL::ID::address()` 在 ID 缺失时会 `report_and_fail`，不能直接试探）。
- `FindCallInFunction(name, hostId, targetId)`（1.5.22，五个幻化钩子全用它）：
  1. 两个 ID 都要在地址库里（`IdExists`），缺任一个就放弃该钩子（`address library id N is missing for runtime X`）。
  2. 宿主函数入口 = `hostId.address()`，必须在 `.text` 内；扫描范围 = `[入口, min(入口 + kBodyScanLimit, .text 末))`，`kBodyScanLimit = 0x800`（已知最深的站点是 AE 的 `0x373`）。
  3. 在范围内逐字节扫 `E8`，`rel32` 精确等于 `targetId.address()`（或经一跳 thunk 到达）即候选。**没有任何偏移常量**。
  4. 恰好一个 → 采用，debug 记 `the call to 0x… sits at rva 0x… (N byte(s) into the function at rva 0x…)`。
  5. 多个 → 钩**第一个**并 warn（`calls 0x… N times; hooking the first one at rva 0x…`）；其余候选可能落在相邻函数里，寄存器语义无法保证。
  6. 一个都没有 → error `the function at rva 0x… does not call 0x… within 0x800 bytes; the hook stays disabled so the game keeps running.` + 模块身份 + 全 `.text` 到目标的调用点总数与前 5 个 `call at rva`，放弃该钩子（不 patch、不崩）。
- `FindCallInFunctionOrOffset(name, hostId, targetId, legacyOffset)`（1.5.23，只给 `CustomSkinPlayer` 用）：先完全按上面的 `FindCallInFunction` 逻辑扫描，命中就用扫描结果；**一条都没有时**回退到 `entry + legacyOffset`，且要求 `InText(site, 5)` 与首字节 `E8` 同时成立才安装，并 warn `{}: the function at rva 0x{:X} has no call to 0x{:X} within 0x{:X} bytes, but the site this hook has used since 1.0.0 - rva 0x{:X} (+0x{:X} into the function) - is still a 5-byte call; the hook is installed there.`；历史站点也不是 `E8` 时才报 error（`… and the legacy site rva 0x{:X} is not a 5-byte call either; the hook stays disabled…`）并连同模块身份、全 `.text` 统计一起放弃该钩子。**为什么只给它开例外**：1.5.20 引入的「`E8` 目标必须等于 `ExecuteVisitorOnWorn`」校验把 1.5.17 上一直可用、目标却不同的成员调用判死（见 §3.3），而那处调用的语义只依赖 `rcx` 是 `InventoryChanges*`，不依赖目标函数是谁。
- `ResolveSite(name, id, offset, expected, window)` 仍保留，只用于「不是某函数调用」的站点（目前只有 `_DEBUG` 的 RTTIPrinter）。1.5.21 的 ±窗口自定位逻辑留在里面，但正式钩子已不再使用。
- **成对约束（1.5.21，1.5.22 放宽）**：藏真甲的 `DontVanillaSkinPlayer` 与贴件的 `CustomSkinPlayer` 缺一就会出现「真甲被藏、幻化没人贴」= 裸体。1.5.21 的做法是两个都必须装上，否则整条幻化关闭（fail-closed，显示真实护甲）。1.5.22 起 `CustomSkinPlayer` 有了第二条贴件路径（§3.3 的延迟附加），所以 `ApplyPlayerSkinningHooks()` 只要求 `DontVanillaSkinPlayer`：`SetIllusionDisplayEnabled(g_dontVanillaSkinPlayerInstalled)`。日志三分支：
  - `Illusion display is active: the player skinning hooks are installed.`（快路径）
  - `Illusion display is active: the hiding hook is installed and the display set is attached from the deferred pass.`（钩不到站点时的降级路径，功能正常）
  - `Illusion display is off for this session: …`（critical，藏真甲的钩子都没装上 → 显示真实护甲）；菜单、脚本、存档不受影响。
- 全部成功后 `g_branchTrampoline->write_branch<5>(site, code)`；trampoline 空间不足时 `Install()` 返回 false（`do_allocate` 失败是 `TerminateProcess`，不能靠异常兜）。

## 2. 钩子表

1.5.22 起没有「偏移」列：站点统一由 `FindCallInFunction` 在宿主函数入口后的 `0x800` 字节内扫描「`E8` 且 `rel32` 等于被调用函数地址」的调用点确定。地址库只提供两个 ID —— 宿主函数（谁）与被调用函数（调谁）。

### 2.1 SE 1.5.97 的 ID 表

| 钩子 | 宿主函数 ID | 被调用函数（ID） |
| --- | --- | --- |
| DontVanillaSkinPlayer | 24232 | TESObjectARMO::ApplyArmorAddon (17392) |
| ShimWornFlags | 24220 | InventoryChanges::GetWornMask (15806) |
| CustomSkinPlayer | 24231 | InventoryChanges::ExecuteVisitorOnWorn (15856) |
| FixEquipConflictCheck | 36979 | BGSBipedObjectForm::TestBodyPartByIndex (14026) |
| FixSkillLeveling | 37589 | BipedAnim::VisitGetWornArmorTypeVisitor (37688) |

### 2.2 AE 1.6.1130+ 的 ID 表

| 钩子 | 宿主函数 ID | 被调用函数（ID） |
| --- | --- | --- |
| DontVanillaSkinPlayer | 24736 | TESObjectARMO::ApplyArmorAddon (17792) |
| ShimWornFlags | 24724 | InventoryChanges::GetWornMask (16044) |
| CustomSkinPlayer | 24725 | InventoryChanges::ExecuteVisitorOnWorn (16096) |
| FixEquipConflictCheck | 38004 | BGSBipedObjectForm::TestBodyPartByIndex (14119) |
| FixSkillLeveling | 38627 | BipedAnim::GetActorRaceBodyArmorPtrPtr (15696) |

> AE 侧 `CustomSkinPlayer` 的宿主是 24725：`24735` 的那条调用被编译期内联进了 24725，所以从这个函数里扫到 `ExecuteVisitorOnWorn` 才是正确站点。AE 的 `FixSkillLeveling` 也不再调用 `VisitGetWornArmorTypeVisitor`，改调 `GetActorRaceBodyArmorPtrPtr`，壳代码不同（命中后 `mov r12, 0` 再跳到 `site + 0x7B`）—— 这是真正的机器码差异，所以按 `REL::Module::IsAE()` 分支。

### 2.3 站点自证

每个钩子装完后日志都有 `<Hook>: the call to 0x<目标> sits at rva 0x<站点> (N byte(s) into the function at rva 0x<入口>).`（debug）。`CustomSkinPlayer` 若走的是历史站点回退，则没有这一行，而是 warn `… the site this hook has used since 1.0.0 - rva 0x… (+0x… into the function) - is still a 5-byte call; the hook is installed there.`。把 N 与「入口 + N」对照 2.1 / 2.2 的旧偏移（`0x302` / `0x7C` / `0x81` / `0x97` / `0x52`；AE `0x302` / `0x80` / `0x1EF` / `0x97` / `0x373`）即可判断这份 exe 有没有漂移。

## 3. 各钩子语义

### 3.1 DontVanillaSkinPlayer —— 阻止真实护甲贴图

`ShouldOverride(armor, target)`：
1. `ShouldOverrideSkinning(target)` 为假（未启用或该 actor 没指派套装）→ 返回 false，走原版；
2. 收集该 actor 的已穿件，算 `compute_display_set_c`；
3. 当前护甲**不在**幻化集合里 → 返回 true，跳过 `ApplyArmorAddon`。

生成的壳代码：保存 `rcx/rdx/r8/r9` → `mov rdx, r13`（actor）→ `call ShouldOverride` → `test al, al` → 真则直接 `jmp` 回原址 +5，否则 `call ApplyArmorAddon` 后返回。actor 取 `r13` 是这处调用点的固定事实（调用约定，不随版本变），1.5.22 保留。

另外它会调用 `CustomSkinPlayer::MarkAttachPending(actor)`：当 `CustomSkinPlayer` 的壳钩子没装上时，这一次「藏真甲」就顺带把「延迟贴件」排进主线程任务（§3.3）。

### 3.2 ShimWornFlags —— 伪造 worn 掩码

`OverrideWornFlags(inventory)`（1.5.22 起单参数）：这是**替换**引擎对 `InventoryChanges::GetWornMask()` 的调用，所以每条路径都必须交回一个掩码 —— 该 actor 正被幻化绘制时返回**幻化集合的槽位掩码**；幻化关闭 / 没套装 / owner 不是 actor 时返回 `inventory->GetWornMask()`（原版掩码，**不是 0**）。actor 从 `rcx`（`InventoryChanges*`）的 `owner` 取，不再假定它在 `rsi`（SE）/ `rbx`（AE）。

为什么必须交回幻化掩码：引擎认为某槽没穿，后续 `CustomSkinPlayer` 贴上的模型会被上层逻辑清掉，表现为“戴帽子就秃头”。（真实掩码与幻化掩码**不并集**：并集会让引擎以为幻化件也是穿着的，装备冲突检测与技能结算会跟着出错，那两块各有专门的钩子。）

### 3.3 CustomSkinPlayer —— 把幻化件贴上去

先调用原 `ExecuteVisitorOnWorn`（保持其它 mod 的共钩行为），再 `if ShouldOverrideSkinning(actor) Custom(actor, actorWeightModel)`。

1.5.21 之前这是**唯一**把幻化件贴到角色身上的路径：它没装上而 `DontVanillaSkinPlayer` 装上了，结果就是真护甲被藏、幻化没人贴 —— 裸体。1.5.22 曾试图用「延迟贴件」替代它并保持幻化开启，实机证明只装一半时玩家看到的是裸体，因此 1.5.23 起**两者必须都在**（`illusionAvailable = DontVanilla && CustomSkinPlayer`，见 §5 的 `Illusion display is off for this session`）：

1. **快路径（壳钩子装上，1.5.24 起唯一路径）**：shell 保存/恢复 `rcx`/`rdx`，先 `call ExecuteVisitorOnWorn`（保持其它 mod 的共钩行为），然后**直接用该站点的寄存器契约**：`rbx` = 正在蒙皮的对象（`TESObjectREFR*`），`rdi` = 引擎刚填好的 `ActorWeightModel*`。顺序是 `mov rcx, rbx` → `call ShouldOverrideSkinning` → 真则 `mov rcx, rbx; mov rdx, rdi` → `call Custom(actor, model)`。1.5.20–1.5.23 曾改成只传 `InventoryChanges*`、再把 `&actor->GetBiped(fp)` 强转成 `ActorWeightModel*`，那是错的结构（该类型在 CommonLibSSE-NG 里只有前向声明），结果每件都是 `clone 0x0`（只显示头手）。
2. **延迟路径（代码兜底，1.5.23 起实际不会触发）**：`DontVanillaSkinPlayer` 每次藏真甲、以及 `ShimWornFlags` 每次交出幻化掩码时，都会 `MarkAttachPending(actor)`：按 FormID **100 ms** 节流入队，`SKSE::GetTaskInterface()->AddTask` 在主线程 `DrainAttachPending()` → `LookupByID<Actor>` → `Custom(actor, nullptr)`。装上了壳钩子时 `SiteHookInstalled()` 为真，这条路径完全不启用（零开销）；壳钩子没装上时幻化整条关闭（`ShouldOverrideSkinning` 恒 false），所以它也不会被排队。保留它是为了让「壳钩子在某次调用里被跳过」这类边角情况仍有一帧级的兜底。

**站点怎么来**：`FindCallInFunction` 先扫宿主函数体；扫不到时回退到 1.0.0 起的历史站点（SE `+0x81` = rva `0x364301`，AE `+0x1EF`），只校验首字节是 `E8`。1.5.17 正是这样在 1.5.97 上工作且全功能正常的，1.5.20 的目标校验与 1.5.21/1.5.22 的纯扫描都是把它误判成「站点不存在」。

**只贴未真实穿戴的件**：displaySet 里真实穿着的件由原版通道贴（`DontVanillaSkinPlayer` 对 displaySet 内的件放行），我们只贴 `displaySet - equipped`。这里**不做** `ClearSlots(displayMask)`：换件时 `ApplyArmorAddon` 看到 `slot.item` 变了会自己重建克隆，先清空再重贴在 1.5.14 就被证伪（整批 `clone 0x0`）。

**离开幻化集的槽一律不清空（1.5.25）**：1.5.24 曾对「上次在集合里、这次不在」的槽调用 `ClearSlots` → `RE::BipedAnimAugments::ClearBipedPart` 并把 `BipedAnim::objects[bit]` 的 `item` / `addon` / `part` 置空；那是引擎给该槽维护的唯一条目（含引擎自己的皮肤部件），清空后装备栏不变引擎就不会重建，身体槽（CK 32）永久空模型（m02242）。1.5.25 删除了 `ClearSlots` 与 `SlotClone`，改为只打 `Custom: N slot(s) left the display set (mask 0x…); left to the engine for …`，把槽完全交回引擎。

1.5.25 起文件里已没有 `ClearSlots` / `SlotClone`；`ModelBiped(actor, model)` 只为状态探针保留：贴完一件后 `LogSlotState` 用它取 `BipedAnim*` 打 debug 行 `Custom: {:08X} slot {:02} item {:08X} addon {:08X} part {} clone {}`（`addon 0` = ARMA 未解析，`part true` = 几何已建）。它读模型第一个字段并**只接受该 actor 的两套 biped 之一**，否则 warn 一次并跳过。`Custom(actor, nullptr)`（延迟路径，1.5.23 起实际不会触发）会把 `&actor->GetBiped(fp)` 当地址用，`ModelBiped` 对它是成立的，但 `ApplyArmorAddon` 可能拒绝，所以这条路径仅作兜底。

`Custom` 里取 `TESNPC::race` 与 `GetSex() == SEX::kFemale`，对集合中**本人没穿**的件逐件 `ApplyArmorAddon(armor, race, actorWeightModel, isFemale)`。

**诚实还原**（1.4.0 起真正实现）：`ApplyArmorAddon` 会把 `BipedAnim::objects[i].item` 改成幻化件，而后续战斗结算和各种读 biped 的 mod 读的就是这个字段。做法：

1. 贴件前按 `equipped` 建 `wornBySlot[32]`（BOD2 槽位 → 真实护甲）；
2. 每件 `ApplyArmorAddon` 返回后，遍历该件槽掩码，`if (slot.item == 这件幻化件 && wornBySlot[bit]) slot.item = wornBySlot[bit];`
3. 第一人称与第三人称 biped 各做一遍。

只撤销自己刚写入的值（判等 `slot.item == a_applied`），真实穿着的槽不受影响；外观仍由 `addon`/`part`/`partClone` 提供，数值仍来自真实装备。开关是 `[Compatibility] ProceduralArmorDamage`（默认 on，关掉即回到旧行为）。1.0.0–1.3.0 的代码**没有**这一步，只有文档写了它——这是 PAD 不生效的根因，见 §4.2。

### 3.4 FixEquipConflictCheck —— 修复“叠头盔”

原版装备冲突检测遍历已穿件，若幻化集合里有同槽位的件，就会判定冲突并卸掉真实件。本钩子把遍历范围内的候选替换成“真实已穿件”，幻化件不参与比较。

### 3.5 FixSkillLeveling —— 修复技能升级

原版按 biped 上实际贴的护甲类型累加轻/重甲经验。幻化后贴的是幻化件，于是穿重甲练轻甲。本钩子改为按**真实**已穿件（跳过盾牌，`kBody` 双计）统计 `light`/`heavy` 计数。

## 4. 第三方 mod 兼容（零 hook / 零补丁）

**原则**：兼容层不 hook 别人的函数、不改别人的代码段、不读别人的内部结构，只用对方已经对外发布的数据契约。这样对方升级（甚至重编译）后仍然有效，只要契约不变。代码在 `src/cpp/compat/`。

### 4.1 为什么删掉了 Papyrus 重绑

1.0.2 / 1.0.3 曾用 `RE::BSScript::Internal::VirtualMachine::RegisterFunction` 覆盖 `Actor.GetWornForm` 与 `Actor.WornHasKeyword`（每次 kDataLoaded / kNewGame / kPostLoadGame 各注册一次，因为 VM 读档会重建类型表）。实机日志里这两条重绑都成功了，但**用户触发判定的 mod 一次都没有走过 Papyrus 入口**——它们读的是引擎 TESCondition（`ExtraWorn` 标记，条件函数表不在 Address Library）。既然覆盖不到真正的判定路径，1.1.0 把整块功能删除，改为下面两个定点兼容。

### 4.2 Procedural Armor Damage：靠“诚实还原”天然兼容

PAD 1.0.4 的可撕件判定链（逆向 PDB + 反汇编得到，仅用于确认兼容性，代码里不含任何 PAD 内容）：

| 步骤 | PAD 做什么 | 依赖的数据 |
| --- | --- | --- |
| `CollectWorn`（RVA 0x33DF0） | 从**玩家背包**收集 `InventoryEntryData::IsWorn` 的护甲，排除 `Actor::GetSkin`，加上 `ExtraDataList` 里的破损副本 | 真实穿戴状态 |
| `ScanScene`（RVA 0x331C0） | 遍历 `BipedAnim::objects[32]`，只保留 `BIPOBJECT.item` 落在上一步集合里的槽，取 `partClone` 的模型名 + addon formID 作 MeshKey | biped 上的 `item` |
| `Capture` / `TearArticles` | 按 MeshKey 记损伤、删三角面 | — |

**1.4.0 起**本插件在 `CustomSkinPlayer` 里贴完幻化件后，会把**真实护甲写回** `BIPOBJECT.item`（§3.3 的“诚实还原”），因此 PAD 的两个门禁都成立，撕坏的是幻化外观下那件真实护甲。（1.0.0–1.3.0 缺这一步：`item` 留的是幻化件，`ScanScene` 直接跳过整槽，所以 PAD 在幻化外观上完全不生效。）我们唯一做的事是**记录**“哪些幻化件下面没有真实护甲”（PAD 看不到它们）。1.2.0 起这条记录改为**掩码变化时才打一条 INFO**（不再要求开 `ExtraLogging`），并提供了 `Settings → Mod compatibility → Log slot report` 的一键逐槽报告（`Compat::Pad::Diagnose`）。

还有一条容易误判的门禁：Papyrus 的 `TearArmor` 会检查「该护甲在玩家背包里至少有一个副本带 ExtraWorn 标记（真穿着）」，并且要求 actor 是玩家、护甲非空。这条**只作用于脚本调用 `TearArmor`** 的路径，PAD 自己的撕裂走原生 `ScanScene`/`TearArticles`，不经过它——所以幻化件不是被它拦下的，也不需要绕过它。

另外注意 PAD 自己的 “Tracked armors” 页只列**已经产生过损伤记录**的护甲副本（PAD 字符串 `No tracked damage in this save.` / `Each entry is one armor copy...`），列表为空不代表 PAD 没看到装备。

### 4.3 Advanced Nudity Detection：写它对外发布的阵营

AND 4.2.0 的判定结果通过阵营发布（逆向结论）：

| 事实 | 证据 |
| --- | --- |
| `ActorWornHasKeyword`（RVA 0x24B20）/ `PlayerWornHasKeyword`（0x2A070）/ `GetPlayerWornObjectList`（0x260D0）扫真实装备的护甲关键词，随后写 `AND_NudeActorFaction` / `AND_ToplessFaction` / `AND_BottomlessFaction` 等阵营 | 三个函数实现同构：`GetInventory` → `IsWorn` → `As<BGSKeywordForm>` → 虚 `HasKeyword` |
| `GetNude`（RVA 0x3D600，113 字节）就是 `Actor::GetFactionRank(AND 阵营, isPlayer) > 0` | 反汇编 |
| AND 自己的 Papyrus getter 与对话条件都读这份阵营数据 | `AND_ModEventListener.GetNude/GetTopless/GetBottomless` |

所以本插件只做一件事：按当前幻化算出的槽位掩码，用引擎调用 `Actor::AddToFaction`（等同 Papyrus `SetFactionRank`）写这三个阵营的等级。

| 阵营 | 等级 1 的条件 |
| --- | --- |
| `AND_NudeActorFaction` | 掩码不含 `Body(2)` |
| `AND_ToplessFaction` | 掩码不含 `Body(2)` / `ModChestPrimary(16)` / `ModChestSecondary(26)` 中任何一位 |
| `AND_BottomlessFaction` | 掩码不含 `Body(2)` / `ModPelvisPrimary(19)` / `ModPelvisSecondary(22)` / `ModLegRight(23)` / `ModLegLeft(24)` 中任何一位 |

- 阵营按 **EditorID** 解析（`TESForm::LookupByEditorID<TESFaction>`），不按 FormID、不按 RVA → AND 更新后依然有效；找不到时只写一条 warn 并退回原版判定。
- AND 会按自己的节奏重扫真实装备覆盖等级，所以每次换装刷新后立即同步一次，另有输入事件心跳（节流 1 秒，跳回主线程）补写。
- 近似性：只看槽位掩码，不按 AND 的关键词做精判；只写上述三个阵营，不碰 `AND_Showing*` 与 Modesty 系列。
### 4.4 菜单语言检测：只读配置，不是 hook

1.2.0 的中英双语完全建立在菜单框架自己的字体能力上：插件读 `Data/SKSE/Plugins/SKSEMenuFramework.ini` 的 `[Fonts] EnableChinese` / `PrimaryFont`，再检查 `Data/SKSE/Plugins/Fonts/<PrimaryFont>` 是否存在，决定要不要出中文。**不修改框架配置、不注入字体、不碰 ImGui 字体图集**（框架在 DXGI Present 首次初始化时才建字体图集，我们的回调那时已经太晚）。字体缺失就退回英文并写一条 warn。

## 5. 日志与排查

日志文件：`Documents/My Games/<game>/SKSE/SkyrimOutfitSystemNG.log`。

| 日志 | 含义 | 处理 |
| --- | --- | --- |
| `Trampoline ready: N bytes at rva 0x... (N free).` | 私有 trampoline 分配成功 | 正常 |
| `Skinning hooks skipped: no trampoline.` | 分配失败（通常是地址空间被占满） | 关掉其它大型插件或反馈 |
| `<hook>: address library id N is missing for runtime X` | 该运行时没有这个 ID | 升级本插件（改 ID 表） |
| `<hook>: unexpected opcode 0xNN at +0x...` | 站点不是 call/jmp，可能被别的 mod 改过 | 该钩子被跳过，游戏可继续运行 |
| `Skinning hooks: SkyrimSE.exe 1-5-97-0, N bytes, written YYYY-MM-DD HH:MM:SS, base 0x..., .text rva 0x... + 0x...` | 1.5.21：装钩子前先记下"跑的是哪个 exe"（版本、大小、写入时间、.text 范围） | 反馈问题时连这一行一起发 |
| `CustomSkinPlayer: the call to 0x... sits at rva 0x... instead of 0x... (moved by N bytes); hooking the relocated site.`（warn，**1.5.21 专有，1.5.22 起不再出现**） | 1.5.21：偏移对不上，但在附近找到了唯一那条调用 | 正常，钩子已装到自定位出来的站点 |
| `... .text holds exactly one call to 0x..., at rva 0x... (N bytes from the nominal site); hooking that one.`（warn，**1.5.21 专有**） | 1.5.21：附近没有，但全 `.text` 只有这一处调用 | 正常，仍是同一条调用路径 |
| `CustomSkinPlayer: the function at rva 0x... has no call to 0x... within 0x800 bytes, but the site this hook has used since 1.0.0 - rva 0x... (+0x... into the function) - is still a 5-byte call; the hook is installed there.`（warn，1.5.23） | 历史站点回退生效：该构建的函数体里没有直接调用，但 1.0.0 起的老站点仍是成员调用 | 正常，幻化应可用 |
| `CustomSkinPlayer: ... and the legacy site rva 0x... is not a 5-byte call either; the hook stays disabled ...`（error，1.5.23） | 连历史站点都不是 `E8`：这份 exe 改了这个函数 | 幻化本局关闭（真实护甲可见、不裸体）；把本行与随后的 `the function at rva` / `.text holds N call(s)` / `call at rva` 行发回 |
| `... N calls to 0x... sit inside the +/-0x800 window; refusing to guess...` + 每个 `candidate at rva 0x...`（**1.5.21 专有**） | 1.5.21：附近有多条候选，判断不了哪条进入时寄存器里是 actor | 幻化本局关闭（真实护甲可见），把这批 `candidate` 行发来即可补偏移 |
| `Illusion display is off for this session: the player skinning hooks are not both installed (vanilla pass missing\|hooked, display pass missing\|hooked). The real equipped armour stays visible. Nothing else is disabled.`（critical） | 1.5.21 起（1.5.23 恢复）：成对钩子缺一个，幻化关闭以免裸体 | 把上面的 `CustomSkinPlayer` 失败行与本行一起反馈；这一局不会裸体 |
| `ShouldOverrideSkinning: 0x... is not a readable object; ignoring the call.`（warn） | 1.5.21：自定位钩到的位置传进来的不是有效指针（防御，正常不该出现） | 偶尔一两行可忽略；持续出现说明钩错位置，反馈日志 |
| `hook installed at rva 0x...` | 安装成功 | 正常 |
| `<Hook>: the call to 0x... sits at rva 0x... (N byte(s) into the function at rva 0x...).`（debug） | 1.5.22：站点由「地址库定函数 + 函数体内扫 `E8`」决定，这行说明钩在哪儿（需要 Verbose logging） | 正常；把 N 与 §2.3 的旧偏移对照即可判断该 exe 有没有漂移 |
| `<Hook>: the function at rva 0x... does not call 0x... within 0x800 bytes; the hook stays disabled so the game keeps running.`（error） | 1.5.22：这个运行时的函数体里没有到目标函数的调用（被内联/改写） | 紧跟的 `the function at rva` / `.text holds N call(s)` / `call at rva` 行发回；`CustomSkinPlayer` 缺这条时幻化仍可用（延迟附加），`DontVanillaSkinPlayer` 缺这条则幻化关闭（显示真实护甲） |
| `<Hook>: the function at rva 0x... calls 0x... N times; hooking the first one at rva 0x.... If the illusion skips a path in this game build, report this line.`（warn） | 1.5.22：多条调用只钩第一条 | 幻化正常就不用管；某些路径（1P / 狼人 / 吸血鬼）不刷新时把这行发回 |
| `Illusion display is active: the hiding hook is installed and the display set is attached from the deferred pass.`（info） | 1.5.22：`CustomSkinPlayer` 的壳钩子没装上，幻化改由主线程延迟任务贴件 | 正常，属预期降级（每次皮肤更新后约一帧） |
| `CustomSkinPlayer: the display set is attached from inside the skinning routine.`（info） | 1.5.22：壳钩子装上了，走快路径 | 正常 |
| `Custom: the skinning routine of this game build has no call site to hook; attaching the display set from the deferred pass.`（debug） | 1.5.22：第一次走延迟路径时打一条 | 正常，说明这条路真的在用 |
| `Custom: attaching N outfit-only piece(s) of M displayed (mask 0x..., outfit revision ..., body morph ...) for ...`（debug） | 1.5.24：每次贴件的开启行，N = 我们要贴的件数，M = 幻化集合总件数 | 正常 |
| `Custom: N slot(s) left the display set (mask 0x…); left to the engine for …`（debug，1.5.25） | 这些槽不再由幻化接管，原样交回引擎（1.5.24 的 `cleared for …` 已删除） | 正常；不应因此出现空模型（身体掩码 `0x4`） |
| `Custom: … slot NN item … addon … part … clone …`（debug，1.5.25） | 贴件后的槽状态：`addon 0` = ARMA 未解析，`part true` = 几何已建 | 「贴上了但看不见」时用它取证；`clone 0x0` 本身不算失败证据 |
| `Custom: ... slot NN clone 0x...`（debug） | 该槽的克隆节点指针 | **非 0 才算贴上**；`0x0` 说明 `ApplyArmorAddon` 拒绝 |
| `Custom: ApplyArmorAddon refused ... for weight model 0x...; that slot stays empty.`（warn，5 秒节流） | 引擎拒绝为这件建 addon（模型不对、AA 缺模型、种族不符） | 该槽会空着；把本行与 `slot NN clone 0x0` 一起反馈 |
| `Custom: the weight model 0x... does not point at a biped model of ...; leftover slots are left alone.`（warn，只打一次） | `ModelBiped` 校验没通过：模型第一个字段不是该 actor 的两套 biped 之一 | 只影响「离开幻化集合的槽是否清理」，不影响贴件 |
| `Compatibility: Advanced Nudity Detection ... detected; nudity, topless and bottomless ranks now follow the displayed outfit.` | AND 兼容生效 | 正常 |
| `... was found, but its nudity factions are missing` | AND 装了但阵营 EditorID 找不到 | 兼容自动退回原版行为 |
| `PAD compatibility: ... without a real armour underneath` | 该幻化件下面没有真实护甲 | PAD 撕不到，属预期 |
| `Menu: SKSE Menu Framework not found` | 没装菜单框架 | 只是没有 UI，其它功能正常 |

## 6. 版本升级步骤

1. 用 Address Library 查新版本的**函数 ID**：宿主函数（谁在调用）与被调用函数（调谁）。1.5.22 起不需要查偏移、也不需要反汇编找站点。
2. 只改 `src/cpp/hooking/Hooks.cpp` 里对应 `REL::RelocationID(seID, aeID)` 的两个 ID（`_Hook_ID` 与目标函数 ID）。新增运行时版本就加一列，**不新增代码分支、不新增 ID 表文件**。
3. 若新版本把那条调用内联掉了（宿主函数里扫不到 `E8` 到目标函数），日志会打 `does not call … within 0x800 bytes` 并列出全 `.text` 的调用点 —— 按 §2.2 的 AE 案例换宿主函数（哪个函数里还有这条调用就用它）。`CustomSkinPlayer` 例外：它扫不到时会自动回退到历史站点（warn `the site this hook has used since 1.0.0`），只有当那处也不是 `E8` 时才失败；此时再按反汇编找真实调用点，不要凭猜改 `kLegacyOffset`。
4. 编译后看日志：每个钩子一行 `the call to 0x… sits at rva 0x…`，最后一行是 `Illusion display is active: …`（任一分支）。若出现 `calls 0x… N times; hooking the first one`，说明该构建有多条路径，把这行记下来。
5. `_DEBUG` 的 RTTIPrinter 仍用 `ResolveSite(…, 0x2A)` —— 它只是调试打印，不影响发布版。
