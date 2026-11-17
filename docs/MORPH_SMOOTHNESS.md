# 幻化实时形变：27f4c48 已获用户验收

2026-11-17 19:15 用户确认过渡平滑，实时形变可以验收。验收范围为此前已测试的RaceMenu、ScreenArcherMenu、OBody NG链路及本候选平滑性；不外推为SLIF单独实测、所有AE运行时或发色问题已修。保留27f4c48二进制作为已测基线，不为标版本号而替换已验收DLL。

同次日志：SOS sender100通过skee取得BodyMorph v4；SKSE确认OutfitSystemNG.dll handle100、skee133；SOS第2013/2143/2300/2364/2375行明确重放后vertices moved。六节点低频CPU样本10989–13505us，含目标检查/证据哈希，不是逐帧分布或GPU完成时间。skee缺TRI的mgyma项仍保留装备边界，不影响对已测试有效TRI服装的验收。

以下为候选实现与历史测试说明：

用户确认46d0d48在ScreenArcherMenu和OBody NG均跟随，但相对裸体过渡有阶梯/轻微卡顿。最新日志连续generic observer间隔约255–272ms，与250ms采样限制吻合。SLIF仍未获实测确认。

本轮仅性能路径：轮询250→33ms（目标约30次/秒，实际受输入heartbeat与SKSE任务调度限制）；有公开BodyMorph接口时签名只用权威provider值，不再每次混入全角色几何哈希。Poll与Absorb复用一次读取，避免同一周期重复VisitMorphValues。fallback无接口时旧几何路径保留。

Bake内以当前NiAVObject指针集合去重，一个多槽clone一次只ApplyVertexDiff一次，1P/3P不同节点各自处理。发色只读诊断仍250ms节流且节流前不消费事件，未写颜色。重放仍只在morph值变化/既有事件触发，不伪造插值、不写预设、不改基础钩子。

已有5秒证据采样周期附clone replay CPU time微秒，含目标检查与证据哈希，不是GPU完成时间，也不是每次刷新分布。提高频率可能增加主线程负担，不能由离线编译承诺平滑度。若仍卡，需结合CPU时间、实际变化频率与ExtraLogging关闭对照。建议常规游戏ExtraLogging=off，仅短时采日志。

验收同套有TRI服装/同一滑块渐变速度下对照46d0d48与本候选，检查连续过渡、主线程卡顿、返回原值、OBody切预设；SLIF另测。继续仅用OutfitSystemNG.dll，禁旧名SkyrimOutfitSystemNG.dll。发色未解决且低优先级。
