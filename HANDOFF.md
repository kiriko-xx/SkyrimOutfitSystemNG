# HANDOFF — SkyrimOutfitSystemNG（SOS-NG）交接文档

生成时间：2026-11-21 19:10（Asia/Shanghai）
交接原因：用户 m11335 原话「完全搞砸了，生成的预设简直是不按名称乱组，退回1.5.45，写下交接文档吧由下个agent接手」。
本文件是给下一位 agent 的入口：先读第 0、3、5、6 节，再动手。

---

## 0. 三十秒速览

| 项 | 值 |
| --- | --- |
| 工程 | `D:/Skyrim-dev/src/SkyrimOutfitSystemSE-NG`（git 仓库） |
| 当前分支 | `feature/slot-policy-esp-outfits-golden` |
| 当前 HEAD | `591e437` = tag **v1.5.45**（已按用户要求退回） |
| 发布线 main | `fad3e64` = tag **v1.5.34**（用户唯一验收过的发布版，场景套装切换） |
| 产物 | `dist/SKSE/Plugins/OutfitSystemNG.dll` 2,074,112 B / sha256 `075e211e59f1b6ad87b949eb8747bdcfeff73a231cf900ecf089625f34d4928f` / FileVersion 1.5.45.0 |
| dist | 31 文件 / 3,093,035 B，`dist/SHA256SUMS.txt` 30 条全通过 |
| 成品包 | `SkyrimOutfitSystemNG-1.5.45.zip` 1,341,328 B / sha256 `5c699ce05b6cbb4d3e5c99a83da5453b02ded651b38e248e78a41bbf85d782de` |
| MO2 测试包 | `SkyrimOutfitSystemNG-1.5.45-test.zip` 932,019 B / sha256 `0bed4f3ecb18f83451386cc664f7d883aa500e2430cff7bf16682c532e805f11`（2 条目：DLL + 说明） |
| 1.5.46 | **已废弃**：tag `v1.5.46` = `c7202b4`，另加语义 tag `abandoned/esp-outfits-trie-1.5.46`；其代码「按名字分组」被用户否掉，不要沿用 |
| 本机环境 | **没有 Skyrim**（`D:/Games` 不存在），所有游戏内行为都靠用户实机日志验证；日志在 `D:/Skyrim-dev/bug/` |

---

## 1. 这个项目是什么

- 把上游 Skyrim Outfit System（原仓库 `D:/Skyrim-dev/src/SkyrimOutfitSystemSE`）迁移到 **CommonLibSSE-NG**，单个 DLL 同时支持 SE（1.5.97）与 AE（1.6.x）。
- 磁盘文件名 `OutfitSystemNG.dll`，但插件内部名 / 日志名 `SkyrimOutfitSystemNG.log` / 翻译文件前缀 / INI 标识**一律不变**；**旧名 `SkyrimOutfitSystemNG.dll` 必须从 Data 删除**，两个同时在会双加载。
- 核心机制：用 SKSE 注入钩子，在引擎蒙皮流程里把「幻化套装」的护甲件当作视觉克隆贴到角色身上，真装备的可见性由另一个钩子隐藏。
- 用户是中文母语、非专业 modder 的测试者会发日志；用户本人会主动改 ini、连续切换预设做压力测试。

### 目录职责

| 路径 | 内容 |
| --- | --- |
| `src/cpp/hooking/` | `Hooks.cpp`（注入壳与站点安装）、`HookSupport.cpp`（地址库 + E8 扫描/自定位）、`Patches.cpp`（贴件、ShouldOverrideSkinning、槽位状态） |
| `src/cpp/MorphRefresh.cpp` | 实时形变（BodyMorphs / .tri 跟随）全部逻辑，约 100 KB，最复杂的文件 |
| `src/cpp/MenuUI.cpp` | SMF 菜单全部页面（幻化 / 金件 / 设置），约 87 KB |
| `src/cpp/SceneOutfits.cpp` | OStim / SexLab 场景 `_sex` 套装自动切换（1.5.34 起） |
| `src/cpp/AutoSwitch.cpp` | 按地点/状态自动切换（1.5.29–1.5.34 一行未改，可作为「切换怎么写」的参考实现） |
| `src/core/` | `OutfitService`（套装存储）、`PluginScan`（插件/护甲索引）、`EspOutfits`（从 ESP 生成套装，**本次出问题的模块**）、`GoldenItems`（金件 JSON 库）、`LegacySave`（存档兼容）、`Policy`（槽位策略） |
| `docs/` | 少量内部审计文档（`RACEMENU_V4_AUDIT.md` 是 DLL 改名的依据） |
| `dist/docs/` | 发给用户的 10 篇正式文档：ARCHITECTURE / BUILD / CHANGELOG / DELIVERY / HOOKS / I18N / MCM_PARITY / PAPYRUS_API / TESTING / THIRD_PARTY。**改动前必须先读，用户明确要求过** |
| `tests/*.ps1` | 23 个守卫脚本（源码字面量/结构断言，不做游戏内测试） |
| `bug/` | 用户发来的日志（崩溃 CrashLogger、SOS 日志、skee64、skse64、SLIF、PAD、OBody） |

---

## 2. 版本台账（回退靠 git tag，不要靠改代码猜）

| tag | commit | 内容 | 状态 |
| --- | --- | --- | --- |
| `rollback-1.5.17` | `4b2339a` | 用户实测「全功能正常，只有 BodyMorph 实时形变不跟随」的黄金基线 | 可回退点 |
| `v1.5.22-archive` | `3c0b118` | 1.5.20–1.5.22 钩子校验过强导致幻化失效的实验线 | 归档 |
| `v1.5.23` – `v1.5.29` | 见 `git log` | 恢复贴件站点、ActorWeightModel 契约、禁清槽、.tri 贴 BODYTRI、残留克隆窄清 | 历史 |
| `v1.5.30` – `v1.5.33` | — | 场景套装「补跑 pass / 重建 3D」路线，被用户连续否决 | 历史，勿复用 |
| `v1.5.34` | `fad3e64` | 场景事件一次 AddTask 交接 —— **用户 m08008 验收通过**；= main 当前发布版 | 已验收 |
| `abandoned/menu-morph-live-1.5.38` | `58c7199` | 1.5.35–1.5.38 实时形变分支（SAM 拖滑条跟随 + OBody 连切 26 次不崩，但 SLIF 清除膨胀时零采样），用户当测试分支废弃 | 封存，可复用点见第 11 节 |
| `v1.5.39` | `fbcba9b` | 菜单三件 + 服装过滤 | 作废（ESP 列表空 + 金件页闪退） |
| `v1.5.40` – `v1.5.44` | — | 修 1.5.39 两处回归、启动闪退（自死锁）、删除套装 CTD（size_t 下溢）、金件页三处、组行 ImGui id | 历史，前两项已由用户确认修好 |
| `v1.5.45` | `591e437` | 金件「按 ESP 添加单件」两级列表 + ESP 生成套装「按服装名分组」 | **当前**；金件部分正常，**分组部分有已知风险（见第 5 节）** |
| `v1.5.46` / `abandoned/esp-outfits-trie-1.5.46` | `c7202b4` | 分组改为 Trie + 字符级公共前缀 + 槽位约束组合 | **用户否决：生成的预设乱组** |

`VERSIONS.md`（仓库根）是 zip 台账：每版的包大小 / sha256 / 源码 tag / 状态 / 回退与合并规则。

---

## 3. 用户核心要求（硬约束，违反会被退回重做）

1. **迁移/升级类需求在原仓库上演进**，不要另起炉灶重写（用户 m03417 原话教训：v1 重写版最后整体废弃）。
2. **单 DLL 支持 SE + AE**：换 CommonLibSSE-NG 基座，`REL::RelocationID(seID, aeID)` 单表，绝不搞两套平行钩子实现或两套 ID 表。
3. **长任务自己拍板**：不要为可自主决定的技术方案反复征询；直接改代码 → 编译 → 出交付物 → 一次性中文汇报（用户 m04985 原话）。
4. **已验收功能不许回归**：①1.5.25 起基础幻化（含空幻化）用户实测正常；②1.5.34 场景 `_sex` 套装切换用户验收；③1.5.41 启动不闪退、1.5.42 删除套装不崩、1.5.43/1.5.44 金件页可用。
5. **第三方 mod 兼容只走公开契约**（Papyrus / 原生 API / 状态），不许 hook 别人的函数入口或内存结构；找不到就降级并记日志，绝不改第三方 ini。
6. **幻化件必须跟随 RaceMenu BodyMorphs / OBody / SLIF**：不许露原版装备中间态，不许要求用户重穿或先关幻化；判据要覆盖所有写路径，走通用信号不靠单一 mod 的离散事件。
7. **UI 口径**：术语用「幻化」不用「套装」；槽位文案的号 = 内部 BIPED_OBJECT 索引 + 30（头部=槽位 30）；长按要灵敏（首延迟 0.22 s / 间隔 0.030 s）；窗口几何必须存 ini 且改 ini 直接生效。
8. **菜单编辑纪律**：带导出/导入按钮的编辑器**只有 Export 写盘**，内存改动要常显 `[not saved]` 提示；ImGui 控件 id 必须跨帧稳定；逐槽策略展开状态按套装独立且不进 JSON。
9. **结论必须有证据**：不许把压缩摘要里的转述当用户原话（我犯过，用户 m10183 否认）；不许在没有帧级证据时写「防崩」逻辑。

---

## 4. 已验收 / 未验收

**已由用户亲口验收**
- 基础幻化显示（含空幻化套装）：1.5.25。
- 场景套装自动切换（OStim / SexLab `_sex`，不需要按 Tab/Z 弹窗）：1.5.34，m08008。
- 启动不闪退：1.5.41 由 m10183 确认；删除第 2 页及以后套装不崩：1.5.42 由 m10387 首句确认。
- 金件页可用（组行点得中、手动导出）：1.5.43/1.5.44 之后用户在 m10387 / m10869 语境下继续使用；1.5.45 的金件两级列表尚未单独确认。

**未验收 / 有已知问题**
- **ESP 生成套装的分组正确性**（本次交接的主因）：1.5.45 的启发式与 1.5.46 的 Trie 都被用户判为「乱组」。
- BodyMorphs 实时形变（拖滑条实时跟随）：1.5.38 曾验收（SAM 拖滑条 + OBody 连切 26 次不崩），但整套分支已被用户废弃；SLIF 清除膨胀场景被证明**零采样**（引擎不改装备时轮询根本不跑）。
- SLIF 的骨骼节点缩放通道（kKeys / Fill Her Up 的节点 scale）与幻化克隆不连通，只做顶点级 .tri 才会跟随。
- 场景中约 18 秒无蒙皮 pass（OStim 场景内 18 s 内没有任何 Custom 贴件）未修。
- AE 1.6.x 从未实机验收（只有 SE 1.5.97 的日志）。
- 第一人称单独配 `.tri` 未覆盖。
- 两起 CTD 因果未实证：OStim 结束回普通幻化闪退（崩在 `SkyrimSE.exe+12BACD7` 渲染期）、另一位用户删除膨胀时崩在 `+0D419DD` 且栈上是 ImmersiveEquipmentDisplays —— 都按瞬态处理，**不许据此写防崩逻辑**。

---

## 5. 这次的失败：为什么 1.5.46 被否（必读）

用户原话（m11335）：「完全搞砸了，生成的预设简直是不按名称乱组，退回1.5.45」。

两个版本分别做了什么：

- **1.5.45**（`src/core/EspOutfits.cpp`，15,519 B）：断词 → 剔除 `[]`/`()` 标签词 → 取「≥2 件共享的最长前缀（≤4 词）」当组名 → 组内按槽位冲突拆变体 → 首变体补齐槽位。
- **1.5.46**（同文件重写，27,098 B）：字符级前缀树 + 分支成组判定 + 槽内剥共同前缀取差分标识 + 按同标识跨槽组合；无法唯一匹配的整套不生成。

两者共同的根问题（下一版必须先解决这个，而不是继续换算法）：

1. **判据只有「名字像」，没有验证同槽候选真的互为替代**。两件都叫「上衣」的东西可能一件是给 A 体型、一件是另一个作者的另一套；名字相似 ≠ 同一套服装的替代件。
2. **我们从未见过用户真实的 ESP 名字样本**。作者的命名风格（前置 tag、中英混排、`[]`/`()`、版本后缀、槽位缩写）决定了任何启发式的成败，而这些只能从用户数据里看到。全部设计都停在猜测上。
3. **失败代价太高**：一次点击就往套装列表灌进几十个错套装，用户要手工删，体验直接崩溃。

---

## 6. 建议的下一步（按优先级）

### A. 先取证，不要先改算法
拿一份真实数据再设计：让用户指定**一个出问题的 ESP 名**，然后我们用一份**只加日志的诊断构建**（不改行为，无版本号变更、不打 tag、只出测试 DLL）把该 ESP 里每件护甲的 `GetFormEditorID()` / `GetFullName()` / 槽位掩码（CK 槽号 = bit + 30）/ FormID 逐条打出来，请用户把那段日志发回。
理由：分组判据是否正确只能对着真实名字验证；1.5.45 和 1.5.46 都是凭空设计的。

### B. 判据改造方向：先槽位互补，再名字
同一套服装的件通常**恰好覆盖互补槽位**（身体/双手/双脚/头/发/颈/耳/项链…），而替代款式**集中在同一个槽位**。所以更稳的顺序是：
1. 先按「槽位互斥」做一次最大匹配，得到若干互不冲突的候选组合（这一步完全不用名字）；
2. 只有必须多候选的槽（典型是身体/胸），才用名字差异去配对；
3. 名字只作为**第二判据、且只在同槽内比较**（1.5.46 的 CommonEnd 方向对，但把它当主判据就错了）。

### C. 保持「宁缺勿错」
无法唯一确定时**只生成最保守的一级套装**（每槽取第一件），并在结果消息里写明 `N piece(s) were left out because they cannot be matched`，不要把猜出来的组合塞进列表。

### D. 给用户可控性
生成前给一个「预览 / 只生成不收藏（或只收藏不生成）」的开关；或生成到独立前缀（例如 `[ESP] `）便于一次识别与批量删除。这是降低乱组破坏的最便宜手段。

### E. 之后再回到实时形变
按 tag `abandoned/menu-morph-live-1.5.38` 取回（`git checkout abandoned/menu-morph-live-1.5.38 -- src`，**注意会覆盖 tag 之后的独立修复，先 `git log <tag>..HEAD -- src` 列清单**），优先补「采样锚点」这条：引擎不改装备时轮询必须仍能采样（1.5.38 的采样只在主线程身份成立时做，bake 只挂在 worn pass 的 `SkinTick()` 上，所以 SLIF 清除膨胀的那 94 秒一次采样都没有）。

---

## 7. 构建 / 测试 / 打包 SOP（照抄）

```
cd D:/Skyrim-dev/src/SkyrimOutfitSystemSE-NG
cmd /c buildtask_msbuild.bat          :: 只用这个；裸跑 cmake 会改写 CMakeCache 并失败（找不到 VS 实例）
```
- 版本号两处：`src/cpp/include/version.h` 的 `_VERSION_PATCH`；`CMakeLists.txt` 的 `project(SkyrimOutfitSystemNG VERSION 1.5.xx)`。
- 构建成功判据：`BUILD_EXIT=0`、0 error 0 warning、日志末尾 `Deploying OutfitSystemNG.dll to dist/SKSE/Plugins`；把 `build/build_task.log` 复制成 `build/build_1_5_XX.log` 留档（**声称留档就必须真复制**）。
- 守卫测试：`C:/Program Files/PowerShell/7/pwsh.exe`（PS 5.1 会因 `String.Split(string)` 退化假失败），env 带 `PATHEXT=.COM;.EXE;.BAT;.CMD`。23 个脚本中 **2 个是既有失败、不是回归**：`hair_tint_read_only.ps1`（锚点随代码迁移消失）、`skse_listener_replay.ps1`（需要 `bug/skse64.log`）。
- 打包：`build/pack_1_5_XX.ps1`（全量 31 条目）与 `build/pack_1_5_XX_test.ps1`（MO2 测试包：只有 DLL + 说明 txt）。两者都用 `[IO.Compression.ZipFile]` + 正斜杠 `CreateEntry` + 逐条回读校验，**禁止 `Compress-Archive`**（会写反斜杠条目名 + 外层目录）。派生新脚本只改版本号与 `$dst`，**不要全文 split/join 替换版本号**（会改坏说明文本里的历史版本号）。
- `dist/SHA256SUMS.txt`：30 行、按 hash 字典序、不含自身、LF、逐条复核。两个 `.zip.sha256` 写「hash + 两空格 + 包名 + CRLF」。
- `dist/` 文档八处同步：`DELIVERY.md`（头部版本段 + 新 §4.xx 小节 + §5 编译/版本资源/关键字符串行 + §6 首测条目 + §7 限制 + §8 日志速查 + §9 红线）、`CHANGELOG.md`、`TESTING.md`（§3.xx 用例）、`ARCHITECTURE.md`（§8.7 历史条目）、`BUILD.md`（头部状态段）、`dist/README.md`（头部 + 版本列表）、`VERSIONS.md`、仓库根 `README.md`（当前版本句）。
- 发布顺序：version.h/CMake → 构建 → dist 文档定稿 → SHA256SUMS → 打包逐条回读 → 回填 VERSIONS → `git commit` + `git tag` → 核对 tag 指向。台账里**不要写自身 commit hash**（amend 会改哈希，形成死循环，只写 tag 名）。
- `.gitattributes` 有 `dist/** -text`；`dist/`、`build/`、`*.dll` 被忽略，所以文档/SHA256SUMS/打包脚本/zip 要 `git add -f`。
- 交付两种包：**全量包**（31 条目，用户正式安装用）与 **MO2 测试包**（2 条目，用户实机试新 DLL 用；按 m08008 协议：候选分支 + 不 tag + 不合 main + 不改 VERSIONS）。

---

## 8. 日志判据速查（用户发来的 `SkyrimOutfitSystemNG.log`）

| 想确认什么 | 关键行 |
| --- | --- |
| 钩子成对装好 | `Illusion display is active: the player skinning hooks are installed.`（若为 critical `Illusion display is off for this session` 则幻化整条关闭、真甲可见） |
| 贴件真的发生 | `Custom: attaching N outfit-only piece(s) of M displayed (mask 0x…, outfit revision …, body morph …)` |
| .tri 目标 | `carries a .tri morph target on N of M cloned slot(s)` |
| 实时形变在跑 | `MorphRefresh pipeline (session totals): posted=… on-thread=… poll-done=… player-samples=… player-changes=… cache-replays=…`；`re-morphed … the vertices moved`（`did not change` 表示几何未生效） |
| 场景套装切换 | `SceneOutfits: <事件> came from thread X (game thread Y); the switch is handed to the game thread.` + `… scene started; A -> B` + 同一毫秒的 `Armor Refresh for …` |
| 金件点选 | `Menu: golden piece <plugin>:<id> <name>`；组行 `Menu: golden group <名字> selected (N of M).` |
| ESP 生成套装 | `ESP outfits: …` 结果行 + 每款的 `costume` 明细（1.5.46 的文案，1.5.45 是 `name group(s)`） |
| 金件存库 | `Golden items: N piece(s) of <plugin> added.` / 若 `Message handler: an exception escaped` 说明又被异常撞过 |

游戏崩溃看 CrashLogger：`PROBABLE CALL STACK` 为空时读 `STACK` 段的 char* 槽（能看到我们自己的函数名、日志格式串、skee 返回地址）。没有 pdb 时可用 `.pdata` 的 `RUNTIME_FUNCTION`（start/end/unwind）把崩溃 RVA 对位到函数区间。

---

## 9. 坑清单（都是踩过的，别再踩）

1. **禁止清空 `RE::BipedAnim::objects[bit]` 的 item/addon/part 或调 `ClearBipedPart`**：那是引擎为该槽维护的唯一条目，清掉后装备栏不变引擎就不重贴 → 该槽永久空模型（1.5.24 事故）。离开幻化集的槽一律「交回引擎」；只有「我们上一套留下的克隆」在四条件同时成立时才可摘（`previousOwned & flag` && 该槽无真甲 && 皮肤不覆盖 && `partClone` 非空）。
2. **贴件必须用引擎传进来的 `ActorWeightModel`**（注入壳里 `mov rcx, rbx; mov rdx, rdi` 后 call），不许 `reinterpret_cast` 自造——`RE::ActorWeightModel` 在 CL-NG 里只有前向声明。
3. **成对钩子 fail-closed**：藏真甲（`DontVanillaSkinPlayer`）与贴幻化件（`CustomSkinPlayer`）必须同时装上，否则总开关关闭幻化并打 critical（只装一半 = 全裸且游戏不崩，比不装更糟）。
4. **钩子站点**：地址库只保证函数入口；`+0x81` 这类偏移是某版本的快照。旧版实测可用的站点不许因「E8 目标校验过强」被判死（1.5.20 事故）；反过来，0 命中 call 时**不许放大窗口或在全 .text 乱钩**，应先怀疑 ID 选错或被内联。
5. **线程身份只记录不执法**：SKSE `AddTask` 的回调在这台机器上经常不在主线程，`IsGameThread()` 当门禁会把整段时期丢空（1.5.35/1.5.36 事故）。每个一次性的「闩」都必须在任务不被执行时也能释放（用时间戳节流，不要 `exchange(true)` 后等某条路径来复位）。
6. **第三方接口只能是加速项**：skee 的 `InterfaceExchangeMessage` 广播在用户环境里从未送达 skee64（收件集固定不含 skee），所以任何功能都不能以「拿到 BodyMorph 接口」为前提；skee v5 才有 `AddMorphShapeCallback`（vtable 末项，必须 `GetVersion()>=5` 才能调）。
7. **i18n 译文是 printf 格式串**：en/zh 占位符必须同序（`I18n.cpp` 已有运行时护栏 + `tests/golden_menu_guards.ps1`），错序 = `%s` 吃 size_t = `strlen(0x8)` 崩溃。
8. **ImGui 控件 id 必须跨帧稳定**：`PushID(某每帧新拷贝里的字符串指针)` 会命中 `PushID(const void*)`，按下帧与松开帧 id 不同 → 点击被丢弃（1.5.44 事故）。列表行用行号/索引。
9. **两个无符号数之差不许直接喂 `assign`/`resize`/指针算术**：先改数据再算区间会下溢成 SIZE_MAX，抛 `std::length_error(vector too long)` 并掀掉整个菜单回调（1.5.42 事故）。
10. **FormID 宽度按插件类型**：light(ESL) 只存低 12 位（`GetFormID` 对 light 返回 `0xFE000000 + (lightIndex<<12) + formID`，不掩码），照 24 位存会二次叠加永远查不到（1.5.43 事故）。
11. **持锁代码不许调会自己上锁的公共 getter**：`std::mutex` 同线程重入在 MSVC 抛 `system_error(resource deadlock would occur)`，会以「启动即闪退」的形式暴露（1.5.41 事故）。用 `recursive_mutex` + `*Locked()` 内部版本。
12. `git checkout <tag> -- src` 会**整份覆盖**，把 tag 之后做的独立修复一起抹掉（7f5a2ce 的 BODYTRI 保留语义就是这么丢的）。先 `git log <tag>..HEAD -- src` 列清单再决定。
13. `git add -f a b c` 里只要有一个路径不存在，整条命令返回 128 而后面的 `git commit` 照常执行 → 文件漏进提交；事后 `git show --stat` 核对，缺了就补建 + `commit --amend` + `git tag -f`。
14. 大文档编辑（DELIVERY 已 22 万字符）：用字符串切片 + 回读断言，别指望一个 replace 命中一次；构造内容后**必须真的 `writeFileSync`**（漏写会让断言才发现）；`@B@` 之类占位符必须回填并 grep 校验（曾把 34 个字面占位符带进发布包）。
15. **不要在已被用户否决的机制上打补丁**：1.5.30 补跑 pass、1.5.31 `Update3DModel`、1.5.33 `OnGameThread` 门禁、1.5.35 猜 `Main::Update` 当帧泵、1.5.36 一次性闩、1.5.46 Trie 分组——都是「方向被否后继续加固」。用户说「按 X 那条路走」时，第一步是读 X 的既有实现照抄结构。

---

## 10. 仍未修的技术债（下一版可选）

1. 实时形变「引擎不改装备时零采样」：需要一条与 worn pass 解耦的采样锚点（采样通道与执行通道各自独立），线程身份只记录。
2. SLIF 的骨骼节点缩放通道（kKeys）不与幻化克隆连通；要做需把源骨骼 scale 映射到克隆内网格节点，属新增耦合，动前先问用户。
3. OStim 场景内约 18 秒无蒙皮 pass（场景开始到用户按 Tab 之间只有 1 轮 pass）。
4. AE 1.6.x 从未实机验收；第一人称单独 `.tri` 未覆盖。
5. 场景中切成 `_sex` 套装时身体/手脚槽可能空（1.5.29 后未再复现，若复现按「引擎 Skin 覆盖 3P slot 2/4/7 与幻化克隆争用」方向查）。

---

## 11. 本次回退做了什么（可复现）

```
cd D:/Skyrim-dev/src/SkyrimOutfitSystemSE-NG
git tag abandoned/esp-outfits-trie-1.5.46 v1.5.46    # 语义封存，代码留在 tag 里可取
git reset --hard v1.5.45                            # 源码/版本号/dist 文档/tests 回到 1.5.45
# 再把 1.5.45 成品包解回 dist（31 文件），保证 dist 与发行包逐字节一致：
#   Expand-Archive -LiteralPath SkyrimOutfitSystemNG-1.5.45.zip -DestinationPath <tmp> -Force
#   逐文件覆盖到 dist/
```

校验结果：dist 31 文件 / 3,093,035 B；`dist/SHA256SUMS.txt` 30/30 通过；`dist/SKSE/Plugins/OutfitSystemNG.dll` 2,074,112 B / `075e211e59f1b6ad87b949eb8747bdcfeff73a231cf900ecf089625f34d4928f` / 1.5.45.0；`dist/SKSE/Plugins` 下无旧名 DLL。

1.5.46 的算法代码仍可随时取回对照：`git show v1.5.46:src/core/EspOutfits.cpp`（**只作反面参考**：字符级前缀树本身没错，错在把「名字像」当成「同一套的替代件」）。

---

## 12. 沟通纪律（会影响交付是否被接受）

- 全部推理与汇报用**中文**，要直接、不要绕。
- 先读 `dist/docs/` 再动手；改动前先确认改的是哪个仓库、哪个模块（用户纠正过「我说的是 SexLab/OStim 的 `_sex`，你怎么改的是按地点/状态自动切换」）。
- 汇报结构固定：真因（带日志/源码证据）→ 改法 → 产物数字表（DLL 大小/哈希/FileVersion、包大小/哈希、commit/tag）→ 几步实机判定日志 → 边界与未验证项。**不许声称已实机验证**。
- 引用用户原话必须能对上消息编号；「用户已确认」只能来自用户本人那一轮。
- 用户会主动验证：手改 ini、连续切换预设、开关菜单对比。交付前假设他会往极端里测。

