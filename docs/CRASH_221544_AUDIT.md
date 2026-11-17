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

## 当前验证边界

本机没有该游戏运行现场；尚无证据证明着色器坏指针由哪个写入产生。不能把机械球/显存接近上限/TRI加载告警当作确定根因。当前开发主线另含未完成的Apparel Preview调用链修复，不可直接把dist开发DLL当本次可安装候选。
