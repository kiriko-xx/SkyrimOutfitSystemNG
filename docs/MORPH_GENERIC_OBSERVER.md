# 菜单外形变重放候选

## 实测基线

用户确认加载顺序候选在RaceMenu内拖morph滑块已使幻化服装形变；发色仍失败但降低优先级。最新SOS日志第6–8行真实sender100向skee delivered/map均true并取得v4；同次SKSE我方handle100、skee133。第2379/2391/2790/2804行显式clone重放顶点移动。

菜单外第2406–2417行检测到morph变化，但调用ApplyBodyMorphs后几何不动，并因有TRI目标等待下一轮。旧v4此函数只遍历真实装备/skin的ARMA，不能代替ApplyVertexDiff(actor,clone,false)。

## 本候选

1. 既有主线程250ms轮询读取公开VisitMorphValues与OBody/SLIF签名；有g_api且权威providerSignature变化立即安排RequestBake，不再等连续变化停稳、旧quiet窗口到期。返回原值也触发；首次观察仅建基线。
2. 通用变化处理只要接口存在就走和RM事件相同的显式clone重放，无菜单名判断。RequestBake合并请求，15ms重放限流保留，AbsorbOwnWrite吸收自写几何噪声，ApplyVertexDiff不重写预设。
3. 不动Hook/Patches、身体槽、颜色。不摘穿幻化，不改第三方。依旧使用OutfitSystemNG.dll早加载名称，内部名不变，最终VFS仅留一份SOS。
4. tests/morph_generic_observer.ps1摘取生产分支，mock连续100次变化、值相同去重、返回原值、无接口与无菜单门禁；其通过不证明游戏调度或SLIF各模式均有效。

## 验收与边界

ScreenArcherMenu连续滑动/停止/返回原值；OBody换预设；SLIF渐变及回落，分别观察带有效TRI幻化的跟随。新增日志queueing explicit clone replay ... generic observer (no menu gate)，随后re-morphed ... vertices moved。250ms轮询为有界近实时而非逐帧回调，实际负载待测；SLIF纯骨骼缩放不是TRI morph，不承诺被本路径修复。缺TRI/shape或顶点不匹配仍不能变。发色未修复，本轮不写颜色。
