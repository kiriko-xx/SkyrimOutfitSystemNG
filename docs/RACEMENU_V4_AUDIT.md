# RaceMenu v4 实机失败审计（进行中，非发布说明）

## 对象与证据

- 用户提供的 SE skee64.dll：SHA256 `255c0db0ba5ff14640cc6d75ccddfb24474b498d35b77f3140ccb05eb80e7273`，不是当前 master。
- 有效 REA 证据包：`D:/Skyrim-dev/tmp/racemenu-release-readonly-evidence.json`，7,754,559 字节，SHA256 `991d33dfff6bb0b2168812a5be8a1efb4f6a215aa6302470eae0294b435e4629`。
- `D:/Skyrim-dev/tmp/racemenu-v4-audit.evidence.json` 是关闭会话后误导出的空包（105 字节），不可用。

## 已证实的版本差异

1. BodyMorphInterface vtable `0x180173ec8`，slot1 `0x180005330` 原始字节 `B8 04 00 00 00 C3`，返回 **4**。slot0..24，无 v5 MorphShapeCallback。现有插件允许 v4，callback 仅 >=5，不能把 v5 当依赖。
2. 交换 handler `0x180051520` 比较 `0x9E3779B9`，通过 message.data 回写 interface map `0x1801b6760`。Load 注册 BodyMorph 对象 `0x1801cacb0`，并在 Load 尾直接注册 catch-all；本 DLL 没有 bEnableEarlyRegistration 配置开关。不得要求用户添加该项。
3. PE 导出只有 SKSEPlugin_Load / SKSEPlugin_Query，无 GetInterface。不得硬读内部 map、硬偏移调用、伪造第三方 sender 或再次调用第三方 Load。
4. ApplyBodyMorphs slot13 `0x1800051b0` -> `0x1800086d0` 收集真实装备及 NPC skin 的 ARMO -> ARMA -> `0x1800d2410` 按 addon 名在 Ni 树中找节点 -> `0x180018360` -> `0x180008460` ApplyMorphs。**不是新版 VisitBipedNodes 全槽遍历**；可读 BODYTRI 不保证幻化节点被访问。
5. ApplyVertexDiff slot12 `0x18004b740` 可对明确传入的节点操作，但仍须通过公开交换取得接口，不能把上述分析地址作为运行时调用入口。

## SKSE 2.0.20 公开契约

[PluginManager.cpp](https://github.com/ianpatt/skse64/blob/v2.0.20/skse64/PluginManager.cpp) 的 RegisterListener(nullptr) 仅填当时已分配的 sender 桶（含预留容量）；Dispatch 只遍历 sender 自己的桶。晚加载的插件可能不在旧 skee 注册的范围；命名 receiver 不会绕过订阅。重试任务和我方监听 skee 不会补上反向订阅。

InstallPlugins 经 IDirectoryIterator 枚举 DLL，当前代码没有显式排序。不能仅靠重命名 DLL 就承诺加载顺序（还受文件系统/VFS 枚举影响）。

## 已修的独立错误，未作为新版发布

- 删除自动 ApplyOBodyMorphsToActor：OBody 的 ReapplyActorMorphs -> GenerateBodyByPreset 会清理/重写 OBody 与 OClothe 的预设值，不是只刷新当前网格；独立 RaceMenu/SLIF key 并不因此全被清空。
- ProviderCanMorph 只承认已取得的 RaceMenu mesh 接口；OBody readiness/query/event 保留。
- 接入公开 RSM_SliderChange 事件：回调只置 atomic pending 标志，不访问 UI/actor/g_api；既有 Tick 主线程任务消费后检查 BodyMorph 接口与 RaceSex Menu，再 RequestBake。多次事件合并，不要求 sender，不写预设或颜色。Reset 清标志。此不是 morph 完成回调，也不修复旧版接口不可达；触发时序仍待实机。
- BODYTRI 创建先分配，再删除旧 extra；插入失败尝试恢复旧 extra，不删其它 extra。
- 编译通过；tests/morph_paths.ps1 六例通过；tests/morph_read_only.ps1 静态防误调用检查通过。tests/morph_slider_queue.ps1 提取生产 DrainRaceMenuSlider 原函数，使用模拟 UI/actor 验证九种场景：无事件、多事件合并、无接口、菜单关闭、无 UI、未加载 3D、无玩家、异线程连续发信号后消费、Reset 丢弃待消费标志。后者通过不等于真实引擎时序或线程压力测试；不验证 SKSE 任务调度和 morph 落地。尚未实机验证，未改基础钩子或清槽。

## 发色与崩溃的证据边界

- 用户新确认：切换幻化发色正常；打开 RaceMenu 的瞬间变错；动发色条才恢复，不动则持续到关闭菜单。不能再归因为挂载初始化缺失，也不能用 ActorBase 色持续覆盖预览编辑值。
- 实际参考包 scripts/source/racemenu.psc:274-281 的 OnMenuInitialized 调用 LoadHair 后 UpdateTints；215-223 非骑马时 QueueNiNodeUpdate，226-229 后续 InvalidateShaders；552-557 只调用 ApplyOverrides/ApplyNodeOverrides/ApplySkinOverrides。416-420 的 OnHairColorChange 立即写 _hairColor、ActorBase.SetHairColor 并 Game.UpdateHairColor，不需要确认颜色板。它解释了用户“动一下即恢复”的公开更新通路，但尚未证明哪个阶段写坏假发材质。
- master 源码的 UpdateModelHair_Hooked 使用实时传入颜色；TintMaskInterface 的 attach/NiNodeUpdate 使用 ActorBase 派生颜色，两来源可能竞争。此仅为调查方向，不是本 DLL 的已证实根因。
- crash-2026-11-17-08-58-59：SkyrimSE+013B21C，package/quest alias 更新，Remiel 相关 package，栈经过 PapyrusUtil/OStim。没有足够证据归责 Face Lighting，也不能排除间接破坏。

## 未完成

公开接口获取在旧版晚加载情况下仍待解决。已核对参考包 nioverride.psc 没有 AttachMesh/DetachMesh；别版上游的 AttachMesh 即使存在也仅证明首次 ApplyVertexDiff，不证明持续形变，不能拿它重构已验证挂载路径。公开 UpdateModelWeight 仅 ref 参数，不能指定现有 clone root；没有已验证第三方代理接口。

在此旧 DLL、真实 sender 不被订阅、禁止内部 patch/伪 sender 的条件下，不能用现有公开 API 保证取得任意 root 形变能力。若进一步独立实现 TRI/morph 计算，须先验证当前各 morph key 的公开读取、原顶点基准、TRI shape/顶点数匹配与渲染缓冲更新，不能直接复制新版 skee 顶点写入然后宣称安全。

实时幻化形变、RaceMenu 内发色、CTD 均未闭环。不得将路径计数、加载 TRI 或编译通过描述为功能修复成功。
