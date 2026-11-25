# HANDOFF — SkyrimOutfitSystemNG（SOS-NG）交接文档

生成时间：2026-11-22 11:20（Asia/Shanghai），对应版本 **1.5.56**（已并入 `main`，tag `v1.5.56`）。
上一版交接写于 2026-11-21 19:10（1.5.45 回退那一刻）；本版把 ESP 生成算法定稿、1.5.54–1.5.56 的改动、i18n 全量落地与发布收尾一并写清。
本文件是给下一位 agent 的入口：先读第 0、3、4 节，再动手。

---

## 0. 三十秒速览

| 项 | 值 |
| --- | --- |
| 工程 | `D:/Skyrim-dev/src/SkyrimOutfitSystemNG`（git 仓库） |
| 发布线 | `main` = **v1.5.56**（本次一次性收尾 1.5.47–1.5.56 全部测试分支）；上一个发布 tag 是 `v1.5.34` |
| 产物 DLL | `dist/SKSE/Plugins/OutfitSystemNG.dll` 2,207,232 B / sha256 `42dcf428d3035e18852be3ba409f59f1980319e5d5fff7e14767b5501c4c2a47` / FileVersion 1.5.56.0 |
| dist | 31 文件 / 3,264,062 B，`dist/SHA256SUMS.txt` 30 条逐条通过 |
| 成品包 | `SkyrimOutfitSystemNG-1.5.56.zip` 1,413,907 B / sha256 `82203e35ebbb35c14ca9f23e09618ed58c189fd2323bc21dedfc82486f24194c`（31 条目、0 反斜杠条目名、解包逐文件哈希与 dist 全等） |
| 用户验收状态 | 1.5.55 由用户 m01923 首句确认「实测可通过」；1.5.56 的 Z 键 / 日志 / i18n 三项**尚未实机验证**，判据见第 4 节末 |
| 本机环境 | **没有 Skyrim**（`D:/Games` 不存在），游戏内行为只能靠用户实机日志验证；日志在 `D:/Skyrim-dev/bug/` |

---

## 1. 这个项目是什么

- 把上游 Skyrim Outfit System（原仓库 `D:/Skyrim-dev/src/SkyrimOutfitSystemSE`）迁移到 **CommonLibSSE-NG**，单个 DLL 同时支持 SE（1.5.97）与 AE（1.6.1170+）。
- 磁盘文件名 `OutfitSystemNG.dll`，但插件内部名 / 日志名 `SkyrimOutfitSystemNG.log` / 翻译文件前缀 / INI 标识**一律不变**；旧名 `SkyrimOutfitSystemNG.dll` 不许出现在包里。
- 核心机制：用 SKSE 注入钩子，在引擎蒙皮流程里把「幻化套装」的护甲件当作视觉克隆贴到角色身上，真装备的可见性由另一个钩子隐藏。
- 用户是中文母语、偏实操的测试者，会发日志、会手改 ini、会连续切换预设做压力测试。

### 目录职责

| 路径 | 内容 |
| --- | --- |
| `src/cpp/hooking/` | `Hooks.cpp`（注入壳与站点安装）、`HookSupport.cpp`（地址库 + E8 扫描/自定位）、`Patches.cpp`（贴件、ShouldOverrideSkinning、藏真甲） |
| `src/cpp/MorphRefresh.cpp` | 实时形变（BodyMorphs / .tri 跟随），约 100 KB，最复杂的文件 |
| `src/cpp/MenuUI.cpp` | SMF 菜单（幻化 / 金件 / 设置 / 快速选择窗口），约 100 KB；1.5.55 加了批量删除 |
| `src/cpp/MenuOutfitEditor.cpp` | 套装编辑器：逐槽策略、添加单件、1.5.55 新增「从 ESP 添加单个部件」 |
| `src/cpp/NGHotkeys.cpp` | 键盘热键（随机换装 / 快速选择 / 1.5.56 的 Z 键即时开窗 + 0.5 s 限流） |
| `src/cpp/I18n.cpp` | 内置词典 `kDictionary`（1.5.56 起 **341 条**）+ 翻译文件解析 / 写模板 |
| `src/cpp/SceneOutfits.cpp` | OStim / SexLab 场景 `_sex` 套装自动切换（1.5.34 起） |
| `src/core/` | `OutfitService`（套装存储）、`PluginScan`（插件/护甲索引）、`EspOutfits` + `EspOutfitMatch.h`（从 ESP 生成幻化）、`EspOutfitNames.h`（名称键与预设命名）、`SlotRules.h`（一槽一件的纯逻辑规则）、`GoldenItems`、`Settings` |
| `src/papyrus/` | 7 个 .psc（编译产物在 `dist/Scripts`，Papyrus API 见 `dist/docs/PAPYRUS_API.md`） |
| `dist/` | 发给用户的 31 个文件；`dist/docs/` 是 10 篇正式文档（ARCHITECTURE / BUILD / CHANGELOG / DELIVERY / HOOKS / I18N / MCM_PARITY / PAPYRUS_API / TESTING / THIRD_PARTY） |
| `tests/` | 守卫脚本（源码字面量 / 结构断言）+ `tests/esp_outfits_match.cpp` 与 `tests/slot_rules.cpp`（纯逻辑单测，用 cl 编） |
| `bug/` | 用户发来的日志（SOS / CrashLogger / skee64 / skse64 / SLIF / PAD / OBody） |

---

## 2. 版本台账（回退靠 git tag，不要靠改代码猜）

| tag / 版本 | 内容 | 状态 |
| --- | --- | --- |
| `rollback-1.5.17`、`v1.5.22`–`v1.5.33` | 钩子站点、ActorWeightModel 契约、禁清槽、.tri 贴 BODYTRI、残留克隆窄清、场景套装（1.5.30–1.5.33 的「补跑 pass / 重建 3D」路线被用户连续否决，勿复用） | 历史 |
| `v1.5.34` | 场景事件一次 AddTask 交接，用户 m08008 验收 | 已验收（上一个发布线） |
| `abandoned/menu-morph-live-1.5.38` | 1.5.35–1.5.38 实时形变分支，用户要求废弃 | 归档 |
| `v1.5.39`–`v1.5.45` | 菜单三件 + 金件页 + ESP 生成（1.5.45 的分组仍是启发式，被用户判「乱组」） | 历史 |
| `v1.5.46` / `abandoned/esp-outfits-trie-1.5.46` | Trie + 字符级公共前缀 | 反面参考，用户否决 |
| 1.5.47 – 1.5.53 | 开发线在 `test/esp-outfit-*` 分支链上：预览-确认、宽容分桶、占齐槽位、随机补位、待判断并入、全库离线复算、同名副本崩溃修复 | 已被 1.5.54 取代 |
| `v1.5.54` | 标签组整装优先（不再把一套拆成单件）、单槽不截断、96 上限部分收录 | **用户 m01585 验收：「ESP自动生成套装算法保持目前这样吧，可用。」算法定稿** |
| `v1.5.55` | 批量删除、编辑器「从 ESP 添加单个部件」、全路径一槽一件（SlotRules） | 用户 m01923 验收「实测可通过」 |
| `v1.5.56` | Z 键即时开窗 0.5 s 限流、日志瘦身、i18n 341 条全量落地 + 翻译文件解析逐字 + 守卫脚本 | **当前发布版**，待实机 |

`VERSIONS.md`（仓库根）是成品包台账：包大小 / sha256 / 源码 tag / 回退与合并规则 / 每次发布的固定顺序。

---

## 3. 用户核心要求（硬约束，违反会被退回重做）

1. **迁移/升级类需求在原仓库上演进**，不要另起炉灶重写（m03417 的教训：v1 重写版最后整体废弃）。
2. **单 DLL 支持 SE + AE**：CommonLibSSE-NG 基座 + `REL::RelocationID(seID, aeID)` 单表；绝不搞两套平行钩子实现或两套 ID 表。
3. **长任务自己拍板**：不要为可自主决定的技术方案反复征询；直接改代码 → 编译 → 出交付物 → 一次性中文汇报（m04985）。
4. **已验收功能不许回归**：基础幻化（含空幻化，1.5.25）、场景 `_sex` 套装切换（1.5.34）、启动不闪退（1.5.41）、删除套装不崩（1.5.42）、金件页可用（1.5.43/44）、ESP 生成算法（1.5.54）。
5. **第三方 mod 兼容只走公开契约**（Papyrus / 原生 API / 状态），不许 hook 别人的函数入口或内存结构；找不到就降级并记日志，绝不改第三方 ini / esp。
6. **幻化件必须跟随 RaceMenu BodyMorphs / OBody / SLIF**，不许露原版装备中间态，不许要求用户重穿或先关幻化。
7. **UI 口径**：术语用「幻化」不用「套装」；槽位号 = 内部 BIPED_OBJECT 索引 + 30（头部 = 槽位 30）；长按要灵敏（首延迟 0.22 s / 间隔 0.030 s）；窗口几何必须存 ini 且手改 ini 也生效；窗口标题不许出现 Debug。
8. **菜单编辑纪律**：带导出/导入按钮的编辑器**只有 Export 写盘**，内存改动常显 `[not saved]`；ImGui 控件 id 必须跨帧稳定；逐槽策略展开状态按套装独立且不进 JSON。
9. **一槽一件**（1.5.55 起是全路径硬规则）：任何入口往一套幻化里放件，一个身体槽位只能有一件；掉件必须进日志。
10. **结论必须有证据**：不许把压缩摘要里的转述当用户原话；不许在没有帧级证据时写「防崩」逻辑；不许声称已实机验证。

---

## 4. 已验收 / 未验收 / 1.5.56 判据

**已由用户亲口验收**
- 基础幻化显示（含空幻化）：1.5.25。
- 场景套装自动切换（OStim / SexLab `_sex`）：1.5.34，m08008。
- 启动不闪退：1.5.41；删除第 2 页及以后套装不崩：1.5.42。
- 金件页可用：1.5.43 / 1.5.44；1.5.45 的「按 ESP 添加单件」两级列表随 1.5.55 一并在 m01923 语境下使用。
- **ESP 自动生成幻化**：1.5.54 起定稿（m01585「保持目前这样吧，可用」）——不要再改算法。
- 批量删除 / 从 ESP 添加单件 / 一槽一件：1.5.55，m01923「实测可通过」。

**未验证 / 已知缺口**
- BodyMorphs 实时形变：1.5.38 曾验收，但分支被废弃；SLIF 清除膨胀时引擎不改装备 ⇒ 轮询零采样（未修）。
- SLIF 骨骼节点缩放通道（kKeys）不与幻化克隆连通；只做顶点级 .tri 才会跟随。
- OStim 场景内约 18 秒无蒙皮 pass；AE 1.6.x 从未实机验收；第一人称单独 .tri 未覆盖。
- ESP 生成的三处口径缺口（暂不修，用户已认可现状）：本地化插件缺 .STRINGS 时 490 件读不到名字、1199 件没有 FULL、只认前缀式共同词（后缀式如「…戒指」不成族）；全库 87 个插件因此零预设。
- 三个产品参数用户未拍板：kMaxVariants 32→64、2214 条同名同槽冗余记录是否在列表隐藏、是否支持后缀式共同词成族。

**1.5.56 实机判据（等用户回报）**
- 成功：①按 Z 立刻弹出快速选择（不再等施法动画），连按 0.5 s 内只开一次；②QuickSlotRandom = on 时按 Z 仍走随机换装、不开窗；③背包 / 地图 / 控制台打开时按 Z 不触发；④中文界面无英文残留（批量删除工具栏、ESP 预览、编辑器「从 ESP 添加单个部件」、设置页）；⑤未开 `[Debug] ExtraLogging` 时日志里每插件只剩一行 `ESP outfits: preview [...] pending=N (原因 计数…)`，没有逐件 `input [...]` / `candidate [...]` 行。
- 失败：日志出现新报错、窗口开两次、暂停菜单里被触发、界面出现英文、某条译文回落到英文。
---

## 5. ESP 自动生成幻化的最终形态（1.5.54 定稿，别再动算法）

代码：`src/core/EspOutfitMatch.h`（纯逻辑，可离线编译单测）、`src/core/EspOutfits.cpp`（读 PluginScan 的件、写预览/应用）、`src/core/EspOutfitNames.h`（ComparisonKey 与预设命名）。

- **分组**：候选族 = 槽位互补 + 名称公共前缀（trie 节点，ascii 至少 3 字符、非 ascii 至少 2 字符）的组；打分先比 aligned（对齐件数）再比 placed，宽前缀族不能靠补位压过窄前缀族。
- **分桶**：同槽多件按「同槽 LCP 差目标号」分类，互斥桶按 popcount 升序贪心占槽；撞槽只影响该件，不毁整族（1.5.53 起）。key 完全相同的记录视为可互换副本（Group::duplicates）——旧版在这里对空 shared 取 front()，曾让 12 个插件必崩（USSEP / Vigilant / Glenmoril / Unslaad / DAc0da 等）。
- **占齐槽位**：coverage 取族内争到槽的桶的并集，每个变体必须覆盖全部槽位；空槽从同款候选里**随机抽签**补一件（每个空槽只补一件，不叉乘），随机流可按种子复现。
- **待判断清零**（1.5.52）：剩下的件用「换装并入」——复制一份现成预设、换掉撞槽的那件；每件最多多一个预设，绝不指数膨胀。
- **标签组整装优先**（1.5.54）：`[URGF]`、`[Rabbit404]` 这类方括号标签组先按整组试算一套，只有拆开能摆下更多件且预设更少（StyleBetter）时才拆。方向写反就会把 96 件拆成 33 个单件预设（1.5.53 事故）。
- **上限**：只占单槽的款式（发型 / 鞋 / 眼镜包）上限 = 该族件数，其余 kMaxVariants = 32；预设总数 96 满了改为部分收录，其余记 `preset-limit`。
- **原因自解释**：进不了预设的件在菜单里逐件显示原因（reasonLabel 13 条，1.5.56 已全部走 i18n）。
- 复算工具（`D:/Skyrim-dev/build/` 下，不入库）：`esp_scan.mjs`（Node 解析 ESM/ESP/ESL → `build/esp-scan/scenario-all.txt`）、`esp_dbg`（单插件明细）、`esp_scan_run`（全库跑不变量）。**改 `src/core/*.h` 后必须重编这两个 exe**，否则拿旧算法得错误结论。

---

## 6. 1.5.55 / 1.5.56 新增机制速查

- **一槽一件**：`src/core/SlotRules.h`（BodySlotsOf / SlotsCollide / KeepOnePiecePerSlot，FX 位 bit31 不算身体槽）。全入口接线：OutfitService 的 create_outfit_preset / replace_outfit_armors / modify_outfit（insert_armor_slot_checked）、Papyrus AddArmorToOutfit、OutfitClipboard::Paste、OutfitSystem::OverwriteOutfit、编辑器 AddArmor、EspOutfits::ApplyPreview。掉件日志 `Slot rule: [..] preset=... dropped XXXXXXXX ...`。旧存档导入（LegacySave.cpp）故意不裁剪，保真优先。
- **批量删除**：MenuUI.cpp 的 g_bulkMode / g_bulkSelected / g_bulkConfirm，工具栏实时显示「已勾选 N 套幻化」，删除走 `OutfitService::delete_outfits()`（一次 ++_revision，同步清角色与状态的指派）。删除后必须把 total/begin/end 归零再刷新，否则重演 1.5.42 的 size_t 下溢 length_error。
- **从 ESP 添加单个部件**：MenuOutfitEditor.cpp 的 `##pieceplugins` / `##piecepieces` 子窗口，用 PluginScan::ArmorsOf 列件，已占槽标「[槽位已占用]」，可勾「只显示槽位空闲的」。
- **Z 键即时开窗**（1.5.56）：`[RandomOutfit] QuickPickerKey`（默认 Z，留空禁用）。NGHotkeys.cpp 的 FireQuickslotKey() 在按键当帧调用 Menu::ShowQuickPicker(true)，kQuickslotOpenCooldownMs = 500 限流；**不消费事件**，power 照常施法，它自己的 OpenQuickPicker() 发现窗口已开就空转；QuickSlotRandom = on 时按键直接 return（避免抽两次）；暂停类菜单里不触发。所谓「CD」其实就是 lesser power 的施法动画，不是任何计时器（全 src 与 ESP 记录里都没有 cooldown 字段）。
- **i18n**：词典 341 条；`dist/interface/translations/SkyrimOutfitSystemNG_{english,chinese}.txt` 各 341 行（UTF-16LE+BOM、CRLF）；`$英文<TAB>译文` 两侧**逐字取用**（12 个键以空格结尾、3 个以空格开头）；源码里不许再有裸 IG:: 可见字面量（控件类用 I18n::TrId，纯文本用 I18n::Tr）。

---

## 7. 构建 / 测试 / 打包 SOP（照抄）

```
cd D:/Skyrim-dev/src/SkyrimOutfitSystemNG
cmd /c D:/Skyrim-dev/build/build_1_5_56.bat      # MSBuild 直编；set 环境变量块必须照抄上一版（缺 SystemRoot/ProgramData/ALLUSERSPROFILE/PUBLIC/PATH/TEMP/TMP/USERPROFILE/APPDATA/VCPKG_ROOT 任一就 MSB4018 + FileTracker 崩）
```
- 版本号两处：`src/cpp/include/version.h` 的 SKYRIMOUTFITSYSTEMSE_VERSION_PATCH；`CMakeLists.txt` 的 `project(SkyrimOutfitSystemNG VERSION 1.5.xx)`。
- **改过 CMakeLists.txt 必须重配**：先 `call vcvars64.bat` 再 `cmake -S . -B build -DVCPKG_MANIFEST_INSTALL=OFF`（`build/cfg_1_5_56.bat`），否则报 could not find specified instance of Visual Studio；反之 manifest install 开着会因 vcpkg 内部异常卡死。
- 构建成功判据：BUILD_EXIT=0、0 error 0 warning；产物 `build/Release/OutfitSystemNG.dll` 含字符串 SKSEPlugin_Load / _Version / _Query 与版本号字符串。
- 守卫（pwsh 7，PS 5.1 会假失败）：`tests/i18n_translation_files.ps1`、`tests/golden_menu_guards.ps1`、`tests/esp_outfits_filter.ps1`、`tests/esp_outfits_names.ps1`（内含 esp_outfits_match 单测）、`tests/slot_uniqueness.ps1`、`tests/slot_rules.ps1`。
- 打包：`D:/Skyrim-dev/build/pack_dist_1556.ps1`（全量 31 条目，ZipFileExtensions::CreateEntryFromFile + 正斜杠条目名）与 `pack_1_5_55_test.ps1`（MO2 测试包：只有 DLL + 说明 txt）。`.zip.sha256` 内容为「哈希 + 两空格 + 包名」。
- `dist/SHA256SUMS.txt`：30 行、按路径排序、不含自身、LF、逐条复核。
- 发布顺序（VERSIONS.md 固定要求）：version.h / CMakeLists → 构建 → 收尾 dist → 重算 SHA256SUMS.txt → 打包并把 zip 大小 / 哈希写进 VERSIONS.md → git add -A + commit + tag。**不要在 commit 消息或 VERSIONS.md 里写自身提交哈希**。
- `.gitattributes` 有 `dist/** -text`；`.gitignore` 忽略 `dist/`、`build/`、`*.dll`、`*.pex`、`*.log`，所以文档 / SHA256SUMS / zip 要 `git add -f`。
- 交付两种包：**全量包**（31 条目，正式安装）与 **MO2 测试包**（2 条目，试新 DLL）；测试包协议＝候选分支 + 不 tag + 不合 main + 不改 VERSIONS。
---

## 8. 日志判据速查（用户发来的 SkyrimOutfitSystemNG.log）

| 想确认什么 | 关键行 |
| --- | --- |
| 日志级别 | 默认 info；`[Debug] ExtraLogging = on` 才是 trace（1.5.56 起未开时没有逐件 ESP 行） |
| 钩子成对装好 | `Illusion display is active: the player skinning hooks are installed.`；critical 的 `Illusion display is off ...` 表示成对校验失败 |
| 贴件真的发生 | `Custom: attaching N outfit-only piece(s) of M displayed (mask 0x..., outfit revision ..., body morph ...)` |
| .tri 目标 | `carries a .tri morph target on N of M cloned slot(s)` |
| 实时形变在跑 | `MorphRefresh pipeline (session totals): posted=... on-thread=... poll-done=...` |
| 场景套装切换 | `SceneOutfits: <事件> came from thread X (game thread Y); the switch is handed to the game thread.` |
| ESP 生成 | 每次预览一行 `ESP outfits: preview [<esp>] candidates=N pending=M filtered=K capped=... reason=<原因 计数,...>`；应用后 `confirmed [...] created={} skipped={}`。逐件行只在 ExtraLogging 下出现 |
| 一槽一件掉件 | `Slot rule: [..] preset=... dropped XXXXXXXX ...` |
| 批量删除 | `Outfits: bulk delete removed N preset(s)` |
| 金件点选 | `Menu: golden piece <plugin>:<id> <name>` / `Menu: golden group <名字> selected (N of M).` |
| Z 键 | 启动/重载时 info 一行：`Hotkeys: 'Z' opens the quick picker right away (the quickslot power needs its cast animation).`（按键被禁用则只有 debug）；真正开窗是 debug：`Hotkeys: quick picker opened by the quickslot key 'Z'.` |

---

## 9. 坑清单（都是踩过的，别再踩）

1. **禁止清空 `RE::BipedAnim::objects[bit]` 的 item/addon/part 或调 ClearBipedPart**（1.5.23 事故：该槽永久空模型）。
2. **贴件必须用引擎传进来的 ActorWeightModel**，不许自造（1.5.22 全裸）。
3. **成对钩子 fail-closed**：藏真甲与贴幻化件必须同时装上，只装一半 = 全裸。
4. **钩子站点**：地址库只保证函数入口，`+0x81` 这类偏移是版本快照；0 命中 call 时不许放大窗口或全 .text 乱钩。
5. **线程身份只记录不执法**：AddTask 回调常不在主线程，IsGameThread() 当门禁会把整段时期丢空。
6. **第三方接口只能是加速项**：skee 的 InterfaceExchangeMessage 在用户环境从未送达，任何功能不能以拿到 BodyMorph 接口为前提。
7. **i18n 译文是 printf 格式串**：en/zh 占位符必须同序同类型（1.5.39 曾因此崩）；golden_menu_guards.ps1 与 i18n_translation_files.ps1 都验这个。
8. **ImGui 控件 id 必须跨帧稳定**：PushID(每帧新拷贝的字符串指针) 会命中 PushID(const void*)，点击被丢弃（1.5.44）。
9. **两个无符号数之差不许直接喂 assign / resize**：下溢成 SIZE_MAX 抛 length_error，掀掉整个菜单回调（1.5.42）。
10. **FormID 宽度按插件类型**：light(ESL) 只存低 12 位。
11. **持锁代码不许调会自己上锁的公共 getter**：MSVC 抛 system_error(resource deadlock would occur)（1.5.40 启动闪退）。
12. **对空 shared 取 front() 就是崩溃源**（1.5.52 线上即有，1.5.53 修）；同名副本要先 dedupe 再判。
13. **标签组递归拆分方向**：StyleBetter(deeper, whole) 为真才拆，写反就把整套拆成单件。
14. **改 `src/core/*.h` 后必须重编离线复算 exe**（esp_dbg / esp_scan_run），否则用旧算法得错误结论。
15. **单测期望值必须先跑探针打印真实分组再写断言**，凭空写的断言必失败。
16. **git commit -F 的相对路径解析到仓库内**：消息文件放仓库外时必须写绝对路径。
17. **`git checkout <tag> -- src` 会整份覆盖**，把 tag 之后的独立修复一起抹掉。
18. **不要在已被用户否决的机制上打补丁**（1.5.30–1.5.33 的补跑 pass / 重建 3D / OnGameThread 门禁）。
19. **I18n.cpp 这类大文件别用行号 splice**：混合行尾会让插入块错位、整段吞掉函数（1.5.56 就把 ParseTranslation 写坏了）→ 从 `git show HEAD:路径` 取干净基线整体重放改动，再 `git diff -U2` 逐处核对。

---

## 10. 仍未修的技术债（下一版可选）

1. 实时形变「引擎不改装备时零采样」：需要一条与 worn pass 解耦的采样锚点；1.5.38 分支可参考（`git show abandoned/menu-morph-live-1.5.38:src/cpp/MorphRefresh.cpp`），但它会覆盖之后的独立修复，先 `git log` 对照再挑。
2. SLIF 骨骼节点缩放通道（kKeys）不与幻化克隆连通；要做需把源骨骼 scale 映射到克隆网格节点，动前先问用户。
3. OStim 场景内约 18 秒无蒙皮 pass；AE 1.6.x 未实机验收；第一人称单独 .tri 未覆盖。
4. ESP 生成的三处口径缺口（本地化 .STRINGS、无 FULL、后缀式共同词）与 87 个零预设插件——用户已认可现状，除非他要求否则别动。
5. 三个产品参数（kMaxVariants 32→64、同名同槽冗余记录是否隐藏、后缀式共同词）仍未拍板。

---

## 11. 沟通纪律（会影响交付是否被接受）

- 全部推理与汇报用**中文**，直接、不绕。
- 改前先确认改的是哪个仓库、哪个模块（用户纠正过「我说的是场景 `_sex` 套装，你怎么改的是按地点/状态自动切换」）。
- 汇报结构固定：真因（带日志 / 源码证据）→ 改法 → 产物数字表（DLL 大小 / 哈希 / FileVersion、包大小 / 哈希、分支与 tag）→ 几步实机判定 → 边界与未验证项。**不许声称已实机验证**。
- 引用用户原话必须对得上消息编号；「用户已确认」只能来自用户本人那一轮。
- 测试包与正式包的边界要守住：验证性修复先出测试包（只有 DLL），用户实机点头后才 tag / 合主线 / 动 dist。
- 用户会主动往极端里测（手改 ini、连切预设、开关菜单对比），交付前按这个标准自查。
