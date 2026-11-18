# SexLab 场景结束后的渲染崩溃审计（进行中）

## 同次日志，2026-11-17

- 用户报告运行 smooth-27f4c48；本次日志资源版本仍1.5.28.0，日志不能独立证明DLL字节哈希。
- `bug/SkyrimOutfitSystemNG.log:9912`，22:14:53.499422，线程23468切至 `_sex`；`:11608–11621`，22:15:36.263392–263545，线程5760直接 Armor Refresh 并恢复正常幻化。
- `:12152–12161`，22:15:42.675301–675360，线程5092完成 CustomOne；`:12162–12163`，22:15:42.703697–703724，线程5760对玩家00000014重放10节点，CPU3255us，不表示GPU完成。
- `bug/crash-2026-11-17-22-15-44.log:5–15`：线程5092，SkyrimSE.exe+12FFA5A，`mulss xmm2, [rbx+0x134]`，RBX=0x0002A4C7E7F58001，异常为EXCEPTION_ACCESS_VIOLATION；不是此前的non_rtti_object。
- `:64–78`：EffectShader SetupGeometry/RenderWorld链包含HorizonFix、CommunityShaders、EngineFixes等；当前直接栈不含SOS，不能据出现顺序归责这些插件，也不能据SOS不在栈排除先前破坏。
- `:161–192`：FXSteamSphereSkin:0 的UserData关联FF00704C机械球（Omega的热线化机械构装物），蒸汽贴图FXCloudRoundTileLite.dds/GradSteamThin.dds；不等于玩家幻化节点。插件日志没有FF00704C的重放记录。
- 本次DontVanilla选+0x302，不是另一用户旧RTTI现场+0x3A8；SKSE日志无ApparelPreview匹配，不能套用其调用链根因。BodyMorph v4交换成功。

## 可证明的源码边界

`SceneOutfits.cpp`原ProcessEvent直接StartScene/EndScene，普通全局set/string无串行保护，最终同步调用`refreshArmorFor` → SetEquipFlag/UpdateEquipment。SMP的后续排队不保护此前刷新。

不能把SKSE AddTask笼统称为固定主线程执行：SKSE2.0.20的 `BSTaskPool::ProcessTasks` 两处hook，队列锁仅保护pop，`cmd->Run()`在锁外；本次重放日志确有5092/5760/6460/8320/8360/23468多个执行线程。仅AddTask不是场景状态串行或渲染安全证明。来源：[SKSE Hooks_Threads.cpp](https://github.com/ianpatt/skse64/blob/v2.0.20/skse64/Hooks_Threads.cpp)。

开发中先用自有FIFO/单drain及状态串行保护修正场景路径，Reset使旧事件失效；以CommonLib公开Main::threadID与GetCurrentThreadId比对拒绝worker执行，待下次heartbeat重试，不在task pool内自旋。MorphRefresh的RequestBake与Tick任务入口也加相同门禁，worker不消费pending，不执行顶点/诊断/发色操作。新增一次性日志记录current/expected及accepted/deferred。不摘幻化、不补任意毫秒延迟、不patch第三方、不降低33ms设置；实际主线程任务被调度的频率仍须实机测量，不能预先保证仍30Hz。线程身份不是装备/渲染完成fence，仍不能因此声称修复这次渲染AV。

`BakeIllusion`与smooth基线相同：biped/clone是局部raw指针，selfWrite为atomic反馈标志而非锁；RequestBake任务与Tick任务都能DrainPending。必须核查本插件并发重放与节点引用寿命；即使局部NiPointer保活，也不保证与引擎拆装/渲染互斥。

## 离线回归（非引擎验证）

- `tests/scene_outfit_queue.ps1`：提取生产dispatch/queue/sink/Tick/Reset编译mock，9组通过；包含自有FIFO、Reset失效、输入心跳重试、worker/nullMain/ID0拒绝。
- `tests/morph_thread_gate.ps1`：提取生产线程判断，5例通过；两个任务入口静态门禁检查通过。
- 既有generic observer、slider9例、hair recovery25例、read-only/fast-path检查通过。没有证明真实Main::threadID值、引擎场景phase、任务在主线程执行频率或CTD消失。
- 不以局部raw指针改NiPointer伪称修复：取slot瞬间和引擎重建仍需同一同步域。RaceMenu自己的partition上传有renderer/skin锁，不给本插件遍历额外保证。

## d026448 实机失败：22:59:25 新现场

- 新 `bug/SkyrimOutfitSystemNG.log:78,883` 确认 accepted=27200/27200，worker24848被deferred。`:5292` 场景恢复、`:5296–5297` 13节点重放也均为27200；线程门禁生效但用户报告同样隐形/CTD，不能宣称线程修正已解决。
- 新 `bug/crash-2026-11-17-22-59-25.log:5–15` 为SkyrimSE.exe+131C6F6，`mov rdx, [rax+0x70]`，RAX=0x0002329422948000，线程27200。异常记录fault=FFFFFFFFFFFFFFFF与计算地址不符，不能据此伪造具体损坏成员。`:64–81` 是灯光/场景剔除到Main更新调用链，不同于前次EffectShader崩点。`:187–221` 栈扫描出现Omega skeleton；它不是已证明的坏对象。
- 插件`:5292` 22:59:21.894052恢复，`:5296` 21.896886显式clone重放（vertices未变），`:5299` 22.046799额外ApplyBodyMorphs；这是NotifyRebuilt旧延迟队列，在clone-only实时观察者之外仍会写整个actor。固定150ms不是装备完成屏障，重复全actor写入是否必要/是否触发CTD尚待隔离。
- 新 `bug/SLIFNG.log:26–44` 膨胀值从0.1566667到0.33999988，部分Apply位于21800/27292/27304/27300，并非27200；这是独立provider的线程证据，不是认定SLIF导致崩溃。SOS门禁不会串行化第三方写入。
- 不能把灯光插件出现在栈中、内存使用较高、或场景结束后约三秒崩溃当作因果证明。下一步审计SOS自身额外全actor写入与clone-only路径的重复性。

## 下一隔离改动：缩限重建后的形变范围

NotifyRebuilt保留原延迟/去重队列，但consumer改为RequestBake(kRebuilt)，不再ApplyBodyMorphs整个actor。即时NotifySlotApplied与通用33ms provider观察者不变，仍用同一ApplyVertexDiff克隆路径，基础钩子及发色算法不动。此改动只隔离本插件不必要的全actor额外写入，不消除第三方并发、不宣称提供节点生命周期屏障或已修CTD。新增morph_rebuilt_scope.ps1是源码静态范围守卫；既有generic/read-only/fast/hair25通过，仍需游戏验证。两名独立审计子代理均执行失败，未提供结论，不引用其审查背书。

## 内存清理后构建复核

用户清理内存后同一源码重试，BUILD_EXIT=0。新增重建范围静态守卫、Scene FIFO九组、线程predicate五例、generic连续变化mock与hair recovery二十五例全部通过。前两次C3859/C1076/1455为构建资源失败，未用旧DLL冒充。仍未取得下一候选的游戏内验证，CTD根因与修复效果未知。

## 162a398 实机：场景与形变等待输入

2026-11-18 10:31:11 新日志仍在 SkyrimSE.exe+131C6F6 光照／剔除路径崩溃（线程 21984）；第二次恢复见插件日志第 11296 行，第三次开始见第 14462 行，不能用一次崩得更晚证明修复。SLIFNG 日志记录了连续的 PregnancyBelly 变化，skee64 日志有水手服 TRI 载入记录；两者均不能证明幻化顶点即时更新。

场景任务落到工作线程被拒后，原来只依赖输入回调重新投递；形变轮询也依赖同一输入回调。新增独立 Win32 定时唤醒，仅投递现有队列，保留输入兜底、游戏线程检查与载入保护，不在计时线程刷新装备或改网格。保留 33 ms 形变节流及去重；场景 FIFO 投递异常后释放标记、保留待处理事件；新游戏初始化完成时恢复载入闸门。游戏线程任务池是否持续消费、切换配件可见时机和实际形变平滑度，仍需不按 Tab 复测。本改动不处理光照崩点本身。

## 0f2638e：另一台机器的 OBody 崩溃和 SAM 跟随回归

同次日志按报告内时间 11:17:32 对齐（文件名是 03-17-32）。SE 1.5.97，OBody 5.3.1.0；定时器已启动，BodyMorph v4 已取得，游戏线程 33220 接受任务。SOS 2431–2437 行证明菜单关闭时通用观察者至少在 OBody 预览阶段有检测及重放，不证明 SAM 每次拖动都被捕捉。1807 行的 0/5 TRI 与后续 2434 行的 6/6 属不同外观时点。

OBody 11:17:31.724 确认预设，SOS 11:17:31.820858 从 CustomOne 返回，随后在 SkyrimSE.exe+12BB9E6 `call [rax+0x38]` 崩溃。栈见 ShadowSceneNode 与玩家相关节点，但不同于前案 +131C6F6；日志未记录确认预设后 SOS 成功完成显式重放。不能据此归责 OBody、计时器或栈上其他插件。

发现独立的源码时序缺陷：旧 AbsorbOwnWrite 把重放之后读到的提供者值当成已处理值，可能标掉重放期间到达的新值。候选只把重放前签名交给 AbsorbOwnWrite，供下次轮询辨认新值；不改线程门禁、定时器、基础钩子、发色或第三方预设。离线测试不能证明 SAM 回归或 CTD 已修。

## 11-18 三组对照：RM 编辑后的 OBody 闪退与 SAM 回归

- 用户确认：先在 RaceMenu 拖身体滑块，再退出并用 SAM 调整 OBody 滑块、回主界面反复确认 OBody 预设，`a708434` 与 `681712c` 均崩；前者 SAM 实时，后者仍不实时。故 681712c 未修 SAM，“新定时器是崩溃必要条件”也不成立。
- 两份报告按正文时间 12:00:27 / 12:07:27 对齐。前者 `SkyrimSE.exe+131F009 mov rcx,[rax+0x70]`，后者 `+131DB06 call [rax+0x18]`；均经 `CommunityShaders.dll+0AF9939` 及游戏 `+12BE848/+12BA35B`，均在最后一次确认预设的同一秒。两组最终预设分别是 3BA-small2 和 3BA-自用-2.2-smallbra，不支持将其归因于单一预设。符号行不等于崩点代码或根因；没有实机 DLL 哈希与本地源码的配对。
- 不崩组也有 RaceMenu 打开、滑块事件和关闭（SOS 4814、5004–5027 行，11:53:20–32）；其 OBody 日志 11:52:50–11:53:13 已有七次预设确认，RM 退出后又有九次。两份崩溃组都是 RM 编辑先于该次记录中的第一笔 OBody 预设确认。不能把不崩组标作“整局完全没进过 RM”；后续应以同一初始存档、每臂重启并严格记录顺序。
- 三份 SKSE 日志均未检出 ShadowSceneNodeCrashFix.dll 或 PortalLightsRuntimePatcher.dll；不能混用之前另一环境的日志。先固定 a708434，只关闭 CS Light Limit Fix 的 Skip Redundant Light Updates（若该实机版本有此选项）做单变量 A/B。本地源码关闭后清跟踪状态并转原调用，不卸挂钩、不等于禁用 LLF。若仍崩，再用相同初始存档和完整操作对照无 SOS 的运行；实验过程不覆盖存档。

## 11-18 单变量对照：关闭灯光更新优化

用户报告仍使用 a708434，重复原本稳定闪退的 RaceMenu → SAM → OBody 操作，只关闭 Skip Redundant Light Updates 后未再崩溃。日志目录 `bug/hair-a708434-关闭Skip Redundant Light Updates/`：SOS 第 2198、2387–2394 行记录 RaceMenu 打开、拖滑块和退出；OBody 第 106–226 行随后记录 16 次预设应用，最后一次为先前崩溃组触发时使用的 3BA-自用-2.2-smallbra；SOS 第 2776–7092 行仍有多次六节点形变重放和顶点变化。CommunityShaders 日志第 75–77 行确认 Light Limit Fix 仍加载，日志没有直接打印该开关的值，开关状态来自用户报告。此结果验证了该机器、该操作序列的配置规避方案；不证明损坏对象的来源，不外推到另一机器的 +131C6F6 或 SAM／Tab 回归。

## SAM 跟随候选：同一游戏线程任务检测并重放

基于 681712c，通用观察者只将检测到的形变写入现有待处理队列，由本轮已通过游戏线程与载入检查的轮询在检测后直接消费。旧流程先消费再检测，检测出来的新值需要二次投递给任务池；如果这次投递落到工作线程，又要等下一轮合格轮询。此改动消除通用观察者的二次投递；保留 33 ms 节流、重放前形变签名、独立唤醒、异步事件回调门禁，以及克隆重放限流。构建与七套离线回归通过，实际 SAM 连续跟随仍待实机。本候选不修改灯光配置或修复照明崩溃。

### 菜单外形变取证补充

在 a708434 崩溃组，RaceMenu 已于 12:06:53.356 关闭，OBody 首次预览是 12:07:13.225；其中 12:07:09.172–10.960 有 34 条通用观察者请求，SOS 日志第 2397–2398 行记录工作线程 35452 的六节点重放与顶点变化。这段与用户描述的 SAM 操作相符，但日志未记录 SAM 事件，不能单独证实来源。681712c 组 11:58:58 的顶点变化发生在 RaceMenu 仍打开时，不能算 SAM 跟随的证据。两版几何证据日志约每 5 秒采样，缺失一条不能反推没有重放。b4e0206 仅消除已接受轮询后的二次任务投递，不能保证暂停菜单内轮询获得游戏线程消费机会。如果仍不实时，下一步分别计量轮询是否被接受、提供者签名是否变化、克隆是否实际完成重放，再决定如何改动；不据日志空窗放开工作线程写网格。

## b4e0206 实机失败与分段诊断

关闭 Skip Redundant Light Updates 后仍崩，SAM 仍不实时。13:00:41.104 确认 3BA-MY-2.2-nude，13:00:41.202941 最后一次 CustomOne 返回；崩点回到 SkyrimSE.exe+12BB9E6（call [rax+0x38]），与 0f2638e 的预设案一致，不是此前 CS 灯光更新分支的同一栈。关闭优化只在先前 a708434 对照中规避，不能外推。

诊断候选只增加一次每秒的会话累计计数（需 ExtraLogging）：投递、线程拒绝、就绪、轮询返回、玩家签名采样与变化、克隆重放返回和节点调用数。重放返回不代表实际移动或 GPU 完成；无日志和无变化须结合菜单操作时间点判读。未放宽线程门禁，也未改引擎或灯光路径。

## 0e0181b：轮询在 SAM 操作期间未进入游戏线程

原始日志 `bug/morph-trace-0e0181b/SkyrimOutfitSystemNG.log`：第2036–2067行，posted 980→3452、wrong-thread 560→3032（均增加2472），ready、poll-done、player-samples 均停在123，player-changes停在0。13:13:15.423833 恢复后检测到变化，约11.6ms后六节点重放报告顶点移动。这次卡在普通任务队列消费，不是缺少 v4 接口或目标网格不能重放。

后续探针使用 SKSE AddUITask，仅记录投递、回调返回、匹配已核实游戏线程的次数及线程ID；十次/秒上限、最多一个待执行探针，回调不访问角色、网格或引擎状态，不更换生产调度。依据 SKSE 2.0.20 Hooks_UI.cpp，界面队列与普通任务队列不同，消费期间持锁；即使探针证明可达，也不能据此视作装备或渲染完成屏障。

## b040aaf 实机：排除 AddUITask 作为 SAM 游戏线程消费入口

`bug/morph-ui-probe-b040aaf/SkyrimOutfitSystemNG.log`：2069→2118 行 UI 回调返回数 192→351（+159），匹配游戏线程次数保持 61；预期线程 28896，最后执行者为其他线程。2072→2119 行普通队列投递与错线程拒绝均增加 1298，玩家采样停在 139。13:26:51.154954 游戏线程检测到变值，13:26:51.167063 完成六节点重放并报告顶点变化。两队列在该测试窗口均未提供合格的游戏线程形变执行机会，不能把 UI 回调已返回当作主线程可达。

不把形变操作搬进 UI 委托、窗口消息回调或工作线程，也不靠提高投递频率掩盖问题。后续候选入口须同时验证 SAM 打开时仍运行、实际线程身份、装备/3D 重建之间的执行阶段；没有这些证据前不改基础钩子或交付猜测版。OBody +12BB9E6 单独处置。

## 回归归属：通用形变调度，不是 SAM 专项兼容

用户明确要求以通用形变机制为目标，SAM 只是复现用例。a708434 源码无 SAM 专用接入；其通用观察者每 33 ms 读取 BodyMorph 值签名，变化时通过 ApplyVertexDiff 原地重放克隆。a708434→d026448 首次在 Tick 和 RequestBake 的普通任务回调中引入 IsGameThread()，分别在签名采样和待办消费之前拒绝非游戏线程。a708434 的实机日志第 2395–2398 行显示线程 35452 执行过通用观察与六节点重放；后续两份探针显示测试期间游戏线程轮询停滞。定时唤醒与同轮消费没有改变入口的可达性。后续以 a708434 为行为对照，沿当前分支修正通用观察与安全消费的交接，不加 SAM／其他框架特判，不把简单撤销线程门禁当作安全修复。CTD 根因另查。

## 当前验证边界

本机没有该游戏运行现场；尚无证据证明着色器坏指针由哪个写入产生。不能把机械球/显存接近上限/TRI加载告警当作确定根因。当前开发主线另含未完成的Apparel Preview调用链修复，不可直接把dist开发DLL当本次可安装候选。
