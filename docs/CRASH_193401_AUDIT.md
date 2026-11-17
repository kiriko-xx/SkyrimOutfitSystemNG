# 另一用户 smooth 构建旧存档崩溃取证

## 证据与范围

用户确认 hair-a708434 发色测试无问题；本次四份日志来自另一位用户的 smooth-27f4c48，不能混作同一环境。旧存档使用过1.5.17，本次运行SE1.5.97/SKSE2.0.20。

- SOS日志98行已读入464套幻化、2个角色赋值，100行Post-load恢复钩子；最后262–263行线程22180进入DontVanillaSkinPlayer.ShouldOverride与ShouldOverrideSkinning，无退出。
- 崩溃报告是non-continuable C++ std::__non_rtti_object，信息Access violation - no RTTI data!，不是单纯读档挂起。栈上狗、护甲、PAD等对象只能作为上下文，不足归责第三方。
- 同次接口探测sender184、skee成功回写map、BodyMorph v4已取得；SKSE只加载一个OutfitSystemNG.dll，未发现同时加载旧名。RaceMenu尾部持续读TRI，不能用无接口解释此崩溃。

## 精确构建静态映射

分析已归档smooth DLL，SHA256 a0a274c58408667907e56156eff86ef18609eade0497d3810fc2153a5f508478。用户描述为此包，但日志本身没有文件SHA256，用户端字节身份尚不能独立证明。REA/Ghidra静态证据保存在build/crash-193401-smooth-evidence.json。

| 报告RVA | 本地归档二进制观察 |
| --- | --- |
| C592D | DontVanillaSkinPlayer::ShouldOverride调用ShouldOverrideSkinning后的返回点 |
| C5F0B | ShouldOverrideSkinning调用Actor转换helper后的返回点 |
| 89E3F | helper调用RTDynamicCast包装器后的返回点；不是C++ typeid |

helper89DD0内TESObjectREFR/Actor RTTI，包装器6E290用引擎RTDynamicCast relocation。因此已定位触发路径，但尚不证明具体输入为何不是有效RTTI对象。

## 站点契约风险

本次DontVanillaSkinPlayer扫描选中宿主+0x3A8（RVA3646F8），既往正常为+0x302（364652）。生成壳始终把r13当actor；仅callee相同不能证明调用点的活跃寄存器相同。原scanner只在entry+0x800逐字节扫E8，并无真实函数边界/指令流/寄存器来源证明。不能为消除异常改用As后继续任意移位。ShimWornFlags本次零命中也是环境差异证据，不据此猜测替代站点。

## 候选构建与验证

本轮Release编译BUILD_EXIT=0；DLL1,922,560字节，SHA256766f72c933272ced1586db8d48dfcaecdde68eb31bee6318e0590d5a86cc45e7，资源仍1.5.28.0。形变路径6例、slider9例、接口10例、发色生产namespace25例通过；严格站点21项为生产表达式转译/静态检查，身份4项仅布尔真值表，均不替代引擎验证。历史skse_listener_replay默认绑定旧139/132/95现场，与本次184/263不符而失败；新增-ModelOnly只跑独立协议模型，不篡改旧现场断言。

## 最小保护与限制

- 隐藏钩子只接受原契约偏移的预期直接call或已有可识别一跳；不扫到其它call就安装，不覆盖未知第三方E9/重定向。失败触发现有成对总开关，保留原装备；这可能让该环境暂时无幻化，而不是假装修好了兼容。
- ShouldOverrideSkinning在可读页/完整头部检查后，只接受Character标签和FormID注册表地址一致的对象，再做无RTTI的FormTraits转换。失败不隐藏真甲；一次原子诊断避免工作线程日志洪泛。不扩大注册表读锁范围。
- 注册表地址校验不pin对象，不能证明生命周期/并发安全，也挡不住r13恰为另一已注册角色；严格站点门禁才是首层防线。
- 不修改存档格式、不删旧存档、不清槽、不改已验收33ms形变与发色算法。安全降级不是该用户完整功能验收。后续必须同一旧存档验证，并看严格站点失败时的周边字节/已加载插件身份；必要时采集真实进程的调用点数据流。
