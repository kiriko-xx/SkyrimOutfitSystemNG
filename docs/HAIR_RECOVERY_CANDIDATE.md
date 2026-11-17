# 发色恢复候选设计（尚未验证）

## 验收范围

用户要求保留RM已有短暂闪色，只让幻化假发随后自动恢复，不用手动预览其他色再取消。实时形变27f4c48已验收，不改其算法/采样节奏；已测DLL备份build/accepted-27f4c48/OutfitSystemNG.dll，SHA256 a0a274c58408667907e56156eff86ef18609eade0497d3810fc2153a5f508478。

## 公开恢复链证据

本地RaceMenu公开脚本UpdateTints非骑马QueueNiNodeUpdate，OnNiNodeUpdate再InvalidateShaders；master TintMaskInterface::ReceiveEvent在NiNodeUpdate后VisitEquippedNodes着色，而VisitEquippedNodes先枚举真实穿戴护甲加skin，幻化非真实装备可能漏掉。旧用户v4既有REA证据只含BodyMorph遍历，不含TintMask NiNode事件反编译，故不能把master实现认定为旧DLL实证。

## 离线验证

Release构建成功，0 warning/error；形变路径/slider队列/API mock/通用观察者/33ms快路径/只读诊断回归通过。hair_recovery_mock.ps1原样提取整个生产namespace，25场景覆盖事件身份、上下文、缺失/歧义/非有限源色、共享源材质/几何、目标去重、Reset、各预算截断、cycle和仅1P目标；无事件不写、回调无game query。mock不验证真实NiNode事件时序、引擎ABI/并发或最终GPU画面，实机未验收。

## 候选约束

公开SKSE NiNodeUpdateEvent仅置原子身份；下一既有主线程任务消费，只在当前玩家、RaceSex Menu打开且有幻化覆盖时处理。不在事件回调读actor或排渲染写入，不用固定延时宣称完成。

以当前HeadPart HairTint实际渲染色为源，非ActorBase旧快照，不做RGB倍率换算，不钳制合法大于1值。来源缺失/多色不一致/预算截断则跳过。只重取当前覆盖槽节点的HairTint目标，不写源或与源共享的材质；局部遍历有界、按材质去重，不保存跨任务裸指针。无节点更新事件不写色，关闭菜单后不持续覆盖。

实现已接入既有主线程任务：先只读诊断，再HairTintRecovery::Drain，再原provider/morph处理。Install/Reset并行接入生命周期，形变算法与33ms节奏无改动。事件存借用reference地址仅作身份比较、不解引用；多个事件合并为最后身份，NPC事件可能覆盖玩家事件而保守漏恢复。全部来源/目标收集、预算检查及当前根材质重验证完成后才写；日志HairTintRecovery记录恢复数量或debug跳过原因。

实测顺序：保持同一幻化假发，直接开RM不动色板，观察是否像真实装备一样闪后自动恢复；再预览其他色并取消/确认，检查不抢预览；空幻化/真实装备不应由此模块写色。若无恢复，提供同次日志看是否有NiNode事件/无源/共享/上下文skip。不能仅凭restored日志宣称画面恢复。

该方案不是已证明旧RaceMenu恢复函数的复刻，而是补齐公开节点更新之后的幻化材质同步候选。源实际恢复是否先于事件、旧v4是否发出所需事件、预览色是否完整保留、共享目标是否有第三方染色语义，均需回归；事件一次消费无来源时可能不恢复，不能盲目轮询强刷补救。
