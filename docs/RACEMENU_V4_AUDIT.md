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

## 通用形变调度：公开源码能证明什么

本地公开源码快照 `D:/Skyrim-dev/tmp/skee64` 的 `479ecab` 不是用户的 v4 DLL。`skee64/BodyMorphInterface.h:368–377` 的 `VisitMorphValues` 经过 `skee64/BodyMorphInterface.cpp:1267–1277` 遍历数据，遍历函数本身未加锁；不能把只读访问等同于可与写入并发。`skee64/BodyMorphInterface.cpp:1086–1101` 的 `ApplyVertexDiff` 同步进入形变缓存；`:690–716` 的锁只证明此段缓存访问串行，`:674–685` 延后的是分区更新任务，并非整个节点遍历和形变。`:1076–1079` 的 `AddMorphShapeCallback` 在该快照是空实现；不能用版本号或虚表槽存在推断注册生效，也不外推其他发行版。

`a708434` 是通用形变的实机行为对照，不是线程安全的依据。`d026448` 的线程门禁回归已定位，普通任务及 UI 任务均不能在已复现的菜单时段稳定到达游戏线程。修复需验证框架无关的执行入口和节点访问时相，两者不可相互代替；不添加针对 SAM 的分支，不搬运 v5 内部实现到 v4。

### 同为 v4，上传调度也不同

- [RaceMenu 0.4.14 / SE 1.5.97（7ffff9a）](https://github.com/expired6978/SKSE64Plugins/blob/7ffff9afb35ce9cae5e26f8bdbf86367b75e29b8/skee/BodyMorphInterface.cpp#L500-L663)：`ApplyVertexDiff` 同步执行 BODYTRI 查找和网格处理；`defer=true` 延后的是分区更新。旧缓存入口没有较新快照的外层缓存锁，`Impl_VisitMorphs` 也没有在遍历时取得形变数据锁。
- [后期 v4（7694eab）](https://github.com/expired6978/SKSE64Plugins/blob/7694eab/skee64/BodyMorphInterface.cpp#L632-L730)：CPU 处理后按调用线程决定分区更新是就地执行还是入队；全角色路径改为遍历 biped 与 RaceMenu 附件容器。这两份公开源码可证明接口号不能作为线程契约，但都尚未与现场 DLL 逐一匹配。
- 本地可参考的游戏帧调用点见 `src/jiayev-compendium/src/Features/GrassCollision.h:124` 与 `src/jiayev-compendium/src/Features/GrassCollision.cpp:336–340`。其他功能在那里执行，不等于暂停菜单时仍能到达，也不等于已离开装备/3D 更新或渲染锁。只列为验证候选，不照搬站点或更改现有钩子。

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

## OBody 对照与接口探测（未发布）

- OBody 的 PostPostLoad 请求 payload 是 sizeof(InterfaceExchangeMessage*)，SOS 是 sizeof(exchange)；本协议在 x64 都是 8 字节，不是握手失败原因。OBody 的 Init(false) 在已检查的 CommonLib bool 重载中只关闭自动日志，并非特殊发送者能力。
- SOS 现补上 PostPostLoad 和 DataLoaded 同步纯接口探测，不在早期阶段装事件、不访问角色、不刷新网格。逐通道记录真实 sender handle、receiver、payload size、Dispatch 返回值和 map 是否回写。只有尚未取得接口时请求。此为控制变量和取证，不保证修复旧版订阅缺失。
- 定向 skee 的 delivered=false 表示未投递到该监听者；delivered=true 而 map returned=false 才需查其处理协议。广播 delivered=true 不代表 skee 收到。记录真实句柄后需与同一次启动 SKSE 日志注册顺序交叉核对。
- SKSE 2.0.20 注册 catch-all 的桶向量有 numPlugins+5 预留容量。因此晚加载也可能恰好落在预留范围，不能只按 skee/OBody/SOS 先后顺序断定；需要回放全部注册时点的 resize。
- OBody 现有 v1/v2 公开 API 没有取得 RaceMenu map 或任意 clone root 形变转交能力，禁止把重施 OBody 预设当只刷新网格。

## 只读发色诊断与当前离线验证

- 新模块 HairTintDiagnostics 在 ExtraLogging=on（trace/debug）时启用；普通 info 级别关闭。Install/Reset/采样都由主线程路径调用。菜单和滑块 sink 只合并原子事件位，不读 UI、actor、材质或 g_api。
- 采样在现有 Tick 的 SKSE 主线程任务最前、provider 重试和 morph 操作之前；只观察玩家当前幻化覆盖槽的 partClone。上限 8 根、512 对象/边、64 几何、深度64；截断明确输出 truncated=1。光照材质必须通过 Ni RTTI、kLighting 和 kHairTint 三重检查才读 tintColor，不写任何颜色、模型或预设。
- 记录 baseRGBA、hairColorForm、menu、view、内部0基槽号、addon/root/geometry/shader/material 身份及实际 tint。只在初次、事件或快照变化时输出。事件 open=1/close=2/slider=4 可合并，steadyMs 是采样时刻，不是事件/写入完成时刻。CPU材质值也不等于最终GPU画面。
- tests/morph_interface_mock.ps1 摘取生产 TryRaceMenu 与 ProbeRaceMenuInterface，10个 mock 场景通过，涵盖未递送、无回写、v4接受、v3拒绝、广播误判防护、已取得去重、无messaging与quiet。只验证最小ABI前缀及控制逻辑，不验证真实SKSE路由或已装DLL完整ABI。
- tests/hair_tint_read_only.ps1 验证只读模块预算/类型守卫、sink只发信号、主线程采样先于morph；这只是静态防回归。既有路径6例、滑块队列9例及两个只读/探测静态检查均通过。统一Release构建 BUILD_EXIT=0，无基础Hook/Patches改动。

### 最小实机采样顺序

1. 使用标为诊断构建的 DLL，保留现有配置，只将插件自己 INI 的 ExtraLogging 改 on；不要修改或添加旧 skee 不支持的 early-registration 配置。
2. 启动后保留同一次启动的 skse64.log 与 SOS日志，检查 PostPostLoad/DataLoaded phase 和逐通道 sender/receiver/delivered/map returned；若无接口，不承诺拖滑块能形变。
3. 先让假发颜色正确并停留半秒；打开 RaceMenu 不动半秒；拖发色条；取消色板；关闭菜单。记录每一步画面并保存 HairTintDiagnostics 行。优先比较同一材质身份的 tint 与 baseRGBA；若身份变了，按替换而非同材质改色分析。
4. 再单独测 BodyMorph 滑块、OBody换预设；不要把读取路径或接口握手成功当作实际形变成功。若重现CTD，保留新 crash 日志，不对旧 Remiel/package 栈预先归因。
5. 若菜单期间无采样行，先核对日志级别与主线程heartbeat是否运行；不能据此断言颜色没变。诊断后将 ExtraLogging 恢复 off。

## 新实测：0f33f8f 仍未解决发色和形变

用户确认不再崩溃，但不是CTD永久消除证明。最新同次日志证明 sender139 的桶没有 skee132：旧skee Load wildcard时向量size133，后续139新桶未获订阅；完整重放应得 [24,35,39,45,66,91,95,140]。OBody95桶含132，所以它能取得接口，SOS不能；请求时机相同不能改变路由。

最小公开契约候选：只将我方模块basename换为 OutfitSystemNG.dll，内部PluginInfo.name仍SkyrimOutfitSystemNG，INI/存档标识不改。必须在实际Data/SKSE/Plugins最终VFS视图禁用旧SkyrimOutfitSystemNG.dll，不能双加载。文件名不保证枚举排序；仅当新日志证明我方handle H早于skee S且receiver skee delivered/map均true并取得v4，才说明路由已修。不得改名第三方、伪造sender或硬读内部map。

发色操作时序以用户新澄清为准：正确色为13,13,13；进入RM已错色；之后用户在临时色板预览56,59,44，再取消（不是确认），画面恢复正确13,13,13。日志没有色板打开/预览/取消的精确事件，不能把open快照中的56,59,44归因为初始化写色，也不能将未采到的瞬时错色等同该值。seq6/7出现RGB/128通路、九次其它slider事件未恢复仅为日志观察，不能覆盖用户实际操作说明。2/255与1/128是不同现有写路径，不能硬编码统一倍率。新只读采样增加独立headpart-hair来源，与illusion-slots明确分区，仍共用8根/512对象/64几何预算；不写颜色。真实头发缺失或共享材质时仍不能作为无歧义写入来源。

## 未完成

公开接口获取在旧版晚加载情况下仍待解决。已核对参考包 nioverride.psc 没有 AttachMesh/DetachMesh；别版上游的 AttachMesh 即使存在也仅证明首次 ApplyVertexDiff，不证明持续形变，不能拿它重构已验证挂载路径。公开 UpdateModelWeight 仅 ref 参数，不能指定现有 clone root；没有已验证第三方代理接口。

在此旧 DLL、真实 sender 不被订阅、禁止内部 patch/伪 sender 的条件下，不能用现有公开 API 保证取得任意 root 形变能力。若进一步独立实现 TRI/morph 计算，须先验证当前各 morph key 的公开读取、原顶点基准、TRI shape/顶点数匹配与渲染缓冲更新，不能直接复制新版 skee 顶点写入然后宣称安全。

实时幻化形变、RaceMenu 内发色、CTD 均未闭环。不得将路径计数、加载 TRI 或编译通过描述为功能修复成功。
