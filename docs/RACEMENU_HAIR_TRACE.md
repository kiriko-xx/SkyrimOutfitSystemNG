# RaceMenu hair tint trace — 2026-11-17

范围：分析现有日志与本地随包公开脚本；本文是证据记录及待验证设计，不是生产修复。未改生产代码、DLL、脚本或配置。

## 新对照：同一假发真实装备且不启用幻化（19:34会话）

用户确认真实装备与幻化时为同一假发；不启用幻化，进RM短暂错色后自行恢复。最新日志第270行19:34:55.493 menu=open mask0 base56,59,44，真实headpart tint为0.43921572,0.46274513,0.345098048；第295行19:34:56.349仍open，base13与真实headpart tint0.101960793恢复，节点/材质均替换；关闭仍正确。采样间855ms不等于精确闪色时长。

重要覆盖限制：当前诊断mask0只采headpart-hair，不扫描真实穿戴假发槽；因此日志直接证明的是ActorBase和原生headpart恢复，真实装备假发画面恢复由用户实测证明，不能冒称日志已测得假发材质恢复。两组对照共同把调查范围缩至幻化附加件遗漏后续更新，但未锁定具体回调/写者。下一步如需写色，须先单独采真实装备槽（只读源，与幻化目标分区），或核对公开节点更新事件后的重取路径；不能持续ActorBase覆盖预览。

## 新对照：空幻化无假发（19:21会话）

用户报告只切空幻化、进入RM，无选色操作，真实头发短暂闪错后自行恢复。当前日志第1987行closed mask0 base13/tint0.101960793；2163行19:21:16.212 open mask0 base56,59,44/tint0.43921572,0.46274513,0.345098048；2338行19:21:17.009仍open，base13/tint0.101960793恢复；2339行关闭仍正确。两个采样点间约797ms，不是精确闪色持续时间。真实头发root/material随恢复发生替换；全程幻化mask0且贴件数0，说明闪色不要求存在幻化假发，但不能排除SOS加载或其它mod间接影响。

这次无选色的对照与此前有色板预览的会话必须分开：本次56,59,44确实出现在用户未选色的菜单打开流程，不能反推此前的56,59,44也是初始化。下一调查聚焦真实头发恢复路径与幻化假发未随之恢复的差异，不改已验收形变二进制，不以ActorBase持续覆盖预览。

## 1. 历史会话结论与边界

- 用户澄清：正确发色13,13,13；进入RM时已变错色；随后色板预览56,59,44，再取消（没有确认）后画面恢复正确13,13,13。日志缺少色板打开/预览/取消标记，不能把56,59,44当作进入菜单初始化色，也不能由open标记定位该瞬间。
- 后续发色编辑阶段能回色：seq6/7分别表现为170/128、13/128；不能仅凭events=0断言没有用户操作。
- 第二次开菜单再次记录Tpreview，seq11–19九条滑条采样均未恢复，seq20关闭仍未恢复。
- 当前诊断只扫描covered-slot biped partClone，没有独立真实HeadPart/FaceGen发色源；不能证明“真实头发已恢复，唯独幻化未恢复”。
- 日志不能确定实际写色调用栈、写者或脚本完成先后。初始化链可调查，但此前将56,59,44归因为初始化恢复链的推断已撤回。

## 2. 精简seq表（只描述采样，不能替代操作时序）

来源：[SkyrimOutfitSystemNG.log](../../../bug/SkyrimOutfitSystemNG.log)。events位：open=1、close=2、slider=4；coalesced，**不是完成通知**。

T0=(0.101960786,0.101960786,0.101960786)，Tpreview=(0.43921569,0.4627451,0.345098048)。全部baseRGB省略恒为0的alpha。

| seq | 时间 | 行 | menu/events | baseRGB | tint | slot1根 |
|---|---|---:|---|---|---|---|
| 1旧 | 18:10:23.871 | 93 | closed/0 | missing | 无hair，未loaded | 无 |
| 1 | 18:11:40.746 | 1451 | closed/0 | 0,0,0 | 无hair | R0 |
| 2 | 18:11:42.044 | 1650 | closed/0 | 13,13,13 | 无hair | R0 |
| 3 | 18:11:47.920 | 2029 | closed/0 | 13,13,13 | T0 | R1 |
| 4 | 18:11:56.692 | 2221 | open/1 | 56,59,44 | Tbad | R2 |
| 5 | 18:11:57.471 | 2410 | open/0 | 13,13,13 | Tbad | R2 |
| 6 | 18:12:09.347 | 2415 | open/0 | 170,170,170 | 1.328125 ×3 | R2 |
| 7 | 18:12:11.133 | 2416 | open/0 | 13,13,13 | 0.1015625 ×3 | R2 |
| 8 | 18:12:15.448 | 2419 | closed/2 | 13,13,13 | 0.1015625 ×3 | R2 |
| 9 | 18:12:28.708 | 2605 | open/1 | 56,59,44 | Tbad | R3 |
| 10 | 18:12:29.483 | 2790 | open/0 | 13,13,13 | Tbad | R3 |
| 11 | 18:12:36.548 | 2791 | open/4 | 13,13,13 | Tbad | R3 |
| 12 | 18:12:36.808 | 2792 | open/4 | 13,13,13 | Tbad | R3 |
| 13 | 18:12:37.068 | 2793 | open/4 | 13,13,13 | Tbad | R3 |
| 14 | 18:12:37.328 | 2794 | open/4 | 13,13,13 | Tbad | R3 |
| 15 | 18:12:37.588 | 2795 | open/4 | 13,13,13 | Tbad | R3 |
| 16 | 18:12:37.848 | 2796 | open/4 | 13,13,13 | Tbad | R3 |
| 17 | 18:12:38.109 | 2797 | open/4 | 13,13,13 | Tbad | R3 |
| 18 | 18:12:38.368 | 2798 | open/4 | 13,13,13 | Tbad | R3 |
| 19 | 18:12:38.628 | 2799 | open/4 | 13,13,13 | Tbad | R3 |
| 20 | 18:12:41.204 | 2800 | closed/2 | 13,13,13 | Tbad | R3 |

hairColorForm：seq3为654313473；seq4/9临时为656428；seq5–8、10–20回654313473。

### 指针与根替换

- R0=2095902753472；R1=2095683570560；R2=2095900291648；R3=2095899544448。
- 三个HairTint几何名始终是mesh_70、mesh_69.001、mesh_701。
- seq3几何：2095900417024 / 2095900416256 / 2095900415488。
- seq4–8几何：2089948077824 / 2089948077056 / 2089948076288。
- seq9–20几何：2096250690560 / 2096250692864 / 2096250693632。
- 三几何每次都共享material地址 **2095900105344**，并跨R1→R2→R3保持该地址。支持共享/复用调查方向，**地址相同不能证明同一allocation全程存活**，不能排除释放后重用。

## 3. 两种倍率不能混淆

| 通路观察 | 13 | 170 |
|---|---:|---:|
| 2×RGB/255 | 0.101960784 | 1.333333333 |
| RGB/128 | 0.1015625 | 1.328125 |

(2/255)/(1/128)=256/255，即前者约大0.392%。seq3/4/9符合前者，seq6/7符合后者。不能把它们都写成“归一化RGB”，不能硬编码统一倍率；大于1并不等于坏值。

父调查已核实本地CommonLib-NG：Actor::UpdateHairColor按baseRGB/128调用根的UpdateHairColor；NiAVObject::UpdateHairColor(const NiColor&)遍历HairTint赋值。这是公开库实现依据，不是实际游戏DLL所有写色路径的统一契约；skee的实时UpdateModelHair hook与OnAttach取base路径也应分别核验。

## 4. 本地随包RaceMenu公开脚本通路

来源：[racemenu.psc](../../../兼容参考/捏脸插件-RaceMenu%20Special%20Edition/scripts/source/racemenu.psc)。**没有核对运行中实际PEX与此源码是否同版本**。

- L274–281 OnMenuInitialized：LoadDefaults → UpdateColors → UpdateOverlays → LoadTints → LoadHair → UpdateTints → parent。
- L304–309 LoadHair：仅_customHair时，_hairColor.SetColor(_color)，再_playerActorBase.SetHairColor(_hairColor)；自身不直接Game.UpdateHairColor。
- L215–222 UpdateTints：骑马时Game.UpdateHairColor、Game.UpdateTintMaskColors，并发RSM_ShadersInvalidated；非骑马时QueueNiNodeUpdate。
- L226–229 OnNiNodeUpdate：目标匹配时InvalidateShaders。故不能笼统说LoadHair后“没有任何刷新”；异步节点/失效链是否覆盖本次幻化目标仍待验证。
- L416–420 OnHairColorChange：读取strArg → SetColor → SetHairColor → Game.UpdateHairColor。
- L293–300 OnMenuSliderChange：ChangeHairColorPreset分支仅SaveHair、UpdateColors；不能将任意RSM_SliderChange当作发色更新完成。
- L261–270 OnMenuClose：SaveHair、SaveTints、释放TintTextureCache，不显式UpdateHairColor。

诊断[HairTintDiagnostics.cpp](../src/cpp/HairTintDiagnostics.cpp) L57只收RSM_SliderChange，L193–203只选coveredSlotMask下的biped对象；因此发色事件可能没有slider位，真实HeadPart也未独立采样。

## 5. skee64日志能证明什么

[skee64.log](../../../bug/skee64.log)共226行：L216–219为mgyma几项缺TRI；L222成功载入haircolorfollow.tri（1664407 bytes），L226成功载入hairaccessory.tri（324697 bytes）。没有逐事件时间戳、HairTint色值或写者栈。缺TRI不能据此归因为本次色差；读到BODYTRI也不证明幻化已实际应用morph。

## 6. 可实做优先顺序：先接口与mesh-only验证，发色另立验收

以下为父调查给出的后续优先计划，**本文没有执行**：

1. 先通过修改自身插件文件名，尝试让真实发送者进入旧skee注册范围，内部插件身份不变，取得合法的RaceMenu BodyMorph v4接口；不是修改或替换skee DLL，不绕过版本、ABI或消息校验。成功以实际返回interfaceMap及GetVersion为准。
2. 取得接口后，仅对明确归属幻化的当前clone执行mesh-only ApplyVertexDiff验证；确认顶点变化、目标mesh身份和其他mod morph保留。不能仅凭TRI存在或dispatch成功宣称成功。
3. **拿到BodyMorph接口或mesh-only成功不代表发色修复。** tint独立记录、独立回归；不借此宣称覆盖了RaceMenu实时发色链。

本次日志L4–10、L60–67仅显示skee64/skee定向dispatch失败、广播delivered但map未返回；没有取得BodyMorph接口的成功证据。文件名方案需结合父调查的实际消息注册证据验证，不可由这份发色日志单独推出。

## 7. 发色最小设计与禁止项

当前没有可靠菜单初始化完成信号。菜单open、任意slider、固定延时、base短暂稳定均不构成该信号；base恢复也可能与用户预览编辑交错。

优先调查：捕获正式发色路径实际传入的实时NiColor，在原路径完成后仅镜像到幻化HairTint，重取当前root，按menu/root generation作废旧任务。须先验证自定义发色及预设都经过该路径。

备选：单独采集明确真实HeadPart/FaceGen HairTint作为渲染色源，再同步幻化。现有诊断不够；源缺失、多色歧义、源目标共享材质时应跳过。真实件不得混入幻化target mask；避免对共享material写入反向污染真实头发，必要时先局部克隆隔离目标。

仅在能证实初始化恢复完成且用户发色revision未改变时，才考虑每generation一次reconcile；无法区分时，菜单内不执行base强制重放，关闭后也应等正式提交完成再一次性核验。

禁止持续ActorBase覆盖、每个slider按base重刷、固定延时后强写、复用旧root/裸指针、在全actor根上广泛写材质。即使不修改ActorBase而直接源码写材质，也可能覆盖实时预览或共享源；“只写材质”本身不是安全保证。以上尚未落实为生产代码。
