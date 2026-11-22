# 更新日志（CHANGELOG）

## [1.5.56] - 2026-11-22

### 改：Z 键立刻打开快速选择 + 日志瘦身 + 翻译文件全量落地（m01923 / m02062）

**用户原话**：「1.5.55实测可通过：现在检查i18n，将所有文本部分全部落实到interface\translations下的翻译txt。缩减未开启详细日志时的日志输出。完成后打完整包。」「还有一个把esp中z键的调出快速选择菜单的能力cd改成0.5s」。

- **Z 键即时开窗（0.5 s 限流）**：新增 `[RandomOutfit] QuickPickerKey`（默认 `Z`，留空即禁用）。按 Z 不再等「Change Outfit」这个 lesser power 的施法动画，按键当帧就开快速选择窗口；同一个键 0.5 s 内只开一次。按键事件**不消费**，power 照常施法，只是它的 `OpenQuickPicker()` 发现窗口已开就空转，所以不会开两次。`QuickSlotRandom = on` 时按键什么都不做（随机换装仍由 power 抽，避免抽两次）；暂停类菜单（背包/地图/控制台）里不触发。
- **日志瘦身**：默认 `info` 级。未开 `[Debug] ExtraLogging` 时，从 ESP 生成幻化的逐件/逐预设/candidate/unplaced 行、HairTint 的两条逐帧诊断全部降到 `debug`；保留的只有每次预览一行摘要（现在附带待判断原因直方图，例如 `pending=9 (same-slot-alternative 7, non-unique-difference 2)`）、应用结果一行、以及所有真实异常 `warn`。
- **i18n 全量落地**：内置词典 **341 条**，`dist/interface/translations/SkyrimOutfitSystemNG_english.txt` 与 `_chinese.txt` 各 341 行、与词典逐条对应。源码里 129 处直接传给 ImGui 的英文文案（批量删除工具栏、ESP 预览、编辑器「从 ESP 添加单个部件」、设置页等）全部改为 `I18n::Tr()` / `I18n::TrId()`，中文界面不再夹英文。
- **解析更严格也更保真**：`$英文<TAB>译文` 的两列现在**逐字取用**（不再裁掉首尾空格），因为词典里有 12 个以空格结尾的键（`Outfit: `、`Added ` …）和 3 个以空格开头的键；`$…=…` 与手写 `key=value` 仍两端去空格。
- **守卫测试**：新增 `tests/i18n_translation_files.ps1`（词典 ↔ 两份 txt 逐条对齐、占位符签名一致、UTF-16LE BOM + CRLF、源码里不许再出现没走 `I18n::` 的可见字面量）。

## [1.5.55] - 2026-11-22

### 新：批量删除 + 编辑器「从 ESP 添加单个部件」+ 全路径一槽一件（m01600）

- **批量删除**：幻化列表搜索行右侧加「批量删除」开关，行前出现勾选框，工具栏实时显示「已勾选 N 套幻化」，支持本页全选 / 本页取消 / 全部筛选结果 / 清空勾选；确认后一次性删除并同步清理角色与状态指派。
- **从 ESP 添加单个部件**：套装修辑器「添加护甲」区新增折叠项，可按插件搜索、列出该 ESP 的全部槽位护甲，已在身上的件标「[槽位已占用]」，可勾「只显示槽位空闲的」。
- **一槽一件（全路径强制）**：新增纯逻辑头 `src/core/SlotRules.h`，`create / replace / modify`、Papyrus API、剪贴板粘贴、编辑器添加、ESP 预览应用所有入口统一走 `KeepSlotUnique()`：一个身体槽位只留第一件，掉件写 `Slot rule:` 日志。FX 位（bit 31）不算身体槽位，不再误判撞槽。

## [1.5.54] - 2026-11-22

### 修：同一标签的整套件不再被拆成单件预设（m01350）

**用户原话**：「你看看这回生成的什么玩意，有一件完整的预设吗！」（1.5.53 日志：[SSE] Personal Pack vol.2 的 56 个候选里 33 个是单件）

- 方括号标签组的分组改为「整装优先」：先按整组试算一套，只有当拆开能摆下**更多件**（且预设更少）时才拆。
- 只占一个槽位的款式（发型包/鞋包/眼镜包）不再被 32 个变体上限截断，上限改为「该族件数」。
- 预设总数达到 96 时不再整族丢弃，而是收录到满为止，其余逐件记 `preset-limit`。

## [1.5.47 – 1.5.53] - 2026-11-22

### 改：从 ESP 自动生成幻化——算法定型（多轮实机与离线复算迭代）

- 分组从「按名字前缀硬匹配」改为「按槽位互补 + 名称公共前缀」的宽容分桶：撞槽只影响那一件、不再毁掉整族；同名副本不会崩（1.5.52 的 `nullptr` 崩溃修复）；单槽款式也能成族、一件也能成套。
- 生成的预设必须**占齐该款式的所有槽位**；空槽从同款候选里随机抽一件补（每个空槽只补一件，不叉乘）。
- 待判断的件用「换装并入」消化：每件复制一份现成预设并换上它，每件最多多出一个预设，绝不指数膨胀；实在进不去就在菜单里逐件显示原因（「与其他件占用同一槽位」「同槽多件无法区分」…）。
- 全库离线复算（445 个插件、12,395 件可幻化护甲）：1.5.53 起待判断从 3,276 降到 1,780，不变量违规 0。用户 1.5.54 验收：「上轮问题实测已修复。ESP自动生成套装算法保持目前这样吧，可用。」
## [1.5.45] - 2026-11-21

### 改：金件「按 ESP 添加单件」+ 从 ESP 生成套装按服装名分组（m10869）

**用户原话**：「“金件””按FormID添加单件“改成”按ESP添加单件“，点击后选择该单件所属ESP时，再弹出该esp内所有符合条件的金件装备菜单，点击即可添加单件。同理在点击左侧金件列表中金件esp预设时，右侧的添加单件也改成类似操作。 “幻化”中的从ESP自动生成套装的规则还需优化，兼容同一个esp中包含多种服装的情况，可以通过服装名称来判断，一般同一套服装前面的几个字段都是相同的。」

- **金件页（两处入口换成列表）**：删掉 `ParseLocalId()` 与 `InputTextWithHint("##goldenlocalid", "000800", …)`（手输本地 FormID 这条路 1.5.39–1.5.44 一直在产生 `was not found in this load order`）。新增 `RenderGoldenPieceList(plugin, added, scopeId, height, filter, size)`：列出某个 ESP 的全部槽位护甲（`PluginScan::ArmorsOf()`），带搜索框、`[已加入]` 标记、`[槽位 30/32/…]`，点一行即 `Golden::AddLocal()`（只置 `g_goldenDirty`，不写盘）。入口一 = `Add one piece from a plugin... / 按 ESP 添加单件...`（选 ESP → 展开该 ESP 的装备）；入口二 = 选中金件组后右侧编辑器底部的同一列表（只含该组所属 ESP）。两个列表用 `PushID(a_scopeId)` 分栈、行 id 用行号。
- **生成套装按名字分组**：`EspOutfits::GenerateFromPlugin()` 先 `SplitWords()` / `NameWords()` 切词（忽略 `[]` / `()` 标签词、中文不切词、最多比 4 个词），再取「至少两件共享的最长前缀」当服装组名，无共享前缀的落「其余」组；组内仍用 `BuildVariants()` 按槽位冲突拆变体（上限 32，第 n 个变体用首变体补齐）。只有一套服装时命名与以前完全一致（`<ESP名称>` / `<ESP名称>_1`），多套时 `<ESP名称> <服装名>`。总上限 96 套、超了打 `truncated`。每套仍是 `XXXO`（需要已装备才显示幻化）。
- **I18n**：新增/改写 11 条中英文案（`Add one piece from a plugin...`、`Pick a piece of that plugin below.`、`Close the piece list`、`Search pieces...`、`%zu slot armour record(s) in this plugin; click a piece to add it.`、`No piece of this plugin matches that search.`、`[in the list] %s  %s` 等），中英占位符同序（1.5.40 的护栏会在顺序不一致时回退英文）。
- **守卫测试**：新增 `tests/golden_piece_picker.ps1`（两处入口都走列表、无手输 FormID、`SaveToDisk` 只在导出路径 1 次）与 `tests/esp_outfits_names.ps1`（切词/共享前缀 ≥2/变体上限/`XXXO`/消息含 `name group(s)`）。全套 23 个脚本：21 通过，2 个既有环境失败（`hair_tint_read_only.ps1` 锚点随代码迁移消失、`skse_listener_replay.ps1` 需现场 `bug/skse64.log`），非本版回归。

### 未改动

- 引擎侧一行未改：注入壳 / 贴件语义 / 槽位清理 / `.tri` 推导 / 场景套装切换 / 钩子安装 / 金件存库锁。1.5.25 的基础幻化、1.5.34 的场景切换、1.5.42 的删除崩溃、1.5.44 的组行点击都不受影响。
- 金件 JSON 结构未变（旧档照读）；金件仍只有「导出」写盘。

### 未验证

- 本机没有游戏。分组口径（最长共享前缀 ≥ 2 件）是从「同一套服装前面几个字段相同」推出的启发式，未在真实 ESP 上跑过；两种已知偏差（两套前缀相同被并组、某件第一个词不同掉进「其余」组）只影响命名与分桶。判据是日志里的 `K name group(s)` 与自己数出的套数是否一致。

## [1.5.44] - 2026-11-21

### 修：金件列表依旧点不中——控件 id 跨帧不稳定（m10689）

- **用户原话**：「“金件”列表依旧点不中，“添加某个esp的全部装备”后可以自动焦点中。」
- **症状**：金件栏左侧的插件组行怎么点都没反应；但用「添加某个 ESP 的全部装备」加进来之后，那个组会自己变成选中状态（右侧编辑区随之出现）。
- **真因**：`MenuUI::RenderGolden()` 每帧 `const auto groups = Golden::Groups();`（`std::vector<Group> Groups();` 是**返回值**，每帧一份新副本），组行却 `IG::PushID(group.plugin.c_str())` —— `const char*` 命中 `PushID(const void*)` 重载，指针每帧都变。ImGui 的点击判定要求「按下」与「松开」落在同一个控件 id 上（`ActiveId` 比对），id 一变这次点击就被丢弃。**对照组**：同页插件选择器推 `PushID(it->order)`（整数）所以能点；「添加全部装备」是代码直接给 `g_goldenSelected` 赋值，不经过点击判定。1.5.43 只补了显式宽高，没碰到 id，所以问题原样留下。
- **修法一**（`MenuUI::RenderGolden()`）：组行改用行号做 id —— `std::size_t rowIndex = 0;` + `IG::PushID(static_cast<int>(rowIndex));` + 循环尾 `++rowIndex;`（`Groups()` 按插件名排序，行号跨帧稳定）。
- **修法二**（取证能力）：选中日志从 `debug` 升到 `info` 并带位置：`Menu: golden group <名字> selected (<第几行> of <总数>).` —— 默认日志级别就能证明点击有没有进到代码里。
- **修法三**（守卫）：新增 `tests/golden_row_id.ps1`，断言全仓不再有 `PushID(<字符串指针>)`、组行按行号推 id、选中日志为 info、`GoldenRow` 的显式宽高仍在。
- **保留**：1.5.43 的三条修复（显式命中矩形、两栏宽度夹紧、脏标记 + 只有「导出」写盘 + `[not saved]` 提示）与 light 插件 id 宽度全部不动。
- **未改动**：注入壳、贴件语义、槽位清理、`.tri` 推导、场景套装切换、钩子安装、逐槽策略、从 ESP 生成套装、金件存库锁。
- **产物**：DLL `OutfitSystemNG.dll` 2,042,368 字节，sha256 `859d81d55db6de8b91224a96f998f03f32caf34dd96d8344f9d4134d0640914f`，FileVersion/ProductVersion `1.5.44.0`；Release 全量重编 `BUILD_EXIT=0`（0 error / 0 warning，留档 `build/build_1_5_44.log`）；全量包 31 条目、测试包 2 条目（只含 DLL + 说明）。
- **未验证**：本机没有游戏，结论来自 ImGui 点击语义 + 同函数里「指针 id 坏 / 整数 id 好」的对照事实，不是实机复现。判据是日志里的 `Menu: golden group … selected (N of M).`。

## [1.5.43] - 2026-11-21

### 修：金件页三处（点不中 / 每次编辑都写盘 / light 插件金件解析不到）（m10387）

- **用户原话**：「上个轮问题已修复。“金件”列表里鼠标很难点中金件，或者说压根点不中！ 将“金件”的json自动导出取消，在界面提示，完成修改后需要手动点击导出才会保存。 “金件”中“添加某个esp的全部装备”后，右侧的单件菜单里会显示”<当前加载顺序里没有>“」
- **症状一**：金件左栏组行点不中；**症状二**：改一件就自己写盘，没有「改完再导出」这回事；**症状三**：用「Add every piece of a plugin」加进来的 light(ESL) 插件金件，右侧每行都是 `<当前加载顺序里没有>`，`Golden items: 0 entr(ies) resolved, 61 missing.`
- **日志证据**：`GoldenItems.cpp:93` 连发 61 条 `Golden items: <plugin>:<id> was not found in this load order.`（`[SSE]Mini Bunny.esp:002E280C`、`AE_AP_Xiaoyu_Bunny.esp:002E1D66`、`VtawWardrobe4 [SE].esp:002E0823`），紧接着 `GoldenItems.cpp:96: 0 entr(ies) resolved, 61 missing.`；添加当时却是成功的（`51 piece(s) of [SSE]Mini Bunny.esp added.`）。三个插件 id 的高 12 位 0x2E0/0x2E1/0x2E2 连续递增 = light 插件的光索引。
- **真因**：①`IG::Selectable(label, isSelected)` 尺寸默认 `(0,0)`，命中矩形只有一行文字高，且左栏可宽到把右侧 `ImVec2(0, 440)`（剩余宽度）挤成负宽、盖在左栏上吞点击；②1.5.39 起「批量添加 / 移除单件 / 移除整组 / 手输添加」四处都在改完立刻 `Golden::SaveToDisk()`；③金件存的是 `formId & 0x00FFFFFF`，对 light 插件这 24 位里高 12 位是光索引，而 `TESDataHandler::GetFormID(modIndex, lightIndex, formID)` 对 light 插件是 `0xFE000000 + (lightIndex << 12) + formID`（不掩码）⇒ `LookupForm` 二次叠加 ⇒ 永远查不到。
- **修法一**（`PluginScan`）：`Index` 增 `light` 表，新增 `IsLightPlugin()` / `LocalIdFor()`（light 掩 `0xFFF`）/ `ResolveForm()`（先掩码 id、失败再用 24 位兜底）；`ResolveArmor()` 改走它们。
- **修法二**（`GoldenItems`）：`Normalise()` 对每条跑 `LocalIdFor()`（旧档 61 条加载即自愈）；`AddArmor()` / `AddAllFromPlugin()` / `AddLocal()` / `Resolve()` 全部改走 `LocalIdFor()` / `ResolveForm()`；`AddLocal()` 的报错文案改成 `No armour with that id was found in that plugin.`。
- **修法三**（`MenuUI`）：新增 `g_goldenDirty`，四处自动保存改成置脏，`RenderGolden()` 内 `Golden::SaveToDisk()` 只剩「导出」一处；数据路径下方常显 `[not saved] The changes live in memory only; press Export…` / `Changes reach the file only when you press Export.`；新增 `GoldenRow()` 给插件选择器与组行显式宽高，`listWidth` 夹到 `[240, max(240, avail - 320)]`、右栏 `max(320, avail)`；`Remove` 按钮挪到行首。
- **守卫测试**：新增 `tests/golden_local_id.ps1`（掩码宽度、ResolveForm 兜底、SaveToDisk 只有 1 处、GoldenRow 用法、宽度夹紧）；同步修掉 `tests/esp_outfits_filter.ps1` 里因重构失效的单件查找断言。全量 20 个脚本 18 PASS，2 项既有环境失败（`hair_tint_read_only.ps1` 锚点消失、`skse_listener_replay.ps1` 缺现场日志）。
- **未改动**：引擎侧（注入壳 / 贴件语义 / 槽位清理 / `.tri` 推导 / 场景套装 / 钩子安装）、逐槽策略、从 ESP 生成套装、金件存库锁（1.5.41）一行未改。
- **未验证**：本机无游戏。点不中的根因是 ImGui 语义推断（对照 `MenuUI.cpp:737-738` 显式给宽高的写法），light id 宽度是从日志 id 分布加 CL-NG `GetFormID` 事实推出；三项都要实机确认。
## [1.5.42] - 2026-11-21

### 修：删除幻化套装预设时闪退（m10183）

- **用户原话**：「1.5.41启动游戏时的ctd修复了，又出现新的ctd：在删除幻化套装预设时出现了ctd。」
- **症状**：菜单里的幻化套装列表，在第 2 页及以后删除一套预设就闪退；第 1 页删除没事。
- **崩溃证据**（`crash-2026-11-21-05-13-54.log`）：`Unhandled exception at 0x7FF88FC3933A KERNELBASE.dll+00C933A`，线程 16272，`Process Uptime 00:08:39`；`[RSP+188]` = `(char*) "vector too long"`、`[RSP+48]` = `(std::length_error*)`、`[RSP+2B0]` = `src\cpp\MenuUI.cpp`、`[RSP+2C0]` = `void __cdecl SOS::Menu::`anonymous-namespace'::RenderOutfits(void)`；前 6 帧全在 `OutfitSystemNG.dll`，第 7 帧才是 `SKSEMenuFramework.dll+0110402 UI.cpp:1585 UI::RenderMenuWindow`。
- **定位手法**：无 .pdb / dumpbin，用自写 PE 解析 —— `.pdata` 的 4,738 个 `RUNTIME_FUNCTION` 把 `0x8C164` 落到 `0x8B640..0x8DA34`（`+0xB24`）= `RenderOutfits`、把 `0x2BA0` 落到存有 `vector too long` 的小函数（`std::_Xlength_error` 抛出点），`0x8C15F` 处是 `call 0x91740`（`std::vector` 的 assign）。
- **真因**：`RenderOutfits()` 删除按钮先 `delete_outfit(...)` 再 `g_filtered.clear();`（第 968 行），紧接着仍按**删除前**的页码算本页区间：`begin = (g_page - 1) * pageSize`、`end = min(g_filtered.size(), begin + pageSize)`。列表清空后 `end = 0` 而 `begin > 0`，`end - begin` 以 `size_t` 下溢成接近 `SIZE_MAX`，`pageFavorites.assign(end - begin, 0)` / `pageArmorCounts.assign(end - begin, 0)` 抛 `std::length_error("vector too long")`；异常在菜单渲染回调里没人接，展开到游戏主循环 → 闪退。第 1 页 `begin = 0` 所以不崩。
- **修法一**：先取 `const std::size_t total = g_filtered.size();`，再 `begin = std::min(total, static_cast<std::size_t>(std::max(0, (g_page - 1) * pageSize)))`、`end = std::min(total, begin + static_cast<std::size_t>(pageSize))` ⇒ `begin <= end <= total` 恒成立。
- **修法二**：`assign` 本身不放宽（夹紧是硬前提），新增 `tests/menu_page_slice.ps1` 静态断言三行夹紧存在、旧写法不存在、`g_filtered.clear();` 在夹紧之前、本页循环是 `begin < end`。
- **未改动**：引擎侧（注入壳 / 贴件语义 / 槽位清理 / `.tri` 推导 / 场景套装切换 / 钩子安装）与金件存库（1.5.41 的递归锁 + 兜底）一行未改。
- **未验证**：本机无游戏；崩溃点由 PE 帧对位 + 源码复核得出，不是实机复现。
## [1.5.41] - 2026-11-21

### 修：启动即闪退——金件存库锁的自死锁（m10014）

- **症状**：装 1.5.40 后直接启动游戏即闪退（崩溃日志 `Process Uptime 00:00:39`，连第一帧都没到）。
- **崩溃证据**（`crash-2026-11-21-04-06-07.log`）：栈顶是 `KERNELBASE.dll+00C933A`，但 `[RSP+18]` / `[RSP+188]` 是字符串 `resource deadlock would occur`、`[RSP+48]` 是 `std::system_error*`，`PROBABLE CALL STACK` 前 7 帧全在 `OutfitSystemNG.dll`。异常来自 C++ 标准库：`std::system_error(errc::resource_deadlock_would_occur)`，不是引擎访问违例。
- **真因**：`src/core/GoldenItems.cpp` 第 37 行的 `g_mutex` 是普通 `std::mutex`，而 `ImportText()` 持锁后又调公共 `TotalCount()`（自己再 `lock_guard` 一次）⇒ 同线程重入。MSVC 对这种重入**抛异常而不是死等**，异常在 SKSE 消息回调里没人接，直接展开到游戏主循环 → 闪退。触发点是 `kDataLoaded` 里无条件的 `Golden::LoadFromDisk()`；1.5.39 那次金件页崩溃**之前** `SaveToDisk()` 已写出 `Data/SKSE/Plugins/OutfitSystemGoldenItems.json`，所以 1.5.40 起每次启动必崩。该缺陷自 1.5.39 引入（`git diff v1.5.39 HEAD -- src/core/GoldenItems.cpp` 为空）。
- **定位手法**：本机无 dumpbin / .pdb，用自写 PE 解析 —— `.pdata` 的 4,738 个 `RUNTIME_FUNCTION` 把 6 个返回地址对到函数区间（`0xDE131`→`0xDE0C0+0x71`、`0x19AAF`→`0x19630+0x47F`、`0x11651F`→`0x116488+0x97` 等），再扫函数体内的 `lea rip-rel` 取字符串、`call` 取调用目标 ⇒ `MessageHandler(kDataLoaded)` → `Golden::LoadFromDisk` → `Golden::ImportText` → 计数 getter → `std::mutex::lock`。
- **修法一**：`g_mutex` 改 `std::recursive_mutex`（同线程重入以后合法）。
- **修法二**：新增 `TotalCountLocked()`（不自锁），`ImportText()` / `LoadFromDisk()` / `SaveToDisk()` 的计数全改用它；公共 `TotalCount()` 留外部。
- **修法三（兜底）**：`kDataLoaded` 的金件块整体包进 `try / catch(std::exception) / catch(...)`，失败打 critical `Golden items: the store could not be loaded (…); the golden list stays empty for this session.`，游戏照常进。
- **修法四（兜底）**：`MessageHandler()` 拆成 `HandleMessage()` + 只做 try/catch 的壳，日志 `Message handler: an exception escaped (…); the rest of this message is skipped.` —— 异常再也出不了 SKSE 回调。
- 新增静态守卫 `tests/golden_lock_guards.ps1`（锁递归、无 `std::to_string(TotalCount())`、启动加载在 try 内、消息壳在位）。

### 未改动

- 引擎侧零改动：注入壳、贴件语义、槽位清理、`.tri` 推导、场景套装切换、钩子安装、菜单 / ESP 生成 / 金件功能一行未改。1.5.25 实测正常的基础幻化、1.5.34 已验收的场景切换、1.5.40 的两处修复均不受影响。
- 数据格式零改动：`OutfitSystemGoldenItems.json` 的 `{"version":1,"groups":[…]}` 原样，1.5.39 / 1.5.40 写出的文件照读。

### 复核

- `BUILD_EXIT=0`，0 error / 0 warning（日志 `build/build_task.log`，留档 `build/build_1_5_41.log`）。
- 守卫测试 16 项 PASS（新增 `golden_lock_guards.ps1`）；2 项既有环境失败（`hair_tint_read_only.ps1` 锚点消失、`skse_listener_replay.ps1` 缺 `bug/skse64.log`），不是回归。
- `OutfitSystemNG.dll` 2,037,760 字节 / `39f6b65eb45a6ba671c5cf2f88f192caf25019b78a91be0f86a53265fd967ad0` / `1.5.41.0`。

### 未验证

- 本机无游戏：没有实机确认「装完能进游戏」。判据是崩溃栈字符串 + PE 帧对位 + 源码核对，不是实机复现。

## [1.5.40] - 2026-11-21

### 修：1.5.39 的两处回归（m09740）

- **「幻化」页点「Choose a plugin...」后 ESP 列表为空**
  - 真因：`PluginScan::IsOutfitArmor()` 判「有没有模型」时只看 `bipedModels[RE::SEXES::kMale]` / `bipedModel1stPersons[kMale]`；只做女性服装的包男性模型为空，全库被判成非服装；`EspOutfits::PluginsWithArmor()` 又按 `entry.outfitArmors == 0` 过滤，列表被清空。金件页用的是不带该过滤的 `PluginScan::PluginsWithArmor()`，所以它的列表正常。
  - 修法：判据同时看男女（`for (const auto sex : { RE::SEXES::kMale, RE::SEXES::kFemale })`，三/一人称都查）；名字判据放宽为 `GetFormEditorID()` 或 `GetFullName()` 任一非空；列表过滤改 `entry.armors == 0`；新增 `OutfitVerdict` 把四类跳过原因分开计数。
- **「金件」页点「Add every piece of a plugin」必 CTD，之后左侧组点不中、右侧编辑器空白**
  - 真因：`I18n::T()` 的返回值被当 printf 格式串用，而中文译文占位符顺序与英文相反（`"%zu piece(s) of %s added."` ↔ `"已添加 %s 的 %zu 件。"`、`"Imported %zu piece(s) from %s"` ↔ `"已从 %s 导入 %zu 件"`）⇒ `%s` 拿到 `std::size_t` 当指针 ⇒ `strlen(0x8)` ⇒ `EXCEPTION_ACCESS_VIOLATION at OutfitSystemNG.dll+0125E41 cmp byte ptr [r8], 0x00`。崩溃栈前 7 帧全在 `RenderGolden` 的渲染路径内。
  - 修法：两处中文串改为与英文同序；`I18n::Apply()` 新增 `PlaceholderSignature()` / `PlaceholdersMatch()`，译文占位符与英文不一致就忽略该条并回退英文（warn 明细最多 5 条 + 汇总 1 条）。

### 未改动

- 引擎侧零改动：注入壳、贴件语义、槽位清理、`.tri` 推导、场景套装切换、钩子安装。1.5.25 实测正常的基础幻化与 1.5.34 已验收的场景切换不受影响。
- 数据格式零改动：幻化 JSON、金件 JSON、`[Customization] EspOutfitAutoFavorite` 原样。

### 复核

- `BUILD_EXIT=0`，0 error / 0 warning（日志 `build/build_1_5_40.log`）。
- 守卫测试 15 项 PASS（新增 `tests/golden_menu_guards.ps1`：全仓 `I18n::T` 双串占位符同序、`I18n` 护栏在位、ESP 列表按 `entry.armors` 过滤、服装判据双性别）；2 项既有环境失败不是回归。
- `OutfitSystemNG.dll` 2,036,224 字节 / `95c2b38848bf3d3f2a0577f2b611b8cbc06c7ad76f653674eb89fbcb5a5618bf` / `1.5.40.0`。

### 未验证

- 本机无游戏：菜单、ESP 列表、金件增删都没实机走过；崩溃点由 PE 解析对位到 `snprintf`，非实机复现验证。
## [1.5.39] - 2026-11-20

### 新增：逐槽策略按套装独立 + 从 ESP 生成套装 + 金件页（m09191 / m09426）

**用户诉求**（m09191）：①幻化栏选中一套装时勾选右侧「逐槽策略编辑器」，会把**所有套装**的该选项一起打开 —— 应只对勾选的套装生效，且导入导出 JSON 保持兼容；②新增「按 ESP 自动生成套装」（点按钮弹列表、按加载顺序倒序、选中后扫描装备生成新套装、默认名 = ESP 名称、焦点跳到新套装、同槽位多件拆成 `ESP名称_1`…、槽位策略统一「需要已装备」、设置里加「生成套装自动收藏」开关默认打开）；③新增「金件」栏（左列表按 ESP 分组、右侧显示该组金件、增删齐备、支持从 ESP 自动生成与按 FormID 选单件、金件 JSON 与幻化 JSON 分开）。m09426 追加：生成套装扫描时**只收正常服装**（FX 特效、没有 EditorID、各种不算），**金件放开**按自己的规则来。

**实现**：

1. `MenuOutfitEditor.cpp`：逐槽策略展开状态从全局布尔改为按套装名保存的 `std::unordered_set<std::string> g_slotPolicyOpen`，旧全局变量删除；JSON 仍是每套自己的 `slotPolicy` / `slotPolicies`，格式未变。
2. 新增 `src/core/PluginScan.h/.cpp`：按 `GetLoadedMods()` + `GetLoadedLightMods()` 建加载顺序视图，护甲用 `GetFormArray(RE::FormType::Armor)` 按 `OwningPlugin()` 归组；新增 `IsOutfitArmor()`（EditorID 非空 + 至少一个非 FX 槽位 + 至少一个 armor addon 有模型），并给 `PluginSummary` 加 `outfitArmors` 计数。
3. 新增 `src/core/EspOutfits.h/.cpp`：`GenerateFromPlugin()` 先用 `IsOutfitArmor()` 过滤（被排除的计入 `notClothing`），分桶用 `mask & ~kFxBit`，按 32 槽「每槽第 i 件配第 i 套、不足复用第 0 件」生成，同名原地覆盖，生成后 `FocusOutfitInList()` + 刷新。
4. 新增 `src/core/GoldenItems.h/.cpp`：金件独立存 `Data\SKSE\Plugins\OutfitSystemGoldenItems.json`，`AddAllFromPlugin()` / `AddLocal()` **不做服装过滤**（m09426 的金件放开）；`Settings::IsGoldenItem()` 委托 `Golden::IsGolden()`；旧 ini `goldenItems` 在 kDataLoaded 一次性迁移并写 `[Compatibility] GoldenItemsMigrated`。
5. `MenuUI.cpp`：幻化页底部新增「从 ESP 生成套装」块，新增「金件」页（`RenderGolden()`，左列表右编辑 + 导出/导入 + 按 FormID 添加），设置页新增「套装生成」段放 `EspOutfitAutoFavorite`；全部文案走 `I18n::T()`，中文侧同步。
6. `Settings.h/.cpp` 新增 `espOutfitAutoFavorite{true}`（`[Customization] EspOutfitAutoFavorite`，默认打开）与 `goldenIniMigrated{false}`（`[Compatibility] GoldenItemsMigrated`）。
7. 新增守卫测试 `tests/esp_outfits_filter.ps1`：断言套装生成走 `IsOutfitArmor()` 且金件路径**不**调用它。

**未改动**：钩子安装、贴件语义、槽位清理、`.tri` 推导、`SceneOutfits` 一行未动（`git diff v1.5.34 HEAD -- src/cpp/hooking src/cpp/MorphRefresh.cpp src/cpp/SceneOutfits.cpp` 为空），1.5.25 / 1.5.34 的实机结论不受影响。

**未验证**：本机没有游戏；菜单交互、ESP 扫描结果、生成套装外观全部未实机验证；静态侧 `BUILD_EXIT=0`（0 error / 0 warning）+ 守卫测试 7 项通过。

## [1.5.34] - 2026-11-18

### 修复：场景事件被线程门禁整条丢弃（m07842）

**用户报告**：「这次 OStim 场景开始后幻化压根没切换」。

**真因**：1.5.33 重写 `SceneOutfits.cpp` 时把 `AutoSwitch` 的线程模型（`OnGameThread()`）也照抄了进来，并写成 `if (!OnGameThread()) { warn; return kContinue; }`。但 OStim / SexLab 的 mod 事件是从它们自己的工作线程投过来的，于是事件在查名字之前就被丢掉 —— 用户日志 `v1-5-33-0` 里三条 `'ostim_start' / 'ostim_end' arrived off the game thread; the scene switch is skipped.`（线程 39516 / 10604 / 27040，主线程 34628）就是这三条被丢的事件。

**修法**：

1. `ModEventSink` 内离线程事件改走 `SKSE::GetTaskInterface()->AddTask([eventName]() { Dispatch(eventName); });`，把换装交接到游戏线程；游戏线程上的事件仍内联 `Dispatch(eventName)`。
2. 新增单一入口 `void Dispatch(const std::string& a_event)`：`std::lock_guard switchLock(g_switchMutex);` 内按事件名调 `StartScene()` / `EndScene()`。
3. 不建队列、不加 `Tick()`、不做 generation 计数（队列那套是 1.5.33 按 m07666 删掉的），只借「一次 `AddTask`」这一个既有 API。
4. 任务接口缺失时 warn `… there is no task interface, so the switch cannot be handed over; the scene outfit is left as it is.`，不换装也不崩。
5. 离线程时 debug 打 `SceneOutfits: '{}' came from thread {} (game thread {}); the switch is handed to the game thread.`，为下一轮取证留线程号。

**未改动**：换装调用序列仍是 `setOutfitByName()` + `OutfitSystem::refreshArmorFor()`；`AutoSwitch.*` 仍为空 diff；钩子层 / 注入壳 / 贴件语义 / 槽清理 / `.tri` 推导 / DLL 文件名零改动。

**未验证**：尚未实机验证。若日志出现 `scene started; 'A' -> 'A_sex'.` 但模型仍不出现，那是「切了但 3D 不刷新」的另一层问题（1.5.30/1.5.31 的方向已被实机否掉），需按新日志再定位。
## [1.5.33] - 2026-11-18

### 修复：场景套装回退到 1.5.29，只保留「找名字 → 用状态切换那套方法换装」（m07666）

- **用户指令**（m07666 原话）：「依旧和之前一样的问题，我发现你改的地方是不是错了，我说的是 Sexlab 和 OStim 场景的专用换装 `_sex`，你不会改的是按地点/状态自动切换吧？不行直接回退到 1.5.29，从这开始改，把原来的 Sexlab 和 OStim 场景的专用换装 `_sex` 套装逻辑删了，参照按地点/状态自动切换这里的换装方法去换装。」
- **先核对「有没有改错模块」**：`git diff v1.5.29 HEAD -- src/cpp/AutoSwitch.cpp src/cpp/AutoSwitch.h` 输出为空 ⇒ 按地点/状态自动切换从 1.5.29 到 1.5.32 一行未动；场景 `_sex` 换装一直在 `SceneOutfits.cpp`（`EndsWithSexSuffix()`、`candidate = current + "_sex"`、四个 mod 回调事件）。1.5.32 的问题不是改错文件，而是场景入口走了菜单那条 `Features::AssignOutfit()` 并自带一套 SKSE 任务队列 + `Tick()` 心跳，与状态切换的两步序列不同。
- **修法一（回退源码基线）**：`git checkout v1.5.29 -- src` —— 1.5.30 的补跑 pass、1.5.31 的 `rebuildActorModel()`、1.5.32 的 `Features::AssignOutfit()` 场景入口全部撤掉；CMake 的模块名 `OutfitSystemNG.dll` 保留。
- **修法二（新增共用入口，`OutfitSystem.h/.cpp`）**：`bool setOutfitByName(OutfitService&, RE::Actor*, const char*)`，与 `setOutfitUsingState()` 并排：`has_outfit()` → `set_outfit_c()` → `true`；名字不存在返回 `false` 且不改状态。
- **修法三（`SceneOutfits.cpp` 只剩「找名字 + 换装」，与 `AutoSwitch::Evaluate()` 同构）**：StartScene / EndScene 共用 `SwitchTo()` = `{ 取 service 锁 → setOutfitByName() }` 然后锁外 `OutfitSystem::refreshArmorFor(a_actor)`。事件在 `ModCallbackEvent` sink 里同步执行，`std::recursive_mutex` 串行化，非游戏线程 warn 并跳过。
- **修法四（删掉场景专用机件）**：移除 `QueueSceneEvent` / `DrainSceneEvents` / `ScheduleDrain` / `g_generation` / `g_queueMutex` / `g_dispatchMutex` / `g_pendingEvents` / `g_drainScheduled` / `std::deque` / `GameState::IsLoading` 分支 / `enum class SceneEvent` / `DispatchSceneEvent()` / `SceneOutfits::Tick()`，以及 `main.cpp:279` 与 `NGHotkeys.cpp:255` 两处调用；`SceneOutfits.h` 只剩 `Install()` / `Reset()`。开始/结束日志升到 info（`SceneOutfits: OStim scene started; 'A' -> 'A_sex'.` / `SceneOutfits: OStim scene ended; 'A' restored.`）。
- **未改动**：基础幻化钩子（`Hooks.cpp`）、注入壳、贴件语义、槽位清理、`.tri` 目标推导、DLL 文件名（`OutfitSystemNG.dll`）一律不动；1.5.25 实测正常的基础幻化与空幻化不受影响。
- **未验证**：本机没有 Skyrim。静态侧新增 `tests/scene_outfit_switch.ps1`（断言场景模块无队列/`Tick`/补跑/重建，换装必须是 `setOutfitByName` 紧跟 `refreshArmorFor`），删除过时的 `tests/scene_outfit_queue.ps1`；全量守卫测试除 2 条既有环境失败外全部 PASS。判定见 `docs/DELIVERY.md` §4.61、`docs/TESTING.md` §3.45。

## [1.5.32] - 2026-11-18

### 修复：场景套装只做「找对名字 → 切套装」（m07382 / m07424）

- **用户指令**（m07424 原话）：「你只用找对名字然后切换，切换套装不用管什么之前逻辑里头饰假发显示正常了直接不用那个了删了，不行你就参考按地点/战斗切换幻化套装这里的逻辑。」上一轮（m07382）更直接：「让你把原来的那套专门给 _sex 切换的删了！不是让你继续修补那套逻辑！」
- **症状**（1.5.31 实机）：OStim 场景开始切 `_sex` 套装，模型不出现，按一次 Tab/Z 打开快速选择窗口才出现；场景结束回普通幻化同样要按键。切到 `_sex` 后**头饰/假发正常、身体/手/脚是空模型**。
- **证据**（`SkyrimOutfitSystemNG.log`，754,136 字节 / 8,081 行，v1-5-31-0）：19:20:35.403 场景前 3P pass 完整（`attaching 5 outfit-only piece(s) … mask 0x00002096`，五槽全 `part 1`）；19:20:35.421 `SceneOutfits: OStim scene started`；1.5.31 的三次随访全部执行（`follow-up 1 of 3`、`asking the game to rebuild the model of 00000014 … (follow-up 2 of 3)` + `Model Rebuild for JueLunV6`、第三次 `Armor Refresh`）**模型仍不出现**；19:20:44.056 起引擎自己连跑十几轮 pass（几十条 `attaching … (outfit revision false)`，无对应 `Armor Refresh`），模型随后出现；19:20:48.781 `HairTintDiagnostics` dump 显示 3P biped slot 1/2/4/7/13 全部有 addon 与几何。⇒ 数据层正确，问题不在切换、也不在 pass 次数。
- **修法一（删除补跑机件，`MorphRefresh.cpp` / `MorphRefresh.h`）**：移除 `kOutfitPassCount`、`kOutfitPassDelay`、`struct OutfitPassState`、`g_outfitPassMutex`、`g_outfitPasses`、`RequestOutfitPassesInternal()`、`DrainOutfitPasses()`、公开的 `RequestOutfitPasses()` 以及 `Reset()` 里的清表块。
- **修法二（删除模型重建，`OutfitSystem.cpp` / `OutfitSystem.h`）**：移除 `rebuildActorModel()`（`Actor::Update3DModel()`）。`NGFeatures::RefreshActor()` 回到只调 `::OutfitSystem::refreshArmorFor(a_actor)`。
- **修法三（场景切换只做切换，`SceneOutfits.cpp`）**：StartScene / EndScene 一律调 `Features::AssignOutfit(player, name)` —— 与「点套装列表」（`MenuUI::ApplyTo` / `QuickWearFocused`）同一条函数；场景事件在游戏线程上**同步**执行（新增 `OnGameThread()`，与 `AutoSwitch` 的 location / fast-travel sink 同构），非主线程事件才落到任务队列；`g_dispatchMutex` 改 `std::recursive_mutex` 防同步重入。
- **修法四（名字查找）**：先试 `has_outfit("<当前套装>_sex")`，没有再遍历全部套装名做忽略大小写匹配（命中用真实大小写）；仍找不到打 info `no … outfit for the … scene; create an outfit with exactly that name…`。`_sex` 套装不再有任何专属槽位/时机规则。
- **未改动**：基础幻化钩子（`Hooks.cpp`）、注入壳、贴件语义、槽位清理、`.tri` 目标推导、DLL 文件名（`OutfitSystemNG.dll`）一律不动。
- **未验证**：本机没有 Skyrim。判定见 `docs/DELIVERY.md` §4.60、`docs/TESTING.md` §3.44。
## [1.5.31] - 2026-11-18

### 修复：场景开始/结束都必须弹一次窗才显示（m07050）

- **症状**：1.5.30 实机 —— OStim 场景开始切 `_sex` 套装模型不出现，按一次 Tab/Z 打开快速选择窗口才出现；**场景结束回普通幻化也要按一次窗口**才变回来。用户要求「直接把原来 `_sex` 套装切换部分删了，直接按『点套装列表』原来走的那条函数走」。
- **证据**（`SkyrimOutfitSystemNG.log`，584,253 字节 / 6,129 行，v1-5-30-0）：18:40:22.114 场景开始，1.5.30 的三次随访 22.250 / 22.672 / 23.588 全部按时执行且每次都有 `Custom: attaching 5 outfit-only piece(s) of 5 displayed (mask 0x00002096 …)`，**模型仍不可见**；22.381 的 `HairTintDiagnostics` 全量 dump 显示 3P biped 五个槽**全都有真实几何**；18:40:42.955 引擎自己又跑 2+2 轮 pass，模型依旧不出来；18:40:47.193 打开快速选择窗口后才恢复正常。所有 `Custom:` 行与 pass 都在同一线程（29720），差别不在线程。
- **真因**：装备 pass 只把克隆写进 `BipedAnim::objects[slot]`，被绘制的那份 3D 模型仍是切换前建的；**开/关一次菜单会让引擎重建该模型**，这才是「一按就出来」的真正机制。1.5.30 的补跑只是重复写 biped，从未触发模型重建。
- **修法一（重建模型，`OutfitSystem.h/.cpp`）**：新增 `bool rebuildActorModel(RE::Actor*)` —— `Is3DLoaded()` 守卫、日志 `Model Rebuild for {}` / `Skipping model rebuild for {}: no 3D yet.`、`actor->Update3DModel()`。
- **修法二（随访链加一步，`MorphRefresh.cpp`）**：`kModelRebuildAttempt = 2`，第 2 次随访之前调用一次 `rebuildActorModel()`，日志 `asking the game to rebuild the model of {:08X} so the illusion reaches the drawn 3D (follow-up {} of {})`；其余两次仍是 `refreshArmorFor`。
- **修法三（场景入口照用户要求改写，`SceneOutfits.cpp`）**：场景开始 / 结束**删掉**自己拼的 `set_outfit_c` + `Features::RefreshActor()`，改调 `Features::AssignOutfit(player, name)` —— 与「点套装列表」完全同一条函数；失败时 `warn`（`could not be assigned for the … scene; the current outfit is kept.` / `could not be restored after the … scene.`）并保持原状。
- **未改动**：基础幻化钩子（`Hooks.cpp`）、注入壳、贴件语义、槽位清理、`.tri` 目标推导、DLL 文件名（`OutfitSystemNG.dll`）一律不动；1.5.25 实测正常的基础幻化不受影响。
- **未验证**：本机没有 Skyrim。判定见 `docs/DELIVERY.md` §4.59、`docs/TESTING.md` §3.43。
## [1.5.30] - 2026-11-18

### 修复：场景里切 `_sex` 套装不显示、必须弹一次窗（m06712）；模块改名 `OutfitSystemNG.dll`（m06781）

- **症状**：OStim 场景开始时 `_sex` 套装不生效（角色还是裸的或上一套），场景结束回普通幻化也不更新；按一次 Tab/Z 打开快速换装窗口后立刻正常。用户建议「直接把点套装列表切换的逻辑拿过来用」。
- **证据**（`SkyrimOutfitSystemNG.log`，1,767,358 字节 / 18,260 行，v1-5-29-0）：17:51:50.339 一次 `Armor Refresh` + `Custom: attaching 5 outfit-only piece(s) … (mask 0x00002096)`，17:51:50.357 场景开始；之后 16.5 秒零钩子调用。按秒统计 `CustomOne`：17:51:50 = 2，而用户按下 Tab 的 17:52:06/07 = 2 + 28，之后 17:52:11 = 2 —— 引擎在那一秒连跑 14 轮 pass。
- **真因**：「幻化集已切换」不等于「渲染模型已更新」。一次装备 pass 只把 `addon`/`part` 写进 biped 槽，真正被渲染的克隆几何由紧随其后的 pass 物化；菜单路径之所以灵，是开关菜单让引擎连跑十几轮 pass。脚本路径（`SceneOutfits`）一次 pass 之后没有任何后续 pass。1.5.29 的 `IllusionMissingSlots` 只看 `partClone` 是否为空，在这个时序下永远判不出来（一次 pass 后为空属正常中间态，全日志 `clone 1` 0 条）。
- **修法一（有限次随访 pass，`MorphRefresh.cpp`）**：新增 `RequestOutfitPasses(actor)` → 登记 3 次随访（120 / 400 / 900 ms）；主线程 `PollOnMainThread()` 中的 `DrainOutfitPasses()` 到期调 `OutfitSystem::refreshArmorFor()`，并打 debug `repeating the equip pass for <FormID> (follow-up N of 3), because one pass alone does not materialise the illusion.`；幻化总开关关闭时直接返回。
- **修法二（统一入口）**：`Features::RefreshActor()` 末尾调用 `RequestOutfitPasses()`；`SceneOutfits` 的两处 `OutfitSystem::refreshArmorFor(player)` 改为 `Features::RefreshActor(player)`，于是「点列表」与「脚本切换」走同一条序列。
- **修法三（模块改名）**：`CMakeLists.txt` 设 `OUTPUT_NAME "OutfitSystemNG"`、部署到 `dist/SKSE/Plugins/OutfitSystemNG.dll`；旧名不再发布并在仓库内删除。插件身份（`Plugin::NAME`、日志文件名 `SkyrimOutfitSystemNG.log`、Papyrus 翻译前缀、存档标识）一律不变。用户安装时必须删除旧的 `SkyrimOutfitSystemNG.dll`，否则双加载。
- **不动的东西**：`Hooks.cpp` 钩子、注入壳代码、贴件语义、槽清理策略均未改；1.5.25 已实测确认的基础幻化与空幻化不受影响。
- **未验证**：本机没有 Skyrim。「不弹窗也能显示」与 120/400/900 ms 三档延迟未实机验证；若某套件仍需更多 pass，可再加一档而不改结构。

## [1.5.29] - 2026-11-18

### 修复：场景结束后幻化件残留，"要再按一次键才刷新"（m05502）

- **症状 A**：OStim 场景中 `_sex` 幻化没显示（看到裸体），场景结束切回普通幻化时**闪退**。**症状 B**：场景中按 Tab/Z 打开快换窗后 `_sex` 生效，结束场景切回普通幻化**没有**闪退，但模型仍是旧的一套，需要**再按一次** Tab/Z 才正确。
- **证据**（`SkyrimOutfitSystemNG-TAB.log`）：`_sex` 套装掩码 `0x00002096`（bit 0,1,2,4,7,13），普通套装 `0x00002186`（bit 1,2,7,8,13）；**差集 bit 4（CK 槽 34）只在 `_sex` 里**。6143 贴完 `_sex` 后到 8961 那次 `Armor Refresh` 之间，该槽的幻化克隆没有任何人拆，而引擎也没有可重建的真甲 —— 于是视觉一直停在 `_sex` 的那件。
- **真因**：1.5.25 为了修「身体槽永久空模型」，把所有「离开幻化集的槽」一律只打 `Custom: N slot(s) left the display set …; left to the engine`，没有区分两种截然不同的情况：槽里是**引擎自己维护的真实条目**（交回引擎正确），还是槽里是**我们上一套贴上去的克隆**（交回引擎就没人拆了）。
- **修法一（只拆自己的残留克隆，`Patches.cpp`）**：`g_appliedMasks`（uint32）升级为 `AppliedState { displayMask; ownedMask; }`，`ownedMask` 只记录「贴进去时没有真实穿戴物打底」的槽。离开幻化集的槽只在四条同时成立时 `ClearBipedPart` 并打 `Custom: detached N illusion clone(s) whose slot left the display set (mask 0x…) for …`（debug）：①`previousOwned & flag`（上一轮是我们放的）；②`wornBySlot[bit] == nullptr`（无人真实穿戴）；③`(skinMask & flag) == 0`（不是 `Actor::GetSkin()` 的皮肤/身体槽）；④`anim && anim->objects[bit].partClone`（槽里确实还挂着克隆）。不满足仍打 `left to the engine`。
- **修法二（幻化几何对账，`MorphRefresh.cpp`）**：新增只读的 `CustomSkinPlayer::IllusionMissingSlots(actor)`（按已记录 `displayMask` 查 `objects[bit].partClone`，要求 `anim->root` 非空）与 `ReattachDroppedIllusion()`；主线程任务发现缺失就用**已经验收的** `::OutfitSystem::refreshArmorFor(actor)` 重建，同角色 300 ms 退避、最多 3 次，超限 warn `still holds no illusion geometry … after 3 rebuilds; it is left alone from now on.` 并放手。这段代码里没有任何 OStim / SexLab / SAM / OBody / RaceMenu / SLIF 名字（守卫测试会拦）。
- **不动的东西**：`Hooks.cpp`、注入壳代码、站点解析、贴件语义、1.5.25 的「不清空引擎槽」策略全部未改；基础幻化行为与 1.5.25 一致。
- **新增测试**：`tests/skinning_stale_slots.ps1`（`ClearBipedPart` 在 `Patches.cpp` 只出现一次且四个条件齐全、`IllusionMissingSlots` 只读、重建函数不含框架名）。
- **未验证**：本机没有 Skyrim。同批日志里的 `EXCEPTION_ACCESS_VIOLATION` at `SkyrimSE.exe+12BACD7`（`test byte ptr [rax+0xF4], 0x01`，RAX=0，发生在 `hdtSMP64` → 渲染/剔除阶段）**与本插件的关系没有证据**，本版不加任何"防崩"逻辑，只记录分析与判据（`docs/CRASH_221544_AUDIT.md`）。

## [1.5.28] - 2026-11-17

### 资源路径、回调与无摘穿刷新修正（待实机）

**验证状态（1.5.28）**：尚未实机验证，不能声称已实机修好。生产函数路径回归 6/6 通过；最终 Release BUILD_EXIT=0，DLL 版本/哈希与交付副本一致。包内逐文件清单见 SHA256SUMS.txt；最终 zip 大小、哈希及解包复核记录见包外 VERSIONS.md 与同名 .zip.sha256，避免自引用。

- **修正资源路径**：1.5.27 日志里的模型路径已经带 `meshes` 前缀，`TriFileExists()` 又加一次，实际探测成 `meshes\meshes\...`；1.5.28 规范化模型/TRI 路径，资源打开路径只保留一次 `meshes` 前缀。
- **收紧 BODYTRI 诊断**：不再只按 extra data 名称匹配；必须是 `NiStringExtraData`、路径非空且对应资源可打开才计数。日志改为 `has readable BODYTRI resources on N of M...`，并明确 `paths only, not proof of TRI shape matching or morph movement`。N/M 只是路径资源可读性，不是 TRI shape 名匹配、顶点移动或实时跟随的成功证据；即使 N=M 也不能宣告修好。
- **分离模型缓存**：以实际 addon（ARMA）+ sex + 1P/3P 区分缓存；1P 选择 `bipedModel1stPersons`，3P 使用第三人称模型，避免多 ARMA、性别与视角之间串用路径。
- **保留有效回调**：去掉会误吞所有 biped 回调的 `IsIllusionClone` 过滤，只用 `selfWrite` 防止自家写入重入。provider 调用前采集几何签名，再作调用后比较，避免把已更新的状态当基线。
- **自动刷新不摘幻化**：自动 morph 刷新不再 `CycleIllusion` 摘掉再穿回，而排队 `RequestBake`；不改基础幻化钩子、不清身体槽，也不以重新穿脱作为自动兜底。
- **共享 extra data 安全性**：规范化已有 BODYTRI 时，不原位改写共享字符串；为该节点创建新的 `NiStringExtraData` 替换，持有 `NiPointer` 保留原对象，替换失败时恢复。`GetBodyMorphs` 等未使用虚槽的声明签名纠正，虚表槽顺序不变。
- **能力边界**：主机制仍依赖 RaceMenu 自己的 morph 遍历能访问克隆件、TRI shape 能匹配。拿到且通过版本门禁的接口时才可额外重放；无接口重放不能保证实时，轮询/路径可读日志都不能替代实机顶点与外观验证。

### 握手根因与公开配置修法

SKSE64 2.0.20 的 `RegisterListener(nullptr)` 只订阅注册当时已有的 sender 列表（含当时预留槽），不是对未来 sender 的持续订阅；skee 的 `[Features] bEnableEarlyRegistration=1` 若使其过早注册，可能漏掉后加载的 SOS。`QueueProviderRetry()` 只重发消息，不能补建 skee 对 SOS 的订阅关系。公开配置修法：检查**实际游戏生效的** `Data/SKSE/Plugins/skee64.ini`，若 `[Features] bEnableEarlyRegistration=1`，由用户改为 `0` 并完全退出、重启游戏；不自动修改，也不打包用户 ini。当前没有本次 `skse64.log`，不能断言本次配置必为 `1`。协议依据：[SKSE v2.0.20 PluginManager.cpp L488–616](https://github.com/ianpatt/skse64/blob/v2.0.20/skse64/PluginManager.cpp#L488-L616)。

> **1.5.28 纠正（历史说明，不作当前验收依据）**：旧文档将握手归因于“仅消息嵌套”/receiver 名字错位的解释已纠正，实际是注册时机导致 sender 订阅遗漏；主线程 Queue 重试并非完整修法。旧 `carries a .tri morph target on N of M`、`N > 0` 或 `N = M` 判定均不充分，不能证明 TRI shape 匹配或 morph 顶点移动；旧实时成功措辞是当时目标/假设而非实机结论。当前以 1.5.28 最新章节为准。

## [1.5.27] - 2026-11-17

> **1.5.28 已纠正**：以下为历史诊断与预期，并非实机成功结论。旧“仅消息嵌套”/receiver 名字错位解释已纠正为注册时机的 sender 订阅遗漏；Queue 重试不能补订阅。旧 N/M 目标计数判定不充分，不证明 TRI shape 匹配或 morph 顶点移动；请以本页最新 1.5.28 说明为准。

### 修复：一个 `.tri` 目标都没贴上，幻化件照旧不跟随身形（m02992）

**症状**：1.5.26 装上后行为完全没变 —— `showracemenu` 拖 Body Morphs 滑块、OBody 换预设、SLIF 膨胀时幻化件纹丝不动。

**日志证据**（`bug/SkyrimOutfitSystemNG.log`，432,709 B / 5,046 行 / v1-5-26-0 / runtime 1-5-97-0）：`MorphRefresh.cpp:1212 MorphRefresh: 00000014 has no BodyMorph interface; 0 .tri target(s) of 5 slot(s) stay armed …` 出现 5 次、`0 .tri target(s) of 6 slot(s)` 1 次；整份日志 **0 条** `carries a .tri morph target on`、**0 条** `re-morphed`。同一份日志里 5 次 `the provider pass left the meshes of 00000014 alone; taking the illusion off for one engine pass.`（摘幻化 = 闪原版）。

**真因**：1.5.20–1.5.26 的 `BodyTriPathFor()` 用 `armor->GetArmorAddon(race)` 取 ARMA 再推 `.tri`。`TESObjectARMO::GetArmorAddon` 只回答「ARMA 的 `race` / `additionalRaces` 是否命中传入种族」（`TESObjectARMA::IsValidRace`）；自定义种族、套装没列该种族、或引擎自己走默认种族回退时它返回 null ⇒ 路径为空 ⇒ 连 `AttachBodyTri()` 都不调用 ⇒ 一整局一个 `BODYTRI` 都没贴上。而同一份日志的逐槽转储 `Custom: FE28F803 slot 01 item 1039FCD8 addon FE28F802 part 1 clone 0`（四件都是 `addon` 非 0）证明引擎自己解析得好好的 —— `biped->objects[i].addon` 就是那份 ARMA。

**修法**：
1. `BodyTriPathFor(armor, addon, race, female)` 改为优先用 `biped->objects[i].addon`（引擎已解析），`GetArmorAddon(race)` 只在没有 addon 时兜底；查不到 addon 时不写缓存（避免把该槽永久判死）；
2. 新增 `TriCandidates()`：一次生成 `<stem>.tri`（去 `_0`/`_1`）、`<stem>_0.tri`、`<stem>_1.tri`、原样 `<stem>.tri` 四个候选（本性别优先），逐个用 `BSResourceNiBinaryStream` 探测（与 RaceMenu `CacheFile` 同一问法）；
3. `NodeHasBodyTri(node)` 的计数移出「路径非空」分支：`.nif` 自带的 `BODYTRI` 也算目标；
4. 每个护甲/性别一条 debug 诊断：`… slot model <path> (from the armour addon the engine attached) uses .tri <file>.` / `… has no .tri beside it, so RaceMenu cannot morph that slot. Probed: a, b, c, d.`；
5. 新增 `morphPush::g_armed`：`PollOnMainThread()` 与 `VerifyProviderPass()` 发现签名变化时，若该角色的克隆件已武装就只重设基线、不再 `CycleIllusion`（消除 1.5.26 那 5 次摘幻化的闪烁）；
6. 启动 info 文案改为 `every illusion clone whose armour model ships a .tri gets that .tri as a morph target; …`。

**判定**：切一次幻化套装后，日志里每个幻化件一条 `uses .tri <file>`，该角色第一次的 `carries a .tri morph target on N of M cloned slot(s)` 里 `N` 应等于幻化覆盖槽数（0x00010086 那套是 4）；拖滑块/换预设/膨胀应实时跟随，且不再出现 `taking the illusion off for one engine pass`。若仍 `N = 0`，看 `has no .tri beside it … Probed: …` 那行（含真实模型路径与试过的四个名字）。

**未验证**：本机无 Skyrim，实机待测。若某套幻化件的 `.tri` 目录/名字完全不在这四个候选里，该件仍不会跟随（日志会明说）。
## [1.5.26] - 2026-11-17

> **1.5.28 已纠正**：以下为历史诊断与预期，并非实机成功结论。旧“仅消息嵌套”/receiver 名字错位解释已纠正为注册时机的 sender 订阅遗漏；Queue 重试不能补订阅。旧 N/M 目标计数判定不充分，不证明 TRI shape 匹配或 morph 顶点移动；请以本页最新 1.5.28 说明为准。

### 修复：幻化件不跟随 RaceMenu 的 Body Morphs（m00002 / m00270 / m02536）

**症状**：幻化本身正常（1.5.25 已修），但 `showracemenu` 里拖 Body Morphs 滑块、OBody NG 换身形预设、SLIF NG 加膨胀时，**幻化件的身形不动**，只有真皮肤在变；必须重穿幻化甚至先取消幻化再穿上才能看到新身形。

**真因一（接口拿不到）**：1.5.20–1.5.25 把「先拿到 skee 的 `BodyMorph` 接口」当成功能前提 —— `BakeIllusion()` 开头就是 `if (!g_api || !a_actor || !a_actor->Is3DLoaded()) return kSkipped;`，`NotifySlotApplied()` 也在 `!g_api` 时直接返回。而用户机器（SE 1.5.97）的 `skse64.log` 显示：我们的 `0x9E3779B9` 接口交换广播**恒投给 24 / 35 / 39 / 45 / 66 / 91 / 95(OBody) / 140** 这 8 个插件，14 次尝试 14 次一模一样，从不包含 132(skee) 与 99(OStim)；这 8 个恰好是「在 `kPostLoad` 投递循环**中间**才注册 catch-all 监听者」的插件，而 skee 是**加载期**注册的。连定时器任务里发起、外面没有任何 dispatch 在跑的那一次，收件集也相同 ⇒ 与线程、嵌套无关，是监听者的注册时机。结论：这套加载顺序下不能把「拿到接口」当作功能前提。

**真因二（`.tri` 跟随本来不需要接口）**：skee 的 `BodyMorphInterface.cpp` 里，`Impl_ApplyBodyMorphs` → `MorphCache::UpdateMorphs(refr)`（718–741 行）用 `VisitBipedNodes` 遍历每个 biped 槽的 `data.object` 并逐个调 `ApplyMorphs()`，后者（690–716 行）在该子树里找名字为 `BODYTRI` 的 `NiStringExtraData`，找到就应用。RaceMenu 拖滑块调的正是 `ApplyBodyMorphs`，OBody 换预设也走同一条 ⇒ **克隆件只要带 `BODYTRI`，RaceMenu 自己就会 morph 它**。第一次被 morph 时 skee 还会自己建 `SHAPEDATA` 基准（541–547 行），不需要我们造。

**真因三（兜底探针采不到蒙皮网格）**：1.5.18 起的几何签名只认 `BSDynamicTriShape` + `dynamicData`，而身体和护甲都是 `BSTriShape` + `NiSkinPartition`，所以 250 ms 轮询在拖滑块时从不触发（1.5.25 日志里滑块期间零触发即此因）。

**修法**：
1. `BakeIllusion()` 去掉 `!g_api` 门禁：无接口也推导 `.tri` 路径并 `AttachBodyTri()`，只有 `ApplyVertexDiff()` 留在 `if (g_api)` 里；返回 `kBaked` 的条件改为「贴了目标 或 重放了 morph」；
2. `NotifySlotApplied()` 不再因 `!g_api` 提前返回，皮肤钩子每次贴完件都排队一次 bake；
3. 每角色一条 info 证据：`MorphRefresh: <FormID> carries a .tri morph target on N of M cloned slot(s) (…); RaceMenu and OBody morph those clones in their own apply pass - no interface needed.`；
4. 握手改到主线程任务：新增 `QueueProviderRetry()`（`std::atomic<bool>` 去重 + `SKSE::GetTaskInterface()->AddTask`），`Install()` 与 `EnsureProvidersForUse()` 都改走它（后者原来在皮肤钩子里同步 `Dispatch`，就是日志里夹在 `ShouldOverrideSkinning` 中间那两条 `Failed to dispatch message to skee64` / `to skee`）；
5. 新增 `HashSkinnedShape()`：读 `BSGeometry::skinInstance->skinPartition->partitions[0].buffData->rawVertexData`（绑定姿势顶点，动画不影响），接在 `HashDynamicShape()` 之后；
6. 改写 `TryRaceMenu()` 里那段「SKSE 2.1.2+ 按发送方位置解析 receiver 名」的错误注释，并明确「其它逻辑不得依赖该接口」。

**判定**：幻化一套带 `.tri` 的衣服，RaceMenu 拖滑块 / OBody 换预设 / SLIF 膨胀三种情况幻化件都应实时跟随，不闪原版护甲；日志出现 `carries a .tri morph target on N of M cloned slot(s)`（每角色一次，info），`N < M` 说明有幻化件的 `.tri` 推导不到。没有接口时 `stay armed for RaceMenu's own pass` 的 debug 行属正常。

**未验证**：本机无 Skyrim，实机待测。已知边界：没有 `.tri` 的护甲依旧不跟随；模型名不符合「去掉 `_0`/`_1` + `.tri`」约定时推导不到。

## [1.5.25] - 2026-11-17

### 修复：幻化件能显示了，但身体（32 槽）永远是空模型（m02242）

**症状**：1.5.24 装上后幻化衣服确实出现，但人物只剩头和手；在物品栏里把 32 槽（身体）护甲脱掉，身体仍是空模型；切换到空的幻化套装也一样。日志里 14:42:11 出现 `Custom: 1 slot(s) left the display set (mask 0x00000004); cleared for 00000014.`（`0x4` = 内部位 2 = CK 槽 32 = 身体），且全程没有一条 `ApplyArmorAddon refused`。

**真因**：1.5.24 在 `CustomOne` 里对「上一次在幻化集、这一次不在」的槽调用 `ClearSlots` → `RE::BipedAnimAugments::ClearBipedPart(anim, &objects[bit], true, 0)`，并把 `objects[bit].item / addon / part` 三个字段置空。`BipedAnim::objects[bit]` 是引擎给该槽维护的唯一条目（含引擎自己挂的皮肤/身体部件、ARMA 解析结果与几何）；清空后只要装备栏没有变化，引擎不会再走 `ApplyArmorAddon`，槽就永久空着 —— 脱 32 槽护甲、换空套装都救不回来。上游 1.5.73（以及 1.5.17）的 `Custom` 从不清空任何槽、也不还原真甲，所以 1.5.17 正常。

**修法**：
1. 删除 `ClearSlots()` 与 `SlotClone()`（`Patches.cpp` 里已无任何 `ClearBipedPart` 调用）；
2. 残留槽只打日志：`Custom: N slot(s) left the display set (mask 0x…); left to the engine for …`，槽完全交回引擎；
3. 新增 `LogSlotState(actor, model, armor, bit)`：对每个刚贴过的槽打 debug 行 `Custom: {:08X} slot {:02} item {:08X} addon {:08X} part {} clone {}`（`addon 0` = ARMA 未解析，`part true` = 几何已建）；
4. `ModelBiped` 保留（`LogSlotState` 用它从 `ActorWeightModel` 取 `BipedAnim*`，仍只接受该 actor 的两套 biped 之一）。

**判定**：切套装 / 脱 32 槽护甲 / 切空套装后身体始终是原版皮肤，日志只出现 `left to the engine`，不再出现 `cleared for`。

**未验证**：本机无 Skyrim，实机待测。已知副作用：切到更小的套装后，不再被接管的槽会短暂保留上一套的克隆，直到引擎因换装/脱装重贴该槽（不会出现空模型）。

## [1.5.24] - 2026-11-17

### 修复：1.5.23 装上后「只显示头手、其它槽全空」，切套装时原装备 46 槽外露（m02035）

**症状**：五个钩子全部装上、幻化确实在跑（`Custom: rebuilding 5 display pieces (mask 0x00002186, …)`），但五件全是 `Custom: FE2D6D6B slot 01 clone 0x0` —— 一件都没贴上，人物只剩头和手；切换幻化时还出现 `Custom: 1 slot(s) left the display set (mask 0x00010000); restored 1 real piece(s)`，即原装备的槽 46 被贴了回来。

**真因一（贴不上）**：1.5.20 起把贴件入口改成只传 `InventoryChanges*`，模型在 `Custom(actor, nullptr)` 里用 `reinterpret_cast<RE::ActorWeightModel*>(&actor->GetBiped(fp))` 造出来。`RE::ActorWeightModel` 在 CommonLibSSE-NG 里只有前向声明，真实布局未知，`ApplyArmorAddon` 读到错位字段，于是每件都建不出克隆。1.5.17 用的是引擎传进来的寄存器：`rbx` = 被蒙皮对象、`rdi` = 引擎填好的 `ActorWeightModel*`（上游 `Hooks_PRE_AE.cpp:152-197`）。

**真因二（46 槽外露 + 越刷越糟）**：1.5.20 起 `fullRebuild` 先 `ClearSlots(displayMask)` 再重贴整份 displaySet（含已经由原版通道贴好的真实装备件，清空重贴在 1.5.14 就被证伪）；旧槽离开 displaySet 时还会「还原真甲」把真实护甲贴回去。

**修法**：
1. 壳代码回到 1.5.17 的寄存器契约：`push rcx/rdx` → `call ExecuteVisitorOnWorn` → `mov rcx, rbx` → `call ShouldOverrideSkinning` → `mov rcx, rbx; mov rdx, rdi` → `call Custom` → 恢复 → `jmp site+5`；
2. `CustomOne` 只贴 displaySet 里**未真实穿戴**的件（与上游一致）；
3. 不再 `ClearSlots(displayMask)`：换件交给 `ApplyArmorAddon` 自己重建；
4. 离开 displaySet 的槽只清空、不还原真甲；
5. `ClearSlots`/`SlotClone` 改用 `ModelBiped(actor, model)`：只接受「该 actor 两套 biped 之一」，否则 warn 一次并跳过；
6. `ApplyArmorAddon` 返回 false 时打 5 秒节流的 warn（`Custom: ApplyArmorAddon refused …`），不再静默留空槽。

**判定**：日志里 `Custom: … slot NN clone 0x…` 必须是非 0 指针；切套装后指针应变化。

**未验证**：本机无 Skyrim，实机待测。

## [1.5.23] - 2026-11-17

### 修复：1.5.20 起底层幻化乱套 —— 恢复 1.5.17 的贴件站点（`CustomSkinPlayer` 扫不到时回退历史偏移）

- **症状（m01836 / m01837）**：用户明确纠正「1.5.17 都测过了，全部功能正常，只有 BodyMorphs 实时形变的问题」「1.5.20 版本后你底层的幻化都乱套了」——1.5.20 全裸、1.5.21 幻化整体关闭、1.5.22 扫不到调用点仍然拒绝安装。
- **真因**：1.5.20 给 `CustomSkinPlayer` 增加了「站点必须是 `E8` 且 `rel32` 精确指向 `InventoryChanges::ExecuteVisitorOnWorn`」的校验（日志：`the call at +0x81 targets 0x7FF66C1610E8, but 0x7FF66C3651D0 was expected`）。1.5.17 的 `ResolveSite` 只检查站点首字节是 `E8` / `E9` / `90`，不校验目标，因此在用户这台 1.5.97 上照样装上并**全功能正常** —— 被否掉的只是「目标必须等于那个函数」这个过强假设，站点本身（成员调用，`rcx` = `InventoryChanges*`）是有效的。1.5.21 的 ±0x800 自定位与 1.5.22 的纯扫描都在同一错误前提上继续加码。
- **修法一（新增 `Hooking::FindCallInFunctionOrOffset`）**：先走 1.5.22 的扫描（宿主函数入口起 `0x800` 内找 `E8` 且目标精确命中，或经一跳 thunk 到达）；**扫不到时**回退到 `entry + legacyOffset`，仅当「在 `.text` 内」且「该处是 `E8` 开头的 5 字节 call」才安装，并 warn `… the site this hook has used since 1.0.0 - rva 0x… (+0x… into the function) - is still a 5-byte call; the hook is installed there.`；都不是则保持 1.5.22 的 error（模块身份 + 全 `.text` 调用统计 + 前 5 个候选），该钩子放弃、不 patch、不崩。
- **修法二（只给这一个钩子历史偏移）**：`const std::ptrdiff_t kLegacyOffset = REL::Module::IsAE() ? 0x1EF : 0x81;`，其余四个钩子（`DontVanillaSkinPlayer` / `ShimWornFlags` / `FixEquipConflictCheck` / `FixSkillLeveling`）仍是纯扫描，不引入偏移。
- **修法三（门禁恢复成对）**：`illusionAvailable = g_dontVanillaSkinPlayerInstalled && g_customSkinPlayerInstalled` —— 只装上「藏真甲」一半时整条幻化关闭（`SetIllusionDisplayEnabled(false)`，原版流程照跑，**显示真实护甲而不是裸体**），日志 critical `Illusion display is off for this session: …`；两半都在才 info `Illusion display is active: the player skinning hooks are installed.`。1.5.22 那条「display set is attached from the deferred pass」的 info 已删除（实机证明只有藏真甲时看到的是裸体）。

### 兼容性

- 仍是**一份实现 + 一张 `REL::RelocationID(seID, aeID)` 表**；`REL::Module::IsAE()` 只用于「注入机器码本身不同」的两处（`DontVanillaSkinPlayer` 的 actor 寄存器、`FixSkillLeveling` 的 AE 内联分支）与这里选历史偏移。
- 新区（AE 1.6.1170+ / 1.7.x）：函数体内能扫到那条 call 时走扫描，历史偏移不参与；只有扫不到才用 1.0.0 起的历史站点，且仍要求那里是 5 字节 `E8`。
- 不动幻化数据流、`.tri` morph 跟随（1.5.20 起的 MorphPush / skee v5 `MorphShapeCallback` 订阅）、PAD / AND 兼容、菜单与 Papyrus 接口。

### 未验证

- 本机无 Skyrim，未实机验证。实机看两条日志：`CustomSkinPlayer: the function at rva 0x364280 has no call to 0x… within 0x800 bytes, but the site this hook has used since 1.0.0 - rva 0x364301 (+0x81 into the function) - is still a 5-byte call; the hook is installed there.` 与 `Illusion display is active: the player skinning hooks are installed.`
- 若日志是 `… and the legacy site rva 0x… is not a 5-byte call either`，说明这份 exe 连历史站点都不是 `E8`，需按真实反汇编重新定位。

## [1.5.22] - 2026-11-17

### 修复：幻化依旧失败 —— 钩子站点改为「地址库定函数 + 函数体内扫描」，不再有任何每运行时字节偏移

- **症状（m01170）**：1.5.21 装上后幻化依旧不显示，用户明确纠正：「这项目是要兼容AE以及最新版本的，让你在CommonLibSSE-NG基础上改不是让你专门去钩1597」。1.5.21 的 ±0x800 自定位虽然能兜住一部分情况，但它仍是在一个**写死的偏移**周围找替身，对别的运行时没有意义。
- **真因（偏移来自 1.5.73，不是 1.5.97）**：`+0x302` / `+0x7C` / `+0x81` / `+0x97` / `+0x52` 全部抄自上游 `Hooks_PRE_AE.cpp` 里 1.5.73 的注释。地址库只保证**函数入口**，函数体内布局随编译器/内联变化。实测 `24231` 在 1.5.97 的入口是 rva `0x364280`，而下一条地址库条目只比它大 `~0xD0`，`+0x81` 落在函数中部却是垃圾字节（`E8 E2 CD C7 FF` 的目标在模块外）。
- **修法一（地址库定函数，不再存偏移）**：新增 `Hooking::FindCallInFunction(hook, hostId, targetId)` —— `REL::RelocationID(seID, aeID)` 给出宿主函数入口与目标函数地址，在 `[entry, entry + 0x800)` 内逐字节扫 `E8`，`rel32` 精确等于目标（或经一跳 thunk 到达）即候选。`SiteOffset()` 已删除，`Hooks.cpp` 里不再有任何 `+0x81` 之类的常量。窗口 `0x800` 取「已知最深站点 `0x373`」的余量。
- **修法二（候选处理与自证）**：1 条命中 → 采用，debug 记 `the call to 0x… sits at rva 0x… (N byte(s) into the function at rva 0x…)`；多条 → 钩第一条并 warn `calls 0x… N times; hooking the first one at rva 0x….`（其余可能属于相邻函数，寄存器语义无法保证）；0 条 → error `does not call 0x… within 0x800 bytes; the hook stays disabled so the game keeps running.` + 模块身份 + 全 `.text` 调用数与前 5 个 `call at rva`，放弃该钩子而不是崩。
- **修法三（去掉寄存器假设）**：`ShimWornFlags` 与 `CustomSkinPlayer` 的壳代码只保存/恢复 `rcx`（成员调用点它必然是 `InventoryChanges*`），C++ 侧用 `inventory->owner` 取 actor；`ShimWornFlags` 在幻化关闭 / 无套装 / 非 owner 时直接 `return a_inventory->GetWornMask()`（原版掩码，旧代码这里返回 0）。`DontVanillaSkinPlayer`（actor 在 `r13`）与 `FixEquipConflictCheck`（`rdi` / `rbx`）保留 —— 这两处是调用约定里的固定寄存器，不随版本漂移。
- **修法四（钩不到也能贴幻化件）**：`CustomSkinPlayer` 在某些构建里没有可钩的调用点，新增延迟附加路径：`DontVanillaSkinPlayer::ShouldOverride` 每次要藏真甲就 `MarkAttachPending(actor)`，按 FormID **100 ms** 节流入队，`SKSE::GetTaskInterface()->AddTask` 在主线程 `DrainAttachPending()` → `Custom(actor, nullptr)`；`Custom` 的 model 为 null 时对 `{false, true}` 两套 biped 各调一次 `CustomOne`，指针取 `&actor->GetBiped(fp)`（与引擎传入的 `BSTSmartPointer<BipedAnim>` 持有者地址相同）。壳钩子装上时走快路径、完全不进队列。
- **修法五（门控放宽，只卡「藏真甲」）**：`illusionAvailable` 只看 `DontVanillaSkinPlayer` 是否装上；`CustomSkinPlayer` 缺失不再等于幻化关闭。日志三分支：`Illusion display is active: the player skinning hooks are installed.` / `Illusion display is active: the hiding hook is installed and the display set is attached from the deferred pass.` / critical off（显示真实护甲）。
- **兼容性（这正是用户要的那条）**：五个钩子仍是同一份 `Hooks.cpp` + `REL::RelocationID(seID, aeID)` 单表；`REL::Module::IsAE()` 只出现在注入机器码本身不同处（`DontVanillaSkinPlayer` 取 actor 的寄存器、`FixSkillLeveling` 的 AE 内联分支）。SE 1.5.97 / AE 1.6.1130–1.6.1179 / 1.7.x 走同一条路径，没有第二份实现、没有第二张 ID 表、没有 per-runtime 偏移。
- **不受影响**：幻化数据流（`compute_display_set_c` / `ClearSlots` / 诚实还原）与 1.5.20 的 `.tri` morph 跟随（`MorphShapeCallback` / `Obody_ApplyMorph`）未改；PAD / AND 兼容、菜单、Papyrus 接口未改。
- **未验证**：本机无 Skyrim。按代码推断用户 1.5.97 上四个钩子必中、`CustomSkinPlayer` 走延迟附加；即便如此也不裸体（延迟路径会贴件）。要看的日志见 `docs/TESTING.md` §3.35。
## [1.5.21] - 2026-11-17

### 修复：幻化彻底不显示、角色只剩裸体（CustomSkinPlayer 钩子验证失败 → 改为自定位 + 失败降级）

- **症状（m00746）**：换上 1.5.20 后幻化基础功能全没了 —— 菜单里换哪套都不显示，角色直接裸体；游戏不崩，其它功能（菜单、存档、脚本）正常。日志（`bug\SkyrimOutfitSystemNG.log`，2062 行，`v1-5-20-0`，runtime `1-5-97-0`）里只有一条 error：`HookSupport.cpp:104: CustomSkinPlayer: the call at +0x81 targets 0x7FF66C1610E8, but 0x7FF66C3651D0 was expected; the hook stays disabled so the game keeps running.`，紧跟着 `HookSupport.cpp:55: CustomSkinPlayer: site rva 0x364301 -> E8 E2 CD C7 FF`；其余四个钩子（vanilla player skinning / shim worn flags / FixEquipConflictCheck / FixSkillLeveling）全部 `verified ...` 并安装成功。
- **为什么一条钩子失败 = 全裸**：`DontVanillaSkinPlayer` 对「不在幻化 displaySet 里的已穿护甲」返回 true，引擎于是**跳过** `TESObjectARMO::ApplyArmorAddon`——真护甲被藏起来；而幻化件不是玩家真实穿戴物，只有 `CustomSkinPlayer::Custom()`（先 `ClearBipedPart` 再 `ApplyArmorAddon` 造 clone）会把它们贴上去。前者装上、后者缺席 ⇒ 真护甲被藏、幻化没人贴 ⇒ 裸体。
- **为什么站点对不上（地址库没问题，是那处字节不是这条 call）**：`E8 E2 CD C7 FF` 解出的调用目标是 `base - 0x1EF18`（模块外，不可能是真的 call 目标）；而地址库 ID 24231 解析出的 `0x364280 + 0x81 = 0x364301`、ID 15856 解析出的 `0x1E51D0` 都与代码里的期望值完全一致，同一函数区的另外两个钩子也能对上。结论：**站点算法没错，是这个 exe 在该函数内部的布局与记录偏移不同**（该处不是那条调用）。本机没有游戏本体，无法逐字节复现，所以不再靠"把偏移猜死"，改为运行时自定位 + 失败降级。
- **修法一（自定位，1.5.21 核心）**：`Hooking::ResolveSite()` 新增两个可选参数（期望目标地址、扫描窗口）。目标不符时在站点 ±窗口内逐字节找 `E8` 且 `rel32` **正好落在期望函数**的调用点：恰好一个 → 记 warn（`moved by N bytes; hooking the relocated site`）并采用；两个以上 → 拒绝并列出每个候选的 rva/距离（不知道哪个调用点进入时目标寄存器是 actor，钩错会带错寄存器状态）；窗口内没有、但全 `.text` 只有**唯一一处**调用期望函数 → 采用那一处（调用点唯一意味着它只能是这条路径）；都没有 → 放弃该钩子。只有 `CustomSkinPlayer` 传窗口 `0x800`，其余钩子维持原来的精确比对（今天已验证正确，不做推断）。
- **修法二（失败降级：宁可没幻化，也不能裸体）**：新增 `Hooking::SetIllusionDisplayEnabled(bool)` / `IllusionDisplayEnabled()`（`std::atomic_bool`，默认 false，fail-closed）。`ApplyPlayerSkinningHooks()` 装完钩子后按「`DontVanillaSkinPlayer` 与 `CustomSkinPlayer` 是否都装上」置位；没都装上就保持关闭，`ShouldOverrideSkinning()` 恒返回 false —— 原版皮肤流程原样执行，**玩家看到的是真实护甲**，同时日志打一行 `critical` 说明幻化本局关闭、其它功能不受影响。
- **修法三（防错钩崩游戏）**：`ShouldOverrideSkinning()` 在 `skyrim_cast` 之前用 `VirtualQuery` 检查目标指针（`MEM_COMMIT`、非 `PAGE_NOACCESS`/`PAGE_GUARD`、8 字节对齐、区间够 0x20）。自定位万一挑中了不合适的调用点，这里会把垃圾指针挡掉并记 warn，而不是崩在 RTTI 上。
- **修法四（诊断，下一版能直接改对偏移）**：装钩子前先记一行 `Skinning hooks: SkyrimSE.exe <版本>, <字节数> bytes, written <时间>, base 0x..., .text rva 0x... + 0x...`；失败时额外打站点 `±0x10` 的十六进制窗口，并统计全 `.text` 到期望函数的调用点总数 + 最近 5 个的 rva 与距离。下次只要把这几行发来，就能判断是偏移漂移（自定位会自己修好）、exe 被改过（版本/大小/时间会对不上）还是 Address Library 不对。
- **不受影响**：1.5.20 的 `.tri` 目标与 morph 事件驱动重放（m00002 / m00270）原样保留；幻化关闭时它们自然也不工作（没有幻化可跟随）。
- **未验证**：本机无 Skyrim，未实机验证；用户环境是否能自定位成功取决于那份 exe 附近是否存在唯一一条到 `ExecuteVisitorOnWorn` 的调用（日志 `hooking the relocated site` / `hooking that one` 即成功）。无论成功与否，"裸体"都不会再出现：最差情况是列出候选、关闭幻化、显示真实护甲。

## [1.5.20] - 2026-11-17

### 修复：幻化件根本不跟随 RaceMenu BodyMorphs（幻化网格没有 .tri 目标，RaceMenu 写不到它）

- **症状（m00002 / m00270）**：①在 RaceMenu 窗口里拖 Body Morphs 滑块，裸体外观生效，换上带 `.tri` 的幻化件后身形卡住不动；②OBody NG 换身形预设、SLIF 膨胀同样不跟随。日志（1.5.18，5323 行）里一次预设变化是：`taking the illusion off`（00:00:59.459）→ `restored`（00:01:00.090）→ `rebuilding ... after the body morph provider settled`（00:01:00.371），中间 **631 ms 真人穿着原版护甲**；滑块拖动期间则一条 MorphRefresh 记录都没有。
- **真因（引擎层，不是调参能解决的）**：幻化 clone 是 `CustomSkinPlayer::Custom()` 里先 `ClearBipedPart` 再用 `TESObjectARMA::ApplyArmorAddon` 直接造出来的，**不经过 skee 的 `AttachBipedObject` 钩子**（skee64 `SKEEHooks.cpp` 在引擎挂载槽位后调 `ActorUpdateManager::OnAttach` → `BodyMorphInterface::OnAttach` → `ApplyVertexDiff`）。引擎正常挂载护甲时会顺带给节点生成 `BODYTRI` 额外数据（值是相对 `meshes\` 的 `.tri` 路径），而 skee 的 `MorphCache::ApplyMorphs()` 只在子树里找到 `BODYTRI` 时才读 `.tri`、才写顶点 —— **没有就整棵子树跳过**。所以幻化 clone 上永远没有 morph 目标：无论 RaceMenu / OBody / SLIF 调多少次 `ApplyBodyMorphs`，写的都是真实装备的网格，幻化的那层纹丝不动。1.5.19 的 provider-first（签名一动就替它再 apply 一次）因此注定看不到任何顶点变化，只能回退幻化循环 —— 也就是那 631 ms 的露馅。
- **修法一（补上 .tri 目标，根治）**：新增 MorphPush 子模块（`src/cpp/MorphRefresh.cpp`）。幻化每次重建后由 `MorphRefresh::NotifySlotApplied()`（`Patches.cpp` 在每个 clone `ApplyArmorAddon` 之后调用）排队一次主线程任务，对每个被幻化覆盖的槽位：按 OBody 的规则（`Body.cpp` `CacheSkinMorphFiles`）从该护甲 ARMA 的 `bipedModels[性别].GetModel()` 推导 `.tri` 路径 —— 去掉 `.nif`、去掉 `_0`/`_1` 性别后缀、加 `.tri`（先试去掉后缀，再试保留），并用 `BSResourceNiBinaryStream` 实测文件存在（带护甲 FormID + 性别缓存，没有 `.tri` 的部件静默跳过）；子树里若还没有 `BODYTRI`，就挂一个 `NiStringExtraData`。value 字符串用 `RE::malloc` 从**引擎堆**分配（插件是 `/MT` 静态 CRT，`new char[]` 会被另一个分配器释放）。
- **修法二（改成事件驱动，不再等轮询）**：skee v5 的 `IBodyMorphInterface::AddMorphShapeCallback`（`IPluginInterface.h`：RaceMenu 改完顶点缓冲、拷贝到其它 partition 之前触发）—— 回调参数里的节点若是我们自己的幻化 clone（已跟随）就直接忽略，否则标脏；OBody 的 `NotifyMorphApplied` 会广播 `Obody_ApplyMorph`（`Body.cpp`），用 SKSE `ModCallbackEvent` 事件槽订阅。回调里**只标脏**，真正的重写在主线程任务里做：`MorphCache::ApplyMorphs` 持着自己的锁触发回调，回调里回调 skee 会死锁。
- **修法三（重写顶点，而不是重建）**：主线程任务对每个覆盖槽位调 `ApplyVertexDiff(actor, node, false)`。`erase=false` ⇒ `isAttaching=false` ⇒ skee 先用 `SHAPEDATA` 里的原始顶点把网格还原、再重放该角色当前的全部 morph（幂等，重复调用不会越叠越胖）。全过程不摘幻化、不重建、不换模型，任何一帧都不会露出原版装备。
- **修法四（不让自己写的顶点喂回轮询）**：新增 `AbsorbOwnWrite()` —— 重写完成后把结果当作新基线写回 `g_signatures`、清掉该角色的 `g_verify`、开 1.2 s 静默窗（`kSelfNoiseGrace`）。否则下一轮轮询会把我们自己的顶点位移读成「provider 动了」，再跑一次幻化循环，又闪回原版。静默窗内如果真有外部变化，仍由 1.5.19 的 `quietDrift` 逻辑在窗口关闭后补放。
- **修法五（ABI 与握手通道）**：skee `IBodyMorphInterface` 的本地声明补齐到 v5 的全部 23 个槽位 —— 原先 `ApplyVertexDiff` 少了 `rootNode` 形参（`erase` 会落进 `rootNode` 槽位）、`ApplyBodyMorphs` 之后整段缺失，直接在末尾追加 `AddMorphShapeCallback` 会落到错误槽位并越界读 vtable。`GetVersion() < 5` 时**不装**回调（v4 没有这一项），退回轮询，`.tri` 目标照旧补。`TryRaceMenu()` 依次试 `"skee64"`（DLL 名即插件注册名，6.0 之前一直只试 `"skee"` 必然 miss）→ `"skee"` → 无名广播，并记录最终是哪条通道应答。
- **新日志**：`MorphRefresh: RaceMenu BodyMorph v{} acquired through {}; the illusion follows .tri morph changes.`、`MorphRefresh: RaceMenu BodyMorph v{} hands out MorphShapeCallback; the illusion is re-morphed as soon as RaceMenu writes the vertices.`、`MorphRefresh: RaceMenu BodyMorph v{} has no MorphShapeCallback (v5 added it); the .tri morph of the illusion follows the poll instead.`、`MorphRefresh: .tri morph target support active|inactive (no RaceMenu BodyMorph interface yet) for illusion slots.`、`MorphRefresh: re-morphed {:08X} on N node(s), M new .tri target(s) (the illusion was rebuilt|the provider wrote new morphs); the vertices moved|were already current.`（每个角色最多每 5 秒一条）。
- **兼容性**：只走公开契约 —— skee 的 `IPluginInterface` v5 虚表、SKSE `ModCallbackEvent`、引擎自己的 `BODYTRI` 额外数据约定；不 hook、不 patch 任何第三方函数入口，skee / OBody / SLIF 升级后最坏情况是回调拿不到（自动退回轮询），不会崩、不会改变原 mod 行为。
- **已知边界**：本身没有 `.tri` 的护甲件没有可 morph 的顶点数据，行为与不装幻化时一致（现在不会再触发幻化循环，因此也不会再闪原版）；1P 模型的 `.tri` 沿用 `bipedModels` 推导，若某 mod 给第一人称单独配了 `.tri` 尚未覆盖。

## [1.5.19] - 2026-11-17

### 修复：三条身形症状 1.5.18 一条都没解决（skee 握手在进游戏前就被判死 + 几何探针恒 0 + OBody 换预设跑两套流程）

- **症状（m21255，附 `bug/SkyrimOutfitSystemNG.log`，443 KB / 5323 行，1.5.97）**：①空套装（裸体外观）拖 RaceMenu Body Morph 滑块「生效」，换上带 `.tri` 的幻化反而不跟随；②OBody NG 换预设变卡；③SLIF 膨胀未测。日志里三处决定性证据：`MorphRefresh.cpp:812 RaceMenu did not answer the interface exchange`（只在加载期出现一次）、`SLIF NG query interface v2 acquired (0 tracked actors)`、一次预设变化 `taking the illusion off`（00:00:59.459）→ `restored`（00:01:00.090）→ `rebuilding ... after the body morph provider settled`（00:01:00.371）。
- **真因一（握手 60 秒就判死）**：1.5.18 的重试是「12 次 × 5 秒、从 kDataLoaded 起算」，用户 23:57 加载、00:00:16 才进游戏 —— 窗口在进游戏之前就耗尽，RaceMenu 即便晚答也永远拿不到。而 SLIF NG 作者在 `src/Skee.cpp:442-459` 记录过：1.5.97 的 skee 只从自己的某个消息处理器里才注册监听器，kPostPostLoad 时是否已监听取决于加载顺序，所以握手必须「kPostPostLoad / kDataLoaded / 首次 apply 前」各试一次、永不放弃，失败时还要打印 skee64.dll 是否在进程内以区分「没装」与「装了但不答」。
- **真因二（几何探针恒 0）**：1.5.18 的 `ComputeGeometrySignature` 只对 `objects[i].partClone` 本身调 `AsDynamicTriShape()`，而 `partClone` 多数情况是容器节点（NiNode / add-on node / skin wrapper），41 个槽全失败 → 签名里从来没有几何项 → provider 拿不到时没有任何信号。这也解释了「空套装看起来生效」：那是引擎自己在重刷真实裸体。
- **真因三（OBody 变卡）**：一次预设变化走了两套完整流程 —— 幻化循环（摘掉 + 穿回，日志实测 631 ms 期间真人穿原版护甲）**加上** 700 ms 后又一次全量重建。
- **修法一（握手永不放弃）**：heartbeat 前 8 次每 5 秒、之后每 30 秒一直重试到整个会话结束；`kPostLoadGame` 分支再试一次；真正要用接口之前（每次触发变化时）惰性再试（最短间隔 2.5 秒）。`RetryProviders()` 不再有次数上限。
- **修法二（诊断）**：新增 `ReportRaceMenuSilence()` —— 探测 `skee64.dll` / `skee.dll` / `skeevr.dll` 是否在进程内，每 5 分钟最多报一次（上限 8 次），文案区分「在进程里但没交接口」与「根本没有 skee 模块（RaceMenu 没装）」。
- **修法三（探针递归）**：新增 `HashSubtree()` 递归遍历 `partClone` 子树（深度上限 4、每槽 24 个形状、每槽单独累加后再按槽位序号混合），容器节点里的 `BSDynamicTriShape` 终于能被采到。
- **修法四（provider-first，省掉两套流程）**：签名变化时先直接调 `ApplyBodyMorphs(actor, true)`（RaceMenu）或 `ApplyOBodyMorphsToActor(actor)`（OBody），350 ms 后比对签名；因为 RaceMenu 是延迟提交、OBody 是排队执行，未变化时最多再等 4 × 350 ms，仍不动才回退幻化循环（循环仍是唯一能让引擎重新加载 `.tri` 目标的路径）。成功路径完全不动幻化 → 不露原版装备、不跑两次全量刷新。
- **修法五（删掉二次重建）**：删除 1.5.18 的 `kSettleRebuildDelay = 700 ms` 补建登记（日志里 +280 ms 那次多余全量刷新由此而来）。
- **新 ini 键**：`[Compatibility] MorphProviderFirst = 1`（默认开；设为 0 恢复「每次都走幻化循环」的旧行为）。
- **新日志**：`MorphRefresh: <dll> is in the process but RaceMenu still has not handed out the BodyMorph interface; kept asking (N tries). ...`、`MorphRefresh: no skee module in the process - RaceMenu is not installed. ...`、`the body morphs of <FormID> changed; following them.`、`the provider pass moved the meshes of <FormID>; the illusion followed without a cycle.`、`the provider pass left the meshes of <FormID> alone; taking the illusion off for one engine pass.`。

## [1.5.18] - 2026-11-16

### 修复：身形变化的三条实机症状（RaceMenu 滑块不跟随 / OBody 换预设变卡 / SLIF 膨胀不跟随）

- **症状（m20924，两台机器的日志）**：①幻化空套装（裸体外观）时拖 RaceMenu Body Morph 滑块生效，换上带 `.tri` 的幻化后反而不跟随；②OBody NG 换身形预设明显变卡；③SLIF 膨胀后幻化仍不跟随（这份日志来自另一台机器）。
- **真因一（RaceMenu 从未接上）**：morph 签名只有 RaceMenu / OBody / SLIF 三路信号，该环境（1.5.97 + RaceMenu 4.0.0）里 `Dispatch(0x9E3779B9, ..., "skee")` 一直失败（日志每 5 秒一条 `Failed to dispatch message to skee`），签名从不工作 —— 与「之前的错误判断干扰」无关，是根本没有信号。
- **真因二（变卡）**：一次预设变化触发 `ApplyOBodyMorphsToActor` + 幻化循环 + 1.5.13 遗留的 400 ms / 1400 ms 两轮额外全量重建，同一角色共 4 次刷新。
- **真因三（SLIF 不动）**：签名只读 4 个规范键（`slif_belly` 等，1.0 中性），而 SLIF / Fill Her Up 走 `SLIF_Morph` 写 `morph:PregnancyBelly` 一类滑块，签名完全不变。
- **修法一（供应商无关的几何签名）**：新增 `ComputeGeometrySignature()` —— 遍历第一/第三人称两套 `GetBiped()` 的 `objects[i].partClone`，`AsDynamicTriShape()` → `GetDynamicTrishapeRuntimeData()`，对 `dynamicData` 头 1024 字节逐字节 + 步长 251 稀疏采样 + 顶点数做 FNV，再按槽位序号混合。任何改了顶点缓冲的东西（RaceMenu 滑块、OBody、SLIF、其它写 skee 的 mod）都会让签名变化，不再依赖任何 provider。
- **修法二（SLIF 滑块进签名）**：除 4 个规范键外再读 10 个滑块（`PregnancyBelly` / `PregnancyBelly_Edge` / `PregnancyBelly_SLIF` / `BelatedBelly` / `Breasts` / `BreastsSH` / `Breasts_SC` / `Butt` / `Butt_SC` / `GenitalsScrotum`）的 `GetCombinedMorph` 与 `GetApplied("morph:<滑块>")`。
- **修法三（去掉冗余重建）**：删除 `g_pendingRebuild2` / `g_rebuildScheduled2`，400 ms / 1400 ms 两段合并为一次 `kSettleRebuildDelay = 700 ms` 补建；OBody 换预设从 4 次刷新降到 3 次（循环 2 次 + 700 ms 一次）。
- **修法四（quiet 窗口防自触发）**：循环结束后立刻重算签名作新基线，并置 `kPostCycleQuiet = 2500 ms` 静默窗口，我们自己造成的位移不会再把循环点着；`kSignatureSettle` 250 → 350 ms、`kMinCycleInterval` 600 → 1200 ms。
- **修法五（无 provider 也工作）**：`PollOnMainThread` 去掉「三个 provider 全无就早退」、`Tick` 去掉 `haveAny` 门槛 —— 几何探针不需要任何 provider，RaceMenu 完全不应答时照样跟随。
- **SLIF 探测强化**：模块名依次试 `BodyInflationNG.dll` / `SLIFNG.dll` / `SLIF.dll`，receiver 依次试 `SLIFNG` / `BodyInflationNG` / `SLIF`，全失败再发无名广播。
- **日志**：`MorphRefresh: no body morph provider answered; morph changes follow the geometry probe only.`、`MorphRefresh: SLIF NG query interface vN acquired (N tracked actors); inflation follows the slider and geometry probes.`

## [1.5.17] - 2026-11-16

### 兼容 SLIF NG（膨胀框架）：身形变化时幻化自动跟随，不再依赖「换预设才刷新」

- **需求（m20732）**：兼容 SLIF NG（BodyInflationNG.dll，Nexus 192276）。现象与 OBody 相同 —— 身形（膨胀/瘦身）改变后幻化件不跟着变形、穿模；OBody 那条「一换预设就整轮重建」的路子不通用于 SLIF（它的膨胀是连续值，不是离散预设）。
- **真因**：`MorphRefresh` 只靠两个信号 —— RaceMenu `BodyMorph` 的 morph 值签名、OBody 的预设名签名。用户环境（1.5.97 + RaceMenu 4.0.0）里 skee 一直没应答，日志每 5 秒一条 `Failed to dispatch message to skee`，morph 签名从不工作 → SLIF 把身体滑块改了我们也看不见。
- **修法一（skee 取接口）**：命名 dispatch 失败后再用**无名广播**（receiver = `nullptr`）重试一次。SKSE 2.1.2+ 把 receiver 名解析成「注册了监听器的插件」列表下标，某个 DLL 加载失败会把后面的名字整体错位，`"skee"` 因此可能永远 miss；skee 自己用 `RegisterListener(nullptr)`，广播能到。OBody 的握手同样加了这个兜底。
- **修法二（SLIF 直连）**：新增 SLIF NG 查询接口后端 —— `GetModuleHandleA("BodyInflationNG.dll")` + `GetProcAddress("SLIFNG_GetQueryInterface")`，直接拿 `IQueryInterface1*`（纯 C 导出，不经过 SKSE 消息，因此不受上面那个错位问题影响；失败再退回 `0x534C4946 'SLIF'` 消息握手）。每 250 ms 对每个已幻化 actor 读 `IsTracked` 与 4 个规范膨胀键（`slif_belly` / `slif_breast` / `slif_butt` / `slif_scrotum`）的 `GetApplied`，并入签名。这条信号同时覆盖 SLIF 的两条输出路径：BodySlide morph 与骨架节点缩放。
- **修法三（抖动抑制）**：签名必须**静置 250 ms** 才触发重建（`kSignatureSettle`），同一角色两次幻化循环至少间隔 600 ms（`kMinCycleInterval`）。SLIF 的膨胀是逐帧 ramp，OBody 也是分步写 morph，逐值重建会把幻化刷成幻灯片；去抖后一次变化只重建一次，ramp 结束后再补一轮。
- **日志**：`MorphRefresh: SLIF NG query interface vN acquired (N tracked actors); inflation changes will rebuild the illusion.`、`MorphRefresh: SLIF NG is not installed; ...`；仍未接上任何 provider 时才打印 `no body morph provider is available`。


## [1.5.16] - 2026-11-16

### 新增：幻化栏复制 / 粘贴整套预设

- **需求（m20513）**：在幻化栏选中一套装备时，点 Copy 即复制整份预设（名称、槽位规则、单槽位规则、部件）；点 Paste 读取剪贴板，按里面的套装名判断重名 —— 无重名沿用原名，有重名加 `_copy` 后缀新建；槽位规则与部件按原 JSON 结构还原；粘贴完成后左侧列表自动焦点到新套装。
- **剪贴板格式**：单个 outfit 对象，与 `OutfitSystemData.json` 完全同结构（`name` / `isFavorite` / `armors[{rawFormId,localFormId,modName}]` / `slotPolicies` / `slotPolicy`），策略写原版数字枚举。粘贴端同时接受外层 `{"outfits":[...]}`，因此可以直接从 `OutfitSystemData.json` 里剪一段来用。
- **实现**：新增 `src/cpp/OutfitClipboard.{h,cpp}`（Win32 `CF_UNICODETEXT` 读写 + JSON 组装/解析 + 免重名计算），`OutfitService::create_outfit_preset()` 一次加锁内建好整套（先重置 32 槽与基础策略，再套用文件里的值，缺字段不会继承默认策略），`MenuUI` 底部新增 Copy / Paste 两个按钮。
- **焦点**：粘贴成功后 `FocusOutfitInList()` 选中新套装、翻到它所在页，行渲染时 `SetScrollHereY(0.5)` 把它滚到视野中间。
- **文案**：词典 237 → 248 条（复制/粘贴按钮 2 条 + 9 条提示/错误）。
- **日志**：`Outfits: copied 'X' to the clipboard (N bytes of JSON).`、`Outfits: pasted 'A' as 'A_copy' (N pieces, blanket XXOO, M slot overrides).`


## [1.5.15] - 2026-11-16

### 修复：幻化循环不再闪出原版装备，刷新速度接近列表切换

- **现象**：1.5.14 的 250 ms 幻化循环虽然能刷新 `.tri`，但会先还原成原版装备再变回幻化，既露馅又卡顿。
- **根因**：1.5.14 把两阶段拆成两次独立的主线程任务，中间隔了 250 ms，十几帧渲染的是真实护甲。
- **修法**：`MorphRefresh::RunIllusionCycle()` 把「摘掉幻化 + RefreshActor」与「穿回幻化 + RefreshActor」放进**同一个主线程任务**背靠背执行。引擎的这次装备 pass 是同步的（日志里 `Armor Refresh` 之后立刻就是 `Custom:` 行），因此中间没有任何一帧被渲染，原版装备不会被看到；两次刷新叠加的开销与在列表里连点两次相同。
- **Z 键 / 快选窗 E 立即生效**：`NGFeatures::AssignOutfit()` 在「重穿同一套」时直接调用循环，不再先做一次多余刷新，也不经过 250 ms 心跳节流。
- **兜底开关**：新增 `[Compatibility] ReillusionGapMs`（默认 0，范围 0–1000）。极端机况下若同帧刷新拿不到新形变，把它设成 20–50 就会退回两段式（会短暂露出原版装备）。
- **日志**：`MorphRefresh: taking the illusion of <FormID> off for one pass (<原因>)` 与紧接着的 `MorphRefresh: illusion of <FormID> restored with a fresh morph target.` 现在成对出现在同一帧。
## [1.5.14] - 2026-11-16

### 修复：换身形后幻化件依旧穿模（1.5.9–1.5.13 的路线走错了）

- **现象**：用户 m20054 / m20257 复报，OBody 换身体预设后只有带身体参照的上衣跟随，其余幻化件仍穿模；按 Z 重穿当前套装也不刷新，等于退回到最初的「必须重新穿脱或先取消幻化」。
- **真因一（Z 无效）**：`OutfitService::set_outfit_c`（以及 `set_state_outfit` / `unset_state_outfit` / `set_state_based_switching_enabled`）改的是指派而不是套装数据，从不 `++_revision`。1.5.13 的 `RevisionRebuildPending` 因此永远看不到变化，日志里 16 次 `Custom` 全是 `outfit revision false`。
- **真因二（重建无效）**：在原槽位上 `ClearBipedPart` 再 `ApplyArmorAddon` 只是把同一个 `BIPOBJECT` 重填一遍，引擎不会为新克隆重新加载 `.tri` 形变目标；日志里 48 条 `slot .. clone 0x0` 说明此刻 `partClone` 还没被挂上。真正能让形变生效的是「先把幻化脱掉、让引擎按真实护甲走一遍，再把幻化穿上」——也就是用户一直手动在做的那套动作。
- **修法**：
  1. `set_outfit_c` 等指派类接口一律 `++_revision`，按 Z / 快选窗按 E / 清空幻化都会让皮肤钩子走全量重建。
  2. `MorphRefresh` 新增`幻化循环`：签名变化（或用户重穿同一套）时，先 `set_outfit_c("")` + `RefreshActor` 把幻化摘掉，250 ms 后 `set_outfit_c(name)` + `RefreshActor` 重新穿上，新克隆在形变落定之后创建。整套自动完成，玩家不需要再手动取消幻化。
  3. 同一角色已有循环在排队时不重启（防止 OBody 反复派生形态时来回抖动）；`Reset()` 清空循环表。
- **日志**：`MorphRefresh: taking the illusion of <FormID> off for one pass (body morph changed)` / `... (the outfit was re-applied)`、`MorphRefresh: illusion of <FormID> restored with a fresh morph target.`、`Patches.cpp: Custom: rebuilding ... outfit revision true`。

## [1.5.13] - 2026-11-16

主题：**手动重穿当前套装（Z 键 / 快选窗）也会重建幻化件；OBody 换身形后再补一次重建**。

### 现象与根因

- 用户 m20054（1.5.12 实机）：OBody 换预设后其余幻化件依旧不跟随；这次连按 Z 重新点一次当前套装都不刷新。
- 根因一（Z 无效）：1.5.12 只有「body morph 签名变化」才进全量重建分支。玩家自己重穿同一套时只有 `OutfitService` 的 revision 变了，`Custom` 仍走便宜的 `applySet` 路径（不清槽），旧 clone 被 `ApplyArmorAddon` 当成已是最新原样留下，所以按 Z 看不出任何变化。
- 根因二（仍穿模）：OBody 的 morph 是排队生效，并在装备变化后重新派生；1.5.12 只在 500 ms 后补一次重建，仍可能早于 OBody 落定。

### 修复

- `Patches.cpp` 新增 `RevisionRebuildPending()`：按 actor 记住上次见到的 `OutfitService::revision()`，变化后 2 秒内（覆盖第一/第三人称两次 `Custom` 调用）走全量重建（`ClearSlots` + 重贴整个 displaySet + `NotifyRebuilt`）。套装新建/编辑/改派——包括快选窗按 E、Z 键重穿、Papyrus `SetOutfit`——都会触发。
- `MorphRefresh` 改为两段延迟重建：签名变化后 400 ms 与 1400 ms 各重建一次（`g_pendingRebuild` / `g_pendingRebuild2` 去重），强制窗口 2 秒 → 4 秒，保证 OBody 落定后的那次重建也走全量路径。
- RaceMenu 接口重试降噪：2 秒 × 30 次 → 5 秒 × 12 次（CLIB 的 `Failed to dispatch message to skee` 警告从 52 行降到约 24 行）。
- 诊断：全量重建时逐件打印 `Custom: <ArmorFormID> slot <nn> clone <指针>`（debug）。同一槽位两次重建后指针应变化；不变说明 `ApplyArmorAddon` 复用了旧几何，问题不在 morph 时序。

### 版本

- 版本号 1.5.12 → 1.5.13（version.h、CMake、vcpkg）。
- 本机无 Skyrim，未实机验证；按 `docs/TESTING.md` §3.26 复测。

## [1.5.12] - 2026-11-16

主题：**OBody 换身体预设后，其余幻化件也跟随新身形重建（不再穿模）**。

### 现象与根因

- 用户 m19650（1.5.11 实机）：身体/衣服整体消失已恢复，但 OBody 换预设后只有带身体参照的 32 槽上衣换了身形，裤子、鞋子、头发、发饰仍是旧模型，直接穿模；取消幻化、或先切别的套装再切回来就正常。
- 根因一：OBody NG 的 `ApplyOBodyMorphsToActor` 走 `ReapplyActorMorphs → GenerateBodyByPreset → ApplyMorphs`，其中 `ApplyBodyMorphs(actor, true)` 是排队/异步生效的；1.5.11 在签名刚变化时立刻 `RefreshActor()`，克隆仍从旧 morph 生成。
- 根因二：OBody 会在装备变化后重新派生 ORefit/预设 morph，而我们的重建本身看起来就是一次装备变化，所以第一次重建后形态还会再变一次。
- 根因三：RaceMenu 在 1.5.97 上 `kDataLoaded` 时可能还没应答 `0x9E3779B9` 消息交换，1.5.10 只试一次就永久停用了 RaceMenu 后端（日志 `RaceMenu did not answer the interface exchange`）。

### 修复

- `MorphRefresh::NotifyRebuilt()` 不再要求 RaceMenu：只要 RaceMenu 或 OBody ready 任一存在，就排 150 ms 的 morph pass；OBody-only 时调用 `ApplyOBodyMorphsToActor` 重放整套 morph。
- 签名变化后除立即 `RefreshActor()` 外，再排一次 **500 ms 的延迟重建**（`g_pendingRebuild` + `g_rebuildScheduled` 去重，仍受 2 秒强制窗口保护），等 OBody 稳定后重新清槽重贴 displaySet；这次重建又会触发一次 `NotifyRebuilt` 的 morph pass，形成“重放 → 重建 → 再重放”的闭环。
- `RetryProviders()`：RaceMenu 接口在输入心跳里每 2 秒重试一次（最多 30 次），成功后静默切换回 RaceMenu 权威路径；失败日志降为 info 且只打一次。
- 日志：`MorphRefresh: queued the body morph pass for ...`、`MorphRefresh: rebuilding the illusion of ... after the body morph provider settled.`（debug）。

### 版本

- 版本号 1.5.11 → 1.5.12（version.h、CMake、vcpkg）。
- 本机无 Skyrim，修复未实机验证；按 `docs/TESTING.md` §3.25 复测（重点是裤子/鞋子/头发/发饰是否一起跟随）。

## [1.5.11] - 2026-11-16

主题：**修复 OBody 换身体预设后身体与幻化模型整体消失（只剩头部与手）**。

### 现象与根因

- 用户 m19368：带 `.tri` 的服装在 OBody 更换身体预设后，衣服模型和身体模型全部消失，只剩头部与手；按 Z 重新点一次当前套装就恢复。
- 根因：`CustomSkinPlayer::Custom` 每次刷新会被调用两次，分别对应第一人称与第三人称的 `ActorWeightModel`。1.5.9 的 `ClearSlots` 对 `GetBiped(false)` 和 `GetBiped(true)` 两套 BipedAnim 一起清空，但每次 `Custom` 只把自己那一个 `actorWeightModel` 重新贴满；第二次调用把第一次刚重建的另一套模型又清空，于是第三人称的身体与衣服整体消失。
- `Custom` 的第三个参数是 actor 的 `BSTSmartPointer<BipedAnim>` holder 地址（不是 BipedAnim 本身），因此 `ClearSlots` 现在按传入的 `ActorWeightModel*` 只清当前这一套。

### 修复

- `Patches.cpp`：`void ClearSlots(RE::ActorWeightModel* a_model, std::uint32_t a_mask)`；从 `a_model` 取出 `BSTSmartPointer<BipedAnim>::get()` 后只用 `ClearBipedPart` + 置空 item/addon/part 清 mask 命中的槽位；调用点改为 `ClearSlots(actorWeightModel, displayMask)`。
- 其余强制重建流程不变（displayMask 并集、displaySet 全量重贴、PAD RestoreRealItems、NotifyRebuilt）。

### 版本

- 版本号 1.5.10 → 1.5.11（version.h、CMake、vcpkg）。
- 本机无 Skyrim，修复未实机验证；按 `docs/TESTING.md` §3.24 复测。

## [1.5.10] - 2026-11-16

主题：**OBody 换身形预设后，整套幻化外观实时重建（不再依赖 RaceMenu）**。

### 现象与根因

- 1.5.9 已把强制重建范围扩大到整个 displaySet，但用户实机仍复现：OBody 换身体预设后幻化部件不变化；取消幻化、或先切别的套装再切回来就正常。
- 根因：1.5.9 的 `MorphRefresh` 只通过 `skee` 取 RaceMenu `BodyMorph` v4+ 接口。用户环境没有 RaceMenu，日志为 `MorphRefresh: RaceMenu is not installed; the .tri morph refresh stays off.`，`g_api` 为空，`Tick()` 直接返回，`ShouldForceRebuild()` 永远不会为真；`Patches.cpp` 里已实现的全 displaySet 重建分支因此从未触发。

### 修复

- `MorphRefresh` 新增 OBody 插件 API 后端：`kDataLoaded` 时向 receiver `OBody` 发送 `RequestPluginInterface`（消息类型 `0xc0B0D9cc`，v1），并用静态生命周期的 `IOBodyReadinessEventListener` 跟踪 ready 状态；OBody 4.4.0+ 才有该 API。
- 轮询签名改为 RaceMenu 与 OBody 的并集：RaceMenu 混入 `VisitMorphValues` 的 FNV 签名；OBody 混入 `GetPresetAssignedToActor` 返回的预设名 FNV 签名（空串表示无预设）。任一后端可用即工作。
- OBody-only（没有 RaceMenu）时，检测到预设变化后先调用 `ApplyOBodyMorphsToActor`，再 `Features::RefreshActor()`；后者触发 `Custom` 的 2 秒强制窗口，按 displaySet 并集 `ClearBipedPart` 后重贴全部显示件。RaceMenu 存在时仍以其 BodyMorph 为权威，沿用原有的延迟 `ApplyBodyMorphs`。
- 新增日志：`MorphRefresh: OBody plugin API v1 acquired; preset changes will rebuild the illusion.`、`MorphRefresh: OBody is ready; preset changes will rebuild the illusion.`、`MorphRefresh: no body morph provider is available; the .tri morph refresh stays off.`。
- 未安装 OBody 或版本低于 4.4.0 时只记 debug，不影响换装与幻化。

### 版本

- 版本号 1.5.9 → 1.5.10（version.h、CMake、vcpkg）。
- 本机无 Skyrim，OBody 预设切换后的实际刷新尚未实机验证；OBody API 为按头文件 ABI 手写的最小声明。

## [1.5.9] - 2026-11-16

主题：**修复 OBody 换身形后只有身体参照件刷新、其他幻化件（裤子等）仍用旧形变导致穿模**。

### 现象与根因

- 幻化套装带 .tri 形变时，在 OBody 里换身体预设：32 槽位的上衣（带身体参照模型）刷新了，裤子等其他部件保持旧模型，直接穿模。
- 根因：1.5.7 的强制重建分支只清空并重贴 applySet（显示集中“不是实际穿戴”的幻化件）。显示集中同时包含真实穿戴的护甲时，这些件由引擎应用、被 Custom 跳过；它们的 BipedAnim 槽位没有 ClearBipedPart，partClone 仍是旧形变。

### 修复

- CustomSkinPlayer::Custom 新增 displayMask：对整个 displaySet 每件护甲的 GetSlotMask().underlying() 取并集。
- forceMorphRebuild 时 rebuildSet 改为 displaySet 全部件（不只 applySet），先 ClearSlots(actor, displayMask) 对第一/第三人称一次清空所有涉及槽位（1.5.11 更正：这样会清掉另一套刚重建的模型，已改为只清当前 `ActorWeightModel`），再逐件 ApplyArmorAddon；每件后仍按 PAD 开关 RestoreRealItems。非强制路径保持原来的 applySet 行为。
- 开 Verbose logging 时新增 debug 日志：Custom: force-rebuilding N display pieces (mask 0x........) for .... after a body morph change.。

### 版本

- 版本号 1.5.8 → 1.5.9（version.h、CMake、vcpkg）。
- 本机无 Skyrim，OBody 多件刷新尚未实机验证。

## [1.5.8] - 2026-11-16

主题：**修复套装编辑器右侧身体槽位列表的“移除”按钮点击无反应**。

### 现象与根因

- 在 SMF 幻化栏选中套装后，右侧 Body slots 列表点“移除”没有任何可见反馈，日志也没有操作记录。
- 根因：第一次点击只把待移除的护甲写进 g_pendingRemove，确认条固定渲染在编辑器最上方；用户滚到下方槽位列表时看不到确认条，于是以为按钮失效。另一个叠加因素是按钮 X 由护甲名宽度决定，长名字会把按钮推到子窗口可视区外。

### 修复

- 确认改为行内：点击某一行的“移除”后，该行上方立即显示“从幻化中移除…？”，并在同一行出现“移除 / 取消”两个按钮；确认后立即删除并刷新，取消则清除状态。
- 按钮 X 按 ContentRegionAvail 的当前宽度 clamp，长护甲名不会再让按钮跑出可视区。
- 删除成功写 info 日志：Outfit editor: removed '...' from '...'.；Invalidate 同时清空待移除槽位，切换套装不会留下上个套装的确认状态。

### 版本

- 版本号 1.5.7 → 1.5.8（version.h、CMake、vcpkg）。
- 本机无 Skyrim，行内确认与按钮位置尚未实机验证。

## [1.5.7] - 2026-11-16

主题：**快速选择窗口增加 X 键取消幻化；带 .tri 形变的幻化装备在身形/滑块变化后实时重建**。

### 快速选择窗口：X 键取消幻化

- 键盘 `X`、手柄 `X`（`ImGuiKey_GamepadFaceLeft`；旧框架 raw 路径为 `BSWin32GamepadDevice::Key::kX`）与窗口底部 **Remove outfit** 按钮调用同一个 `QuickRemoveOutfit()`：清空该 actor 的当前套装、刷新外观、HUD 提示“已取消幻化。”。
- 搜索框等文本输入聚焦时不响应 X；底部提示与按钮 tooltip 已同步为 `X: remove`。

### .tri 形变实时刷新

- 现象与根因：服装带 `.tri` 形变（OBody、游戏内换身形预设等）时，幻化件的 partClone 仍保留旧顶点形变；`ApplyArmorAddon` 对同一 item 会复用已有槽位，单纯 `refreshArmorFor` 不会重建，所以必须重新穿脱或先取消幻化。
- 新增 `SOS::MorphRefresh`：通过 SKSE messaging（`skee`，0x9E3779B9）取得 RaceMenu `BodyMorph` v4+ 接口，250 ms 轮询已幻化且 3D 已加载 actor 的 morph 签名（`VisitMorphValues`，FNV 顺序无关累加）；签名变化时给该 actor 开 2 秒强制重建窗口并调用 `Features::RefreshActor()`。
- `Patches.cpp` 的 `CustomSkinPlayer::Custom` 在强制窗口内先对幻化槽调用 `BipedAnimAugments::ClearBipedPart`（15496/15661）并清空 item/addon/part，再 `ApplyArmorAddon`，让新的 partClone 读取当前 `.tri` 形变；重建后延迟 150 ms 调用 `BodyMorph::ApplyBodyMorphs(actor, true)`，覆盖 partClone 由任务池延迟挂载的情况。
- 没装 RaceMenu / BodyMorph 版本低于 4 时只记一条 info，其余功能不受影响。

### 版本

- 版本号 1.5.6 → 1.5.7（version.h、CMake、vcpkg）。
- 已知限制：RaceMenu 接口为按 ABI 手写的最小声明，`.tri` 刷新与 X 键均待实机验证。

## [1.5.6] - 2026-11-16

主题：**更正 SMF 手柄分水岭：3.18 已可用，raw fallback 阈值从 3.23 下调到 3.18**。

### 更正

- 用户实机反馈：SMF 3.18 下 1.5.4 的快速选择窗口与主菜单手柄功能正常，**3.23 并非分水岭**。
- 上游源码核对：SMF 3.14.0 引入 `GamepadNavigation`，同一版本包含 "fixed a regression that was causing gamepad to be unusable"（提交 8fb2d29，2026-09-09）；3.13 及更早的源码树没有这个模块。
- 1.5.5 的 `FrameworkVersion() < 3.23` 门控会在 3.18 这类已经正常的版本上多注册 raw 回调；1.5.6 改为 `< 3.18`，3.18 及以上交给框架自身的 gamepad 翻译。

### 版本

- 版本号 1.5.5 → 1.5.6（version.h、CMake、vcpkg）。

## [1.5.5] - 2026-11-16

主题：**确认快速选择窗口手柄失效来自 SKSE Menu Framework 3.9，并给旧框架加兼容兜底**。

### 结论

- 用户实机验证：**只把 SKSE Menu Framework 从 3.9 升级到 3.23，1.5.4 未改一行代码，快速选择窗口与主菜单的手柄同时恢复**（**1.5.6 更正**：3.18 也已实测正常，3.23 不是分水岭）。因此根因在框架 3.9 对自定义窗口的手柄处理，不在幻化钩子或 1.5.4 的 ImGui 代码。
- 1.5.4 已经把窗口级的 ImGuiWindowFlags_NoNavInputs 去掉；3.9 仍无法把 gamepad 送进自定义 AddWindow 窗口。

### 兼容加固

- MenuBridge::WindowInterface 补上 3.x 的 BlockUserInput 字段，AddWindow 返回后显式置 true，让框架明确把快速选择窗口当作 modal/blocking window。
- 1.5.5 初版仅在框架版本**低于 3.23**（或取不到版本）时注册原始输入回调；1.5.6 已按实机结果把阈值下调为低于 3.18。快选窗打开时直接读取 RE::ButtonEvent 的十字键/A/Y/B，渲染线程消费，不再依赖 ImGui 的 gamepad 导航；3.18 及以上不注册，避免把事件从框架队列里吞掉。
- 回调不存在或注册失败只记 warn，保留原 ImGui key 路径。
- 打开/关闭快速选择窗口时清空手柄状态，避免重开时残留按下状态。
- 版本日志改用 spdlog 格式，修复此前 Menu: SKSE Menu Framework %.2f (api %u) 原样打印的问题；现在可直接看到框架版本。

### 建议

- **使用 SKSE Menu Framework 3.18 或更新版本**（3.18 与 3.23 均已实机确认手柄正常）。3.9 等更旧版本不在本项目的实机验证范围内；1.5.6 的 raw fallback 只是让旧版用户能操作快选窗，不能修复框架自身的其它输入问题。

### 版本

- 版本号 1.5.4 → 1.5.5（version.h、CMake、vcpkg）。

## [1.5.4] - 2026-11-16

主题：**修复快速选择窗口导致手柄导航全局失效**。

> **1.5.5 更正**：实机确认 1.5.4 + SMF 3.9 仍失效，升级 SMF 3.23 后恢复，说明 3.9 的自定义窗口手柄处理才是主因；本节把 NoNavInputs 当作唯一根因的说法不准确。1.5.4 去掉该标志的修改保留，但不是充分修复。**1.5.6 再更正**：3.18 也已实测正常，3.23 不是分水岭。

### 根因

- 1.5.0 为避免菜单框架的 ImGui 导航在快选窗上产生第二个高亮，给独立快选窗加了 ImGuiWindowFlags_NoNavInputs。ImGui 1.90 的 io.NavActive 依赖当前 NavWindow 且要求该窗口没有 NoNavInputs；快选窗一旦成为 NavWindow，整个 ImGui 上下文的 NavActive 变为 false，菜单框架的 GamepadNavigation 也随之停止工作。因此不只是快选窗，连 SMF 主菜单的手柄都无法操作。NoNavFocus 只影响 Ctrl+Tab，不是主因。

### 修复

- 快选窗窗口标志只保留 NoCollapse，去掉 NoNavInputs / NoNavFocus，窗口本身仍可成为 NavWindow，io.NavActive 保持为真。
- 新增运行期可选的 igPushItemFlag / igPopItemFlag 绑定，在 RenderQuickPickerBody 前后压入 ImGuiItemFlags_NoNav（1 << 3），让快选窗内的按钮、列表行、滑条都不进入 ImGui 导航焦点；这样 W/S/A/D、十字键、A/B/X/Y 仍由快选窗自己的 IsKeyDown/IsKeyPressed 处理，不会激活 Remove outfit 之类的控件。
- 缺少这两个导出时只记 warning，快选窗仍可打开（退回不使用 ItemFlag），菜单其余部分不受影响。

### 其他

- SceneOutfits 在找不到“当前套装名 + _sex”时，日志级别由 debug 提升为 info，并明确打印需要的完整套装名。若套装命名不符合约定，用户可以直接在日志里看到期望名字；本次实机报障即属于命名不匹配，并非事件未触发。

### 版本

- 版本号 1.5.3 → 1.5.4（version.h、CMake、vcpkg）。

## [1.5.3] - 2026-11-16

主题：**9 小时无人值守的第三轮静态审计：存档/脚本边界、状态换装判定与构建稳定性**。

### 修复：Papyrus 套装操作不刷新外观

- DeleteOutfit 删除成功后调用 CreateRefreshAllArmorsTask；OverwriteOutfit 改走新的 OutfitService::replace_outfit_armors（整体替换并递增 revision），成功后同样排队刷新。此前第三方脚本删除或覆盖“当前正在显示的套装”时，幻化外观会残留到下一次装备刷新。

### 修复：状态自动换装的两类漏判

- AutoSwitch 与战斗/死亡事件原先只检查 should_override()，而它要求基础套装非空；只配置了状态套装（没有基础套装）的角色永远不会自动换装。现在改为 enabled && 该角色开启状态切换 && (有基础套装 || 任一状态套装已配置)。
- 战斗/死亡事件补上 per-actor 的 stateBasedAutoSwitchingEnabled 判定，关闭“按地点/状态自动切换”后不会再被战斗事件强行切回状态套装。

### 修复：随机换装的收藏回退

- 仅收藏模式下，如果唯一收藏就是当前套装，排除当前后候选为空，原来直接返回空、表现为热键无反应。现在依次回退到全部套装、再取消“排除当前”，保证只要存在套装就一定能抽到。

### 修复：存档与导入的异常边界

- 读 co-save（v7 JSON）和 OutfitSystemData.json 时，字段类型异常会抛 nlohmann 异常并终止进程；两处入口都加了 try/catch，坏文件只记 error 并返回失败，不再崩溃。

### 修复：金件集合的读竞争

- Settings 的 _resolvedGolden 改为 shared_ptr 快照 + atomic_load/store；皮肤钩子/渲染线程的 IsGoldenItem 读路径不再与菜单线程的重载配置竞争，也不再需要每次读取加锁。

### 修复：构建稳定性

- 去掉 /MP：它会无视 cmake --build --parallel 1 再为每个源文件开满核并行 cl.exe，在 4.8 GB 页面文件的机器上触发 C1076 / C3859。现在完全由 --parallel 1 控制。

### 翻译

- 词典去重并补全界面字面量：237 条唯一词条（此前 233 条，新增 12 条中有 8 条与旧条目重复，已合并为同一键）。语言文件重新生成，仍为每语言一份 UTF-16LE + TAB 格式。

### 版本

- 版本号 1.5.2 → 1.5.3（version.h、CMake、vcpkg）。
## [1.5.2] - 2026-11-16

主题：**9 小时无人值守的第二轮静态审计：线程安全、空指针、缓存与界面细节**（无新增玩法）。

### 修复：线程安全

- AutoSwitch::Tick 在输入线程只做节流与排队，真正的 actor 遍历/状态评估/刷新放到主线程 TickOnMainThread；g_tickPending 防止重复排队。
- 随机换装与快速选择热键的 Fire/FireQuickPicker 改为排队到主线程执行；文本输入状态由渲染线程采样并缓存 500ms，输入 sink 不再直接读 ImGui。
- Settings 的黄金件解析改为 atomic + mutex：WarmUp/InvalidateResolvedForms 加锁，IsGoldenItem 只读 atomic 后再查集合。

### 修复：空指针与健壮性

- MenuUI 的 Add player 先判玩家存在；TargetName / RenderOneActor / 删除确认，以及 MenuOutfitEditor 的目标名，对 GetDisplayFullName 返回空值做兜底。
- Patches 的 FixEquipConflictCheck 判 item / target / ActorEquipManager；战斗与死亡事件判玩家；RegisterEvents 判 ScriptEventSourceHolder；CreateRefreshAllArmorsTask 判任务接口。
- OutfitService::check_consistency 在自动插入玩家或清理非法 FormID 后递增 revision，菜单的套装/角色缓存能及时刷新。

### 修复：界面行为

- 状态自动切换勾选、13 个状态套装修改后调用 NGFeatures::ApplyCurrentStateOutfit，立即按当前状态应用；不再只刷新旧 current。
- 删除套装时同时清空过滤结果并失效快速选择列表签名，避免删除后当前帧仍显示旧行。
- 快速选择窗口 Page size 的上限由 200 修正为 500；设置页 List width 240..1200、Outfit / Quick picker page size 5..500。
- 所有菜单页都会推进延迟设置保存；在快选窗或 Outfits 页改页大小后，不必再打开 Settings 才落盘。
- 设置页热键输入框会在 ini 值变化时刷新显示，不再只在首次打开时读取。
- SceneOutfits 支持 OStim / SexLab 场景重叠：同一来源重复 start 忽略，第二个来源只登记，最后一个场景结束才恢复进入场景前的套装。

### 性能

- OutfitService 的读/写守卫改为栈上返回值，去掉皮肤热路径每次调用 make_unique 的堆分配。
- DisplaySet 的 WornArmorVisitor 直接写调用方 vector；ComputeDisplaySet 复用 thread_local worn 缓冲，皮肤钩子不再每次分配。
- Outfits / Quick picker 的名称搜索改为无分配的大小写折叠；只有开启“仅收藏”时才做套装哈希查找。
- Outfits 列表复用页内收藏与护甲数量 vector；非当前穿着行不再复制 label 字符串。
- 服装编辑器的背包/穿戴护甲列表增加 500ms TTL 缓存；PadCompat 的诊断映射超过 512 条自动清空。

### 版本

- 版本号 1.5.1 → 1.5.2（version.h、CMake、vcpkg）。

## [1.5.1] - 2026-11-16

主题：**9 小时无人值守的静态审计与性能优化（无新增用户功能）**。

### 性能：菜单与编辑器不再每帧做全量复制

- OutfitService 新增原子 revision 计数，所有会改变套装集合/收藏/名称/策略的写操作都会递增；MenuUI 的服装名缓存与过滤结果缓存改为按 revision 签名判断，只有真正变化时才复制全部套装名并重新过滤。修复了 1.5.0 中“重命名或改收藏但数量不变时列表不刷新”的隐性 bug。
- Outfit 内部新增槽位缓存与策略缓存：compute_display_set_c（每个皮肤钩子/每帧都会调用）不再每次遍历完整护甲集合、不再对 32 个槽逐个线性查找策略表；护甲增删或策略修改时置脏，下次调用惰性重建。
- 服装编辑器的“完整护甲列表”增加静态缓存（按 TESDataHandler 指针 + 护甲数组 size/data 失效），搜索框每次输入不再扫描整个 TESDataHandler 并对所有护甲反复做 ArmorName/LowerCopy/sort。
- 装备访客 EquippedArmorVisitor 从 unordered_set 改为 vector 并预留 32 个元素，去掉每次 skinning 调用前的本地复制；DisplaySet 的 WornArmorVisitor 同样预留容量。
- 菜单的 Outfits / Actors / 编辑器改为短时间持锁：先复制需要的名字/收藏/状态，再绘制，写操作单独短写锁；避免菜单打开时渲染线程的皮肤钩子等待整帧。
- 设置页滑条与热键输入框的 Settings::Save 改为 0.5 秒节流；快速选择窗口几何仍为 0.75 秒节流并在关闭时强制落盘。

### 修复：线程安全与健壮性

- 随机换装的两处 std::mt19937 改为 thread_local，避免 Papyrus / 输入线程并发抽取时的数据竞争。
- 热键状态（组合键、latch、文本输入标志）改为 mutex + atomic；修复“在文本框中松开按键后 latch 卡住，热键从此失效”的问题。Reload 时先解析到局部再原子替换。
- MenuBridge 的可用标志、MenuUI 的注册标志/快选几何强制标志/待刷新标志改为 atomic；快速选择窗口的列表刷新与焦点定位延迟到渲染线程执行，不再从 Papyrus/输入线程直接改 UI 状态。
- Patches 的三处装备访客增加空 InventoryEntryData 判定；ShimWornFlags 增加 actor / inventory 判空；ModCompat 与 CreateRefreshAllArmorsTask 增加 SKSE 任务接口判空。
- Features::RefreshActor 增加 Is3DLoaded 检查，读档中不再对尚未建立 biped 的角色调用 AI equip；菜单/快选/随机换装统一走 OutfitSystem::refreshArmorFor，确保 PAD 兼容回调与 SMP 重置也会执行。
- 修复 Outfits 列表页码可能超过总页数导致的 end-begin 无符号下溢（删除最后一页唯一套装或过滤缩短列表时可能崩溃）。
- 套装名输入缓冲区从 128 字节扩到 257 字节（kMaxNameLength=256），并在 OutfitService 的创建/重命名/Papyrus 路径强制长度上限。
- DisplaySet::OverrideActive 改用 OutfitService 的真实 enabled 状态；[Debug] InstallHooks=0 现在真正阻止皮肤钩子安装；新游戏重置 enabled=true。
- 快速选择窗口的随机按钮只遵守窗口内的 Favourites 复选框，不再被全局 RandomOutfit.FavoritesOnly 默认值强制。

### 文案

- 中文翻译中“套装”统一改为“幻化”（保留 Policy.cpp 的旧中文别名以兼容历史 JSON / 老存档）；两份语言文件重新生成，仍为 233 条。

### 版本

- 版本号 1.5.0 → 1.5.1（version.h、CMake、vcpkg）。

## [1.5.0] - 2026-11-16

主题：**Outfits 列表按钮可点击、快选窗口手柄不再双高亮、SMP 逐角色重置、SexLab / OStim 场景套装**。

### 修复：Outfits 列表的“穿上 / 删除”按钮点不中

- **根因**：列表每一行的 `Selectable` 被撑到整行宽度（`max(文本宽 + 150, 列表宽 - 24)`），而 Wear / 星标 / Delete 三个按钮用 `SameLine(rowWidth - 142)` 叠在它上面。ImGui 的 `Selectable` 覆盖整个矩形，鼠标点击先被它吃掉，按钮只剩边缘偶尔能命中；用户描述的“穿上按钮压根点不中、删除偶尔能点中”就是这个原因，删除逻辑本身没有问题。
- **修法**：`src/cpp/MenuUI.cpp` 把名字区与按钮区彻底分开计算：`textWidth = CalcTextSize(label) + 8`、`buttonX = max(textWidth + 8, max(listWidth - 150, 0))`、`nameWidth = max(buttonX - 8, textWidth)`；`Selectable` 只占 `nameWidth`，按钮从 `buttonX` 开始，两者不再重叠。长名字仍会撑开横向滚动，双击选中、悬停提示保持不变。

### 修复：快速选择窗口用手柄时出现两个光标

- **根因**：快速选择窗口自己读十字键/摇杆来移动列表焦点，但窗口没有关闭 ImGui 的导航输入，SMF / ImGui 的默认导航同时也在移动另一个控件焦点。于是列表里一个高亮、`Remove outfit` 等按钮上另一个高亮，按 A 时先执行列表换装、再触发按钮，看起来像“一次按 A 做了两件事”。
- **修法**：`RenderQuickPicker` 的独立窗口 flags 改为 `NoCollapse | NoNavInputs | NoNavFocus`。窗口自己的 W/S/A/D、方向键、十字键、E/R/Esc 处理不变；框架不再接管这个窗口的导航，也就不会出现第二个光标。

### 新增：切换幻化时按角色重置 SMP 物理（Faster HDT-SMP）

- 新开关 `[Compatibility] AutoResetSMP`（默认 off），设置页 Mod compatibility 段有对应复选框，并显示 `Faster HDT-SMP (DynamicHDT): detected / not detected`。
- 实现：`src/cpp/SMP.cpp` 在每次 `refreshArmorFor()` 完成后，通过 `SKSE::GetTaskInterface()` 切回主线程，取该 actor 的 `ActorHandle`，调用 FSMP 的 Papyrus 静态函数 `DynamicHDT.ResetPhysics(actor, false)`（`softReloadMeshes`，温和重建该角色的 skeleton）。**只重置换装的那个 actor**，不是全局 `smp reset`，也不会遍历所有 NPC。
- 没装 FSMP、`DynamicHDT` 类型不存在、或 actor 的 3D 尚未加载时静默跳过（首次写一条 debug 日志），不影响换装本身。

### 新增：SexLab / OStim 场景自动换用 `_sex` 套装

- 新开关 `[Compatibility] SceneOutfitSwitching`（默认 off），设置页 Mod compatibility 段有对应复选框。
- 监听 SKSE 的 `ModCallbackEvent`：OStim 的 `ostim_start` / `ostim_end`，SexLab 的 `PlayerAnimationStart` / `PlayerAnimationEnd`（只认带 Player 前缀的线程，避免 NPC-only 场景误触发）。
- 场景开始：取玩家当前幻化名称；若已经以 `_sex` 结尾（大小写不敏感）则保持不变；否则用“当前名称 + `_sex`”在套装列表里精确查找（忽略大小写，与原版套装查找一致），找到就换上并刷新外观，找不到就保持当前套装。场景结束时，如果玩家没有手动换过别的套装，则恢复进入场景前的那一套；如果玩家中途手动换了，则不覆盖玩家的选择。
- 换装走 `OutfitSystem::refreshArmorFor()`，因此 PAD / AND 兼容与 SMP 重置（若开启）都会照常生效。

### 其他

- 词典新增 7 条英文/中文（SMP 与场景套装开关、状态提示），总数 226 → **233**；两份语言文件按词典重新生成。
- ini 两份（包内默认与 `mod_files`）都补了 `AutoResetSMP = off` 与 `SceneOutfitSwitching = off` 及注释。
- 版本号 1.4.4 → 1.5.0（`version.h` 的 MINOR/PATCH、CMake、vcpkg）。

## [1.4.4] - 2026-11-15

主题：**五项实机报障：删除按钮、编辑器横向滚动、日志瘦身、原生地点检测、状态套装 <随机>**。

### 修复：Outfits 列表的删除按钮点了没反应

- **根因**：按钮只把名字写进 `g_pendingDelete`，整个文件再没有任何地方读它 —— 既没有执行删除，也没有日志，所以看起来完全没反应。
- **修法**：`src/cpp/MenuUI.cpp` 在列表下方加确认条（`Delete outfit "%s"? This also removes it from every actor and state assignment.` + Delete / Cancel 两个按钮）。确认后调用 `OutfitService::delete_outfit()`（会同时清掉所有 actor 的当前套装与 13 个状态槽引用），如果删的正是当前选中的套装就清空选择，并 `CreateRefreshAllArmorsTask()` 刷新全部外观。成功/失败都会写一行 info：`Outfits: deleted 'name': yes|no.`

### 修复：套装编辑器的槽位列表与逐槽策略没有横向滚动

- `##slotlist`、`##slotpolicies`、`##outfiteditor` 三个子窗口都加了 `ImGuiWindowFlags_HorizontalScrollbar`。
- 槽位行不再用固定的 `SameLine(320)`：先算 `SlotLabelFor(slot) + ": " + ArmorName` 的实际文本宽度，再 `SameLine(max(320, width + 16))`，长名字不会压住 Remove 按钮，窗口可以横向拖到底。
- 基础策略与每个槽位策略的 combo 固定 `SetNextItemWidth(520)`，长预览文本有足够空间。

### 变更：默认日志瘦身

- 正常运行只保留生命周期与结果行：`Skinning hooks applied.` / `Papyrus functions registered.` / `Game event sinks registered.` / `Plugin loaded, waiting...` / `Menu: sections registered` / `Loaded|Saved N outfits` / `Imported N outfits` / `Migrated co-save` / `Language: wrote ...` / `AutoSwitch: native ... installed` / `Outfits: deleted ...` / `Menu: export/import ...` / PAD 的 `PAD slot report`（按钮主动触发）。
- 其余诊断（Settings 读写、ImGuiApi 可选符号、MenuBridge 加载、Hotkeys 注册与触发、Hooks 的逐站点 dump、NGFeatures 的快捷槽细节、兼容层检测、trampoline/Player3D 等）全部降为 **debug**，默认不可见；勾选 Settings → Diagnostics 的 **Verbose logging**（ini `[Debug] ExtraLogging = true`）后按 trace 级别输出，重启生效。
- warn / error / critical 不受影响。

### 变更：地点/状态自动切换改为 SKSE 原生检测

- 原实现依赖 `SkyOutSysAutoSwitchTrigger.psc` 的 `OnLocationChange` → `Utility.Wait(10.0)` → 调 Papyrus 原生函数，延迟至少 10 秒。
- 新增 `src/cpp/AutoSwitch.{h,cpp}`：`TESActorLocationChangeEvent` 与 `TESFastTravelEndEvent` 两个 `BSTEventSink` 直接注册到 `ScriptEventSourceHolder`；事件里只对已指派且打开「按地点/状态自动切换」的 actor 重新分类，状态没变就直接返回。
- 另加 `AutoSwitch::Tick()`（输入事件心跳，2 秒节流）作为天气变化与漏事件的兜底；读档/新游戏/回滚时 `Reset()` 清缓存。
- `src/papyrus/skyoutsysautoswitchtrigger.psc` 改为空实现并重新编译 pex：ESP/存档里的 alias 仍然有效，但不再有 10 秒轮询与 Papyrus→原生 的往返。
- 行为不变：状态优先级仍是战斗 → 城市 → 城镇 → 地牢 → 野外（含雪/雨变体），只有该 actor 配了对应状态才命中。

### 新增：状态套装支持 `<random>`

- 「状态套装」的每个状态下拉在 `<inherit>` 之后新增 `<random>`（中文 `<随机>`）。
- 选择后该状态存的是哨兵值 `<random>`：进入该状态时从全部套装里随机抽一套（尊重 Settings → Random outfit 的 `FavouritesOnly`，并排除当前套装），同一状态反复触发（战斗事件会连续来）不会每次重抽；离开再回来会重新抽。
- 选择 `<random>` 的当下也会立刻抽一次并刷新外观，不必等状态变化。
- 存档/读档、co-save JSON、`OutfitSystemData.json` 的 `stateBasedOutfits` 都保留该哨兵值；删除套装时不会误删它。

## [1.4.3] - 2026-11-15

主题：**槽位编号改成游戏里真正的 30–61、OutfitSystemData.json 全面对齐原版（数字策略 id）**。

### 修复：槽位编号加错了（显示 0–31，应为 30–61）

- 内部 BOD2 索引是 0 基（kHead=0 … kFX01=31），而 Creation Kit / xEdit / 游戏里给玩家看的是同一槽位 **+30** 的编号（头 30、盾牌 39、FX 61）。1.4.2 直接把内部索引当显示号追加，整体偏移了 30。
- 修法：`src/cpp/MenuOutfitEditor.cpp` 新增 `kDisplaySlotOffset = 30`，`SlotLabelFor()` 输出 `a_index + 30`；同时按用户提供的对照表重写 `kSlotLabels[32]` 的中文名：项链、头发, 短/长、未知 (一般来说是 面部/颈部/胸部/背部/其他/骨盆外侧/骨盆下方/右腿/左腿/面部 2/胸部 2/肩膀/左臂/右臂/其他 2)、Decapitate-Head、Decapitate、FX。`src/cpp/I18n.cpp` 词典同步（仍 224 条），语言文件已重新生成。

### 修复：OutfitSystemData.json 导入后所有套装都变成“从不显示任何东西”

- **根因**：文件里 `"slotPolicy": 2` 是**数字**（原版 0.6.x 的 protobuf 枚举），而 1.4.1/1.4.2 的导入用 `FetchString()` 读它 —— 非字符串一律返回空 → 基础策略保持默认的 `XXXX`（从不显示任何东西）；`slotPolicies` 里只有零星几个槽位是数字（如 `"9": 8`），其余 31 个槽位全部继承这个错误的基础策略。
- **修法**：`src/core/OutfitService.cpp` 新增 `FetchPolicyCode()`，`slotPolicy` 支持数字（走 `PolicyFromInt`）与字符串（走 `NormalizePolicyCode`：四字符码 / `$SkyOutSys_Desc_EasyPolicyName_*` / 中英文标签 / `CustomPolicyNNN`），认不出的只 warn。
- **导出也对齐原版**：`save_legacy_json_c()` 改为写**数字 id**（0–11，脚本策略 128+），与用户实际使用的 `OutfitSystemData.json`（`"slotPolicy": 2`、`"slotPolicies": {"9": 8}`）逐字段一致；默认值 0（XXXX）按 protobuf 规则省略。1.4.2 的更新日志里“导出统一规范化成四字符码（与原版 protobuf 一致）”这句是错的，本版更正。
- 新增 `src/core/Policy.{h,cpp}` 的 `PolicyToInt()`（`PolicyFromInt` 的逆函数）。

## [1.4.2] - 2026-11-15

主题：**快速选择窗口自己开窗（位置/尺寸真正生效、“Debug”标题消失）、JSON 策略能认中文标签、槽位名带编号、中文措辞改用“幻化”**。

### 修复：快速选择窗口不居中、不记忆位置、ini 里的数值不生效，标题还写着 “Debug”

- **根因**：判断“框架是不是已经替我开好窗口”用的是 igGetCurrentWindow() 是否为空。ImGui 每帧 NewFrame() 之后都会隐式 Begin("Debug##Default") 并把当前窗口指向它，所以这个判断**永远为真** → 插件认为窗口已存在，既不 SetNextWindowPos/SetNextWindowSize 也不 Begin，全部内容都画进了那个隐式的 Debug 窗口。用户看到的标题 “Debug” 就是它；ini 里的 QuickPickerX/Y/Width/Height 也因此一次都没被应用。
- **修法**：不再用 GetCurrentWindow 判断。src/cpp/MenuUI.cpp 新增 g_renderingSection 标志，只有 F1 菜单里的“快速选择”页（RenderQuickPickerSectionThunk）才置位；AddWindow 注册的独立窗口永远自绘 Begin/End。窗口标题改为 "<翻译标题>###SOSQuickPicker"，### 之后的稳定 id 保证换语言后窗口状态不丢。
- **几何固化**：每次打开时 g_quickGeometryForce = true → 用 ImGuiCond_Always 套用 ini 里的矩形（FirstUseEver 在同一个 ImGui 上下文里只会生效一次，所以重启后要强制）。关闭/拖动后 0.75 秒节流写回 ini。日志新增一行 Quick picker: opening at <x>,<y> with <w>x<h> (restored from the ini|centred)。
- **可选符号不再一票否决**：ImGuiApi 原来要求 14 个可选函数**全部**命中才允许开窗；igSetScrollHereY 在某些框架版本里叫 igSetScrollHereY_Float，一个名字对不上就整条窗口路径被关掉。现在改为逐符号绑定、失败只 warn（打印试过的名字），并新增 WindowPlumbingReady() 只要求 Begin/End/SetNextWindowPos/SetNextWindowSize 四个；igIsKeyDown_Nil / igIsKeyPressed_Bool / igSetScrollHereY_Float 都带备选名。

### 修复：OutfitSystemData.json 的策略被识别成“从不显示任何东西”

- **根因**：导出/导入把策略字段当成四字符码（XXXO）直接存取，但原版与老文件里可能写着本地化标签，例如“需要已装备（槽位有装备时才显示套装）”“始终使用套装”。FindPolicy() 只做精确匹配，认不出的字符串在 compute_display_set_c() 里被 continue 掉 —— 表现就是该槽位“什么都不显示”。
- **修法**：src/core/Policy.{h,cpp} 新增 NormalizePolicyCode()，接受四字符码、$SkyOutSys_Desc_EasyPolicyName_XXXX 翻译键、英文标签、**新旧两套中文标签**（1.4.1 及更早的“套装”措辞与 1.4.2 的“幻化”措辞）、CustomPolicyNNN。导出时统一规范化成四字符码，导入时先规范化再落库，认不出的值只 warn 并让该槽位继承基础策略，不再静默变成“什么都不显示”。 **（1.4.3 更正：原版 `OutfitSystemData.json` 用的是数字枚举，导出必须写数字；四字符码只是 1.4.1/1.4.2 的过渡写法，见顶部 [1.4.3]。）**
- **健壮性**：导入时 slotPolicies 的值现在同时接受字符串和数字（老枚举 0–11）；compute_display_set_c() 遇到未知策略会退回基础策略、再退回 XXOO，不再让整个槽位消失。

### 改进：槽位名带编号；中文措辞“套装”→“幻化”

- 策略编辑器里的槽位名统一加后缀：中文“头部（槽位 0）”、英文“Head (slot 0)”，32 个槽位一一对应 BIPED_OBJECT 编号，便于精确判断是哪一个槽位。
- 中文界面里“始终使用套装”“只有套装时显示套装”等 7 条策略文案改为“始终使用幻化”“只有幻化时显示幻化”等；“套装”仅保留在“套装名/套装列表”这类指代存档条目本身的地方。

### 改进：快速选择窗口的长按更灵敏

- 首次重复延迟 0.40 s → **0.22 s**，最小重复间隔 0.045 s → **0.030 s**，加速度同步调整（0.045 → 0.030）。轻轻按住方向键/十字键就能连续移动，不必长按一秒。

## [1.4.1] - 2026-11-15

主题：**老存档迁移正确了、位置记得住了、原版 JSON 导入导出回来了、翻译文件按语言拆分并覆盖全部界面文本**。

### 修复：v4 老存档的槽位策略被错认（“需要已装备”变成“始终使用套装”）

- **根因**：v4 的 protobuf 存档里槽位策略是**数字**字段（`Outfit.OBSOLETE_slot_policy = 7`、`OBSOLETE_slot_policies = 6`，`map<uint32,uint32>`），v5 起才改成字符串（字段 9/10）。`src/core/LegacySave.cpp` 只解析了字符串版，而且把 “套用策略” 整段包在 `if (a_version >= 5)` 里 —— v4 存档读进来后完全没套策略，于是留下 `Outfit` 构造函数给的默认值：`blanket = XXOO`、盾牌槽 `XEXO`。`XXOO` 正是“始终使用套装”，您设的 `XXXO`（需要已装备）就是这样被吃掉的。
- **修法**：`LegacySave.cpp` 补上字段 6/7 的解析（`map<uint32,uint32>` 与 `uint32`，同时支持 packed 与非 packed 的 repeated uint32），并在套用前把 32 个槽位与 blanket **重置成原版 `from_proto_data` 的初始态**（`blanket = XXXX`、槽位全部继承），再依次套 `slot_policies` → `slot_policy` → `OBSOLETE_slot_policies` → `OBSOLETE_slot_policy`，与原版 Rust `outfit.rs` 的顺序完全一致；不再看版本号（原版也不看）。
- **数字→策略码**：`src/core/Policy.{h,cpp}` 新增 `PolicyFromInt()`，映射与原版 `Policy::from_int` 逐条相同（0 XXXX / 1 XXXE / 2 XXXO / 3 XXOX / 4 XXOE / 5 XXOO / 6 XEXX / 7 XEXE / 8 XEXO / 9 XEOX / 10 XEOE / 11 XEOO / ≥128 → `CustomPolicy%03d`，其余忽略）。
- **影响范围**：只改老存档（co-save 版本 ≤ 6）的迁移路径；v7 co-save 与新建套装不受影响。已迁移过一次并被写成 `XXOO` 的存档，重新导入一次旧 co-save 即可恢复。

### 修复：快速选择窗口每次重启都回到屏幕中央

- **根因**：窗口位置/尺寸用的是 `ImGuiCond_FirstUseEver`，而 SKSE Menu Framework 不持久化 ImGui 的窗口状态，进程退出即丢失；插件侧也没有存过。
- **修法**：`Settings` 新增 `[UI] QuickPickerX / QuickPickerY / QuickPickerWidth / QuickPickerHeight`（默认 -1/640/800，即“没动过就居中”）。窗口每帧在 `Begin`/`End` 之间用 `igGetWindowPos` / `igGetWindowSize` 读取实际矩形，拖动结束时写回设置并延迟 0.75 秒落盘（避免拖拽时每帧写文件），关闭窗口时立即落盘。两个新符号是**可选**绑定：框架没导出时只退化成“每次居中”，其他功能不受影响。

### 新增：按原版结构导入/导出 `OutfitSystemData.json`

- **原版行为**：旧的 `ExportSettings` / `ImportSettings` 写的是 `Data\SKSE\Plugins\OutfitSystemData.json`，内容由 `protobuf_json_mapping` 生成 —— lowerCamelCase 字段名、`slotPolicies` 的 map 键是字符串、默认值字段省略。1.4.0 的导出走的是插件自己的 `version: 2` 内部格式，跟旧版互不兼容，**这是菜单里找不到导入导出按钮、旧数据也搬不过来的直接原因**。
- **修法**：`OutfitService` 新增 `save_legacy_json_c()` / `replace_with_legacy_json()`，严格按原版 proto 的 JSON 映射生成与读取：`{enabled, outfits:[{name, isFavorite, armors:[{rawFormId, localFormId, modName}], slotPolicy, slotPolicies}], actorOutfitAssignments:{"<formID>":{currentOutfitName, stateBasedAutoSwitchEnabled, stateBasedOutfits}}}`。导入端同时接受 camelCase 与 snake_case、`OBSOLETE_*` 数字策略、以及只写裸 FormID 的 `armors` 数组；套用顺序与上一条修法相同。
- **入口**：Papyrus 的 `ExportSettings` / `ImportSettings` 改为调用同一套代码（HUD 提示走翻译）；SMF 菜单 Settings → **Outfit data** 新增 **Export outfits to OutfitSystemData.json** / **Import outfits from OutfitSystemData.json** 两个按钮，导入成功后自动刷新全部角色的外观。

### 变更：翻译文件改为 `Data\interface\translations` 下按语言分文件

- 旧的单文件 `Data\SKSE\Plugins\SkyrimOutfitSystemNG_i18n.txt`（`[en]` / `[zh]` 两段）**作废并删除**；现在每种语言一个文件：`Data\interface\translations\SkyrimOutfitSystemNG_english.txt`、`SkyrimOutfitSystemNG_chinese.txt`。
- 格式改成游戏自己的翻译文件风格：`$英文原文<TAB>译文`，UTF-16LE（带 BOM），也接受 UTF-8 与 `key=value`；`\n` 表示换行，`#` / `;` 开头是注释。
- **目录里出现哪个文件，语言下拉里就多哪个选项**：加语言不需要改插件，复制一份改名（例如 `SkyrimOutfitSystemNG_german.txt`）填好即可。`auto` 仍然跟随游戏语言（`sLanguage:General`），中文还要求菜单框架开了 `[Fonts] EnableChinese` 且字体存在。
- 首次运行且目录为空时自动写出 english / chinese 两份**全量模板**（已存在的文件绝不覆盖）。

### 修复：大量界面文本没有翻译条目

- 内置词典由 153 条扩到 **224 条**。补上的正是您找不到的那些：12 条槽位策略文案（含 **Require equipped (show the outfit only when the slot is worn) / 需要已装备（槽位有装备时才显示套装）**、**Always use outfit / 始终使用套装**、`<inherit>` 等）、32 条槽位名（Head / Hair, Short / Body / Hands / … / Unnamed 44–60 / FX）、13 个状态名核对、导入导出与错误提示、以及一批用字符串拼接的消息前缀（`Outfit: `、`Added `、`Slot `、`Outfit applied to ` …）。
- 拼接式提示（`"Outfit: " + name`、`"Slot " + label + " uses " + policy`）原来永远只有英文，现改为 `I18n::T(en, zh)` 前缀，中文下整句都是中文。
- 菜单标题（左侧 `Skyrim Outfit System` / `Outfits` / `Actors` / `Settings`）沿用 1.4.0 的翻译，仍然需要重启游戏生效（框架会缓存节点名）。

### 其它

- 版本号 1.4.1（`version.h` 与 `version.rc`，DLL 属性里可见）。
- 交付包内直接附带两份语言文件，装完即是 224 条可编辑条目。

## [1.4.0] - 2026-11-15

主题：**外置可编辑翻译文件、快速选择窗口全面可操作化，以及 PAD 终于能撕到幻化外观**。

### 修复：Procedural Armor Damage 在幻化外观上完全不生效（实机反馈）

- **根因（代码级）**：`CustomSkinPlayer::Custom` 调 `TESObjectARMO::ApplyArmorAddon` 之后，引擎会把该槽的 `BipedAnim::objects[i].item` 换成**幻化件**。PAD 1.x 的 `CollectWorn` 只收背包里 `IsWorn` 的真实护甲（外加 ExtraDataList 0x16/0x17 的副本），`ScanScene` 又只保留 `BIPOBJECT.item` 属于该集合的槽 —— 于是幻化槽整槽被跳过，永远撕不到。**旧文档里写的“诚实还原”在 1.0.0–1.3.0 的代码里并不存在**，本版把它补上（文档同步修正）。
- **修法**：`Custom()` 在贴件前按已穿护甲建 `wornBySlot[32]`（槽位 → 真实护甲），每件 `ApplyArmorAddon` 之后把该槽 `item` 写回真实护甲。只撤销自己刚写入的值（`slot.item == a_applied`），真实穿着的槽、以及同一件护甲本来就占的槽都不动；外观仍由 `addon`/`part`/`partClone` 提供。还原范围同时覆盖第一/第三人称 biped。
- **开关**：`[Compatibility] ProceduralArmorDamage`（默认 on）。关掉就回到旧行为（`item` 留幻化件）。
- **关于日志里那条门禁**：您引用的「该护甲在玩家背包里至少有一个副本带 ExtraWorn 标记（真穿着）」是 PAD 的 **Papyrus `TearArmor` 门禁**，只作用于脚本调用 `TearArmor` 的路径（还要求 actor 是玩家、护甲非空）。PAD 自己撕裂走的是原生路径，**不经过这个门禁**，所以不需要也不能“绕过”它；真正拦住幻化件的是上面那条 `item` 记录。
- **仍然存在的边界**：幻化件下面**没有真实护甲**的槽（例如全身空槽上贴的裙子），背包里没有那件护甲可记损伤，PAD 依旧撕不到。日志会打 `PAD compatibility: <actor> shows pieces over slot mask 0x… with no real armour underneath`。

### 新增：外置可编辑翻译文件

- 新文件 `Data\SKSE\Plugins\SkyrimOutfitSystemNG_i18n.txt`（UTF-8）：`[en]` / `[zh]` 两段，行格式 `英文原文=译文`，`#` / `;` 注释，`\n` 换行转义。首次运行缺失时自动写出**全量模板 153 条**；已存在的文件绝不覆盖。
- 查表顺序：**文件覆盖 → 内置词典 → 英文原文**，删行安全，重复键后者生效。
- Settings → Language 新增 **Reload the language file** 按钮，并常显文件路径与读到的条目数。
- 详情见 `docs/I18N.md`。

### 新增：菜单左侧节点也走翻译

- `Skyrim Outfit System` 根节点与 `Quick picker / Outfits / Actors / Settings` 四项标题改为按当前语言生成（`I18n::Tr`）。
- 框架会把节点标题复制进自己的树，**切语言后需重启游戏**才更新，界面里已注明（页内文案仍即时生效）。

### 新增：快速选择窗口的键盘 / 手柄操作

- **上下选择**：`W`/`S`、`↑`/`↓`、手柄十字上下。
- **翻页**：`A`/`D`、`←`/`→`、手柄十字左右。点按单步；按住 0.4 s 后开始连续，间隔每次减 0.045 s（最快 0.045 s ≈ 22 次/秒）。
- **确认**：`E` / `Enter` / 手柄 A。**随机**：`R` / 手柄 Y。**关闭**：`Esc` / 手柄 B（输入框聚焦时也响应 Esc）。
- **焦点记忆**：打开窗口时焦点落在当前穿着的套装上；随机换装后焦点自动跳到抽中的套装并滚动到可见位置；搜索/仅收藏过滤变化时夹紧焦点不越界。
- 键盘/手柄事件由菜单框架转成 ImGui 按键事件，因此只在框架窗口打开时可用（游戏本身不吃这些键，冲突最小）。

### 新增：快速选择窗口自绘窗口

- 菜单框架只调用渲染函数、**不提供 `Begin/End`**，1.3.0 的快速选择窗口其实是被 ImGui 当成隐式调试窗口画出来的（所以“默认尺寸太小”）。本版改为自己 `Begin/End`，默认 **640×800**、屏幕居中（`ImGuiCond_FirstUseEver`，之后记住用户拖动的位置）；窗口右上角关闭按钮会同步回框架的 `IsOpen`。
- 新增 **Remove outfit（取消幻化）** 按钮：清空玩家当前套装并刷新外观，HUD 提示「Outfit removed.」。

### 新增：窗口尺寸与分页可调并持久化

- 新 ini 键（`[UI]`）：`OutfitListWidth`（240–1200，默认 470）、`OutfitPageSize`（5–500，默认 50）、`QuickPickerPageSize`（5–500，默认 20）。
- Outfits 页与快速选择窗口里的滑条直接写这两个分页值，Settings → Interface 段可统一调；**任何改动立即 `Save()` 回 ini**，不再每次打开都是默认值。

### 改进

- **Outfits 列表与快速选择列表**：行宽按文本长度撑开（`CalcTextSize` + 按钮预留 150 px），子窗口启用 `ImGuiWindowFlags_HorizontalScrollbar`，长名不再被右侧按钮截断；列表宽度可调（见上）。
- **双击应用**改为「鼠标悬停在该行上」才判定，避免双击空白处误触发换装。
- 快速选择窗口底部的 Random / Remove outfit / Close 三个按钮与一行键位提示。
- I18n 内置词典 135 → 153 条（新增 Outfits / Settings / List width / Page size / Remove outfit / 键位提示 / 重新加载提示等）。

### 文档

- 新增 `docs/I18N.md`（格式、第三种语言、覆盖范围、排查）。
- 修正 `docs/HOOKS.md` 与 `docs/ARCHITECTURE.md` 中“诚实还原”的描述：1.3.0 及更早版本实际上没有实现，1.4.0 起才存在。
- `docs/TESTING.md` 新增 §3.9（1.4.0 用例）。

## [1.3.0] - 2026-11-15

主题：**把原版 MCM 的细节功能在 SMF 菜单里补齐**。原版 `SkyOutSysMCM.psc` 有 1,099 行、22 个控件家族，1.2.0 的 SMF 菜单只覆盖了其中一半（套装列表 + 角色列表 + 设置）。本版逐控件对照补齐，新增 `docs/MCM_PARITY.md` 记录每一条的原版位置与等价关系。

### 新增：套装编辑器（原版 `ShowOutfitEditor` 整块）

原来只能在 Papyrus MCM 里做的事，现在在 Outfits 页右栏完成：

- **四种添加方式**：从背包（`GetCarriedArmor`）、从当前穿戴（`GetWornItems`）、按十六进制 FormID（校验「不存在」与「不是护甲」两种错误）、从全护甲表搜索（名字或插件名过滤 + 仅可玩，跳过模板护甲与无名条目，结果上限 400 条）。
- **槽位冲突确认**：加入的护甲与套装已有件占用同一 BOD2 槽时，先弹内联确认（`Replace the conflicting pieces` / `Cancel`），确认后按槽掩码交集移除冲突件再插入——与原版 `RemoveConflictingArmorsFrom` 同语义。
- **槽位清单**：把套装里每件护甲按其槽掩码展开成「槽名：护甲名」行，每行带 `Remove`（带确认，对应原版 `$SkyOutSys_Confirm_RemoveArmor_Text`）。
- **逐槽策略**：Base policy 下拉（12 个策略码，文案取自 `$SkyOutSys_Desc_EasyPolicyName_*` 的语义）+ `Per-slot policy editor` 复选后展开 32 个槽位下拉（每槽含「Use base policy」）+ `Reset all policies to the defaults`（与原版 `OutfitEditor_EditAllSlotPolicy` / `BodySlotPolicy_<slot>` 对齐）。
- **套用目标**：编辑器顶部 `Wear` / `Take off` / `Favourite`，作用于 Outfits 页 `Apply to` 选中的角色。

### 新增：Actors 页与原版 Options 页对齐

- **Apply to 选择器**：对应原版 `OPT_SelectActorSelection`——选中的角色会接收列表的 Wear/双击、编辑器的 Wear/Take off，列表里当前穿戴该套装的角色会显示 `* ` 前缀（原版 ActiveMark）。
- **Add a nearby actor...**：对应原版 `OPT_AddActorSelection`，列出同 cell 内未死亡、FormID 允许的 NPC。
- **移除角色带确认**：对应原版 `OPT_RemoveActorSelection`，玩家不可移除。
- **快捷槽法术开关**：对应原版 `OPT_QuickslotsEnabled`。原版通过 `SkyOutSysQuickslotManager.SetEnabled()` 给玩家加/去法术，本版改为原生实现（EditorID `SkyrimOutfitSystemQuickslotSpell`，失败回退 `SkyrimOutfitSystem.esp:0x000182F`），在 `kDataLoaded` / `kNewGame` / `kPostLoadGame` 三处同步，状态写 `[General] QuickslotsEnabled`。

### 改进

- **删除/移除确认**：套装删除、角色移除都改成内联确认条（SMF 没有弹窗 API），文案显式说明不可撤销。
- **名字长度校验**：新建与改名都按原生 `GetOutfitNameMaxLength()`（256 字节）在界面层挡住，并在下方常显 `Max %zu bytes per name.`；重名与不存在分别提示。
- **中文词条**：I18n 词典从约 85 条扩到 135 条，覆盖编辑器、Apply to、确认条、版本行等新界面。
- **脚本兼容**：`SkyOutSysMCM.psc` 仍是空壳，但补回了原版对外暴露的 `GetModVersion()`（返回 0x01030000）、`GetVersion()`、`GetQuickslotManager()` 三个接口，第三方脚本无需改动；`SkyOutSysMCM.pex` 因此重新编译（703 → 991 字节）。
- **Settings 页**：新增 `Version 1.3.0` 行与快捷槽法术复选。

### 文档

- 新增 `docs/MCM_PARITY.md`：原版每个控件的对照表、12 个策略码、已知差异（没有右键菜单、没有 InfoText、没有逐项 Restore defaults、快捷槽改成原生窗口、Export 只含套装与角色分配）以及两项未实现项的原因。

## [1.2.0] - 2026-11-15

主题：**菜单中英双语**，以及针对“挨打时幻化衣服没有变化 / PAD 的 Tracked armors 列表是空的”这一实机反馈补上**可自证的诊断**。兼容层仍然坚持零 hook、零补丁。

### 新增

- **菜单语言切换**：Settings 页新增 `Language` 下拉（自动 / English / 中文），写入 ini 新段 `[UI] Language`（`auto` / `en` / `zh`），改动立即保存并生效。新模块 `src/cpp/I18n.{h,cpp}`：静态词典（约 85 条界面文案，含 13 个状态名），翻译挂在 `IG::` 包装层，所以菜单代码继续写英文字面量；控件标签用 `中文###English` 形式，ImGui 的控件 id 仍是英文，切语言不会丢状态、也不会撞 id。
- **中文字形的判定**：中文能否显示完全取决于菜单框架自己的字体图集。插件只读它的公开配置——`Data/SKSE/Plugins/SKSEMenuFramework.ini` 的 `[Fonts] EnableChinese` 与 `PrimaryFont`（默认 `CN.ttf`），并检查 `Data/SKSE/Plugins/Fonts/<PrimaryFont>` 是否存在。不满足时界面保持英文，日志写明缺什么，不会画一屏方块；没有任何字体注入或 hook。
- **PAD 槽位诊断**：Settings → Mod compatibility 新增 **Log slot report** 按钮，调用 `Compat::Pad::Diagnose`，把玩家逐槽写成 `PAD slot report: slot  N (0xM) shown 'X' | real 'Y'`，并先列出全部真实穿着的护甲；`real '<none>'` 的槽位就是 Procedural Armor Damage 摸不到的地方。
- **孤儿槽位常驻提示**：`OnOutfitApplied` 现在会在「幻化覆盖了没有真实护甲打底的槽位」时打一条 INFO（仅在掩码变化时打，不刷屏），不再要求用户先开 `ExtraLogging`。

### 说明（重要，回答“PAD 的 Papyrus API 能不能用”）

PAD 1.0.4 公开的 `ArmorTear` 函数（`TearArmor` / `GetDamagedWornArmor` / `GetArmorIntegrity` …）**不能**用来解决幻化件：

- `TearArmor` 反汇编后有三道门禁：actor 必须是玩家、护甲非空、且该护甲在背包里**至少有一个副本带 ExtraWorn 标记**，否则直接返回 -1。幻化件不在背包里，永远过不了这道门。
- PAD 的 `tearing::CollectWorn` 只收「背包里 IsWorn 的护甲」，`ScanScene` 只处理「biped 记录里的 item 属于 CollectWorn 集合」的槽位。幻化件既不在背包，也不是 biped 记录里的 item（幻化后 item 被如实还原成真实护甲）。

所以兼容只能靠“真实护甲打底”：有真实护甲的槽位，PAD 撕的是你**看得见**的那层 mesh，损伤记在真实护甲上；没有真实护甲的槽位，PAD 无处记损伤。本版把这件事做成可自证的日志与一键报告，而不是去 patch PAD。

### 变更

- `Settings` 新增 `language`（`[UI] Language`），`Save()` 会写回；随包 ini 增加 `[UI]` 段与注释。
- `PadCompat` 的 `OnOutfitApplied` 改为按 actor 记住孤儿槽位掩码，只在变化时打日志；逐件明细降为 debug。
- 版本号 1.1.0 → **1.2.0**。

### 验证

- 编译 BUILD_EXIT=0（0 error / 0 warning）；DLL 1,522,688 字节。
- 导入表仍只有 KERNEL32 / USER32 / ole32 / VERSION / SHELL32，delay-import 为 0；导出 `SKSEPlugin_Load` / `_Query` / `_Version`；空目录可单独加载。
- Papyrus 端本轮无改动（仍是 6 个 pex / 6 个 psc，66 个原生函数）。
- **游戏内行为仍未实机验证**（本机无 Skyrim）。

## [1.1.0] - 2026-11-14

主题：移除感知功能（Perception），改为对 **Procedural Armor Damage** 与 **Advanced Nudity Detection** 的定点兼容，且**不使用 hook、入口补丁一类侵入手段**——只通过对方自己发布的数据（背包状态、阵营等级）工作，对方升级后仍然有效。

### 移除

- **感知层整体删除**：删除 `src/cpp/Perception.cpp` 与 `src/cpp/include/Perception.h`；Papyrus 端删除 `IsPerceptionEnabled` / `SetPerceptionEnabled` / `GetPerceivedArmor`，注册名 69 → **66**；ini 删除 `[Perception]` 段与 `[Debug] InstallPerception`。原因：实机日志里从未出现 `Perception:` 行，说明相关判定走的是引擎条件（`ExtraWorn` 标记，地址不在 Address Library），重绑 Papyrus 入口解决不了问题。

### 新增

- **兼容层 `src/cpp/compat/`**：`DisplaySet`（按当前幻化计算显示集，替代原 Perception 里的同类逻辑）、`ModuleInfo`（模块路径与版本）、`PadCompat`、`AndCompat`、`ModCompat`（入口与状态）。ini 新增 `[Compatibility] AdvancedNudityDetection` / `ProceduralArmorDamage` 两个开关，默认开启；Settings 页新增 “Mod compatibility” 段显示检测结果。
- **Procedural Armor Damage 兼容**：不加任何补丁。PAD 判定可撕件的门禁是“该护甲在背包里处于 worn 状态”且“场景里找得到对应模型”，本插件换上幻化外观后如实把真实护甲写回 biped 记录，因此 PAD 照常工作，损伤记在真实护甲上。只有“幻化件盖在没有任何真实护甲的槽位”时 PAD 看不见（模型不是它认识的护甲），`[Debug] ExtraLogging` 会在日志里逐件列出。
- **Advanced Nudity Detection 兼容**：按幻化后的覆盖情况写 AND 的三个阵营——`AND_NudeActorFaction` / `AND_ToplessFaction` / `AND_BottomlessFaction`（等级 1 = 裸体/上身空/下身空）。AND 自己的 Papyrus 接口（`AND_ModEventListener.GetNude/GetTopless/GetBottomless`）内部就是读这三个阵营，所以对话条件、依赖它的其它 mod 都会跟着变；幻化成空套装即判为裸体。阵营按 EditorID 解析，AND 升级只要不改 EditorID 就继续有效；找不到阵营时只记一条 warn 并退回原版判定。
- **防漂移**：AND 会按它自己的节奏（装备事件、游戏时间 tick）重扫真实装备并覆盖我们写的等级，所以每次换装刷新后立即同步一次，另有输入事件心跳（节流 1 秒，跳回主线程）补写。

### 变更

- **覆盖判定是近似值**：只看护甲槽位掩码（`Body` / `ModChestPrimary` / `ModChestSecondary` 算上装；`Body` / `ModPelvisPrimary` / `ModPelvisSecondary` / `ModLegRight` / `ModLegLeft` 算下装），不按 AND 的关键词（curtain / top / bottom / bra / underwear）做精判。护甲件覆盖不全的混搭套装可能被判成“有穿”。
- **检测靠模块名**：`AdvancedNudityDetection.dll` / `ProceduralArmorDamage.dll`，改名即视为未安装（只影响兼容功能，不影响幻化本体）。

### 说明

- 本轮全部为静态验证：DLL 1,489,920 B，导入表只有 KERNEL32 / USER32 / ole32 / VERSION / SHELL32，导出 `SKSEPlugin_Load` / `_Query` / `_Version`，空目录可单独加载。**游戏内行为仍未实机验证。**

## [1.0.3] - 2026-11-14

针对 1.0.2 的三项实机反馈：法术直接随机换装、菜单里找不到相关设置项、NPC 依旧察觉不到幻化。

### 修复

- **“Change Outfit” 法术默认打开快速选择窗口**：上一版的行为由魔法效果上的脚本属性 `RandomOnCast` 决定，而 ESP 的 VMAD 里存的是 `True`，脚本里的默认值改不动它，所以法术永远走随机分支。现在改为读取原生函数 `IsQuickSlotRandom()`（ini `[RandomOutfit] QuickSlotRandom`，默认 off），`RandomOnCast` 属性仅作为遗留兼容保留，不再决定行为。

### 新增

- **设置菜单补齐缺项**：Settings 页新增“快速选择窗口热键”输入框（写 `[RandomOutfit] QuickPickerHotkey`，默认 ctrl+shift+L），随机换装热键独立成行，并新增“The Change Outfit power rolls a random outfit”勾选框（对应 `QuickSlotRandom`）。菜单里的改动会**立即写回** `Data/SKSE/Plugins/SkyrimOutfitSystemSE.ini`（新增 `Settings::Save()`），重开游戏仍然保留；ini 里手写的注释不会被覆盖。
- **Papyrus API**：新增 `IsQuickSlotRandom()` / `SetQuickSlotRandom(bool)`，注册名 67 → **69**，`SkyrimOutfitSystemNativeFuncs` 声明同步更新。
- **修掉一个重复注册**：`GetOutfitNameMaxLength` 在 1.0.2 之前被注册了两次（第一次没带 static 标志），Papyrus 侧可能拿到非静态版本；本版只保留正确的静态注册。
- **[Debug] ExtraLogging 增加感知层取证**：开启后每次脚本查询都会打印 `Perception: GetWornForm(illusion|vanilla|bare skin)(<actor>) -> <armor|None>` 与 `Perception: WornHasKeyword(XXXXXXXX) -> true/false`。用它可以直接判断某个 mod 到底是走 Papyrus 还是走引擎条件。

### 感知层的边界（重要）

Papyrus 入口（`Actor.GetWornForm`、`Actor.WornHasKeyword`）已经覆盖，幻化与“空槽 = 裸体”在脚本侧成立。

但**引擎内部的对话 / 台词条件**（TESCondition 的 `WornHasKeyword`=682、`WornApparelHasKeywordCount`=722 一类）读的是真实装备的 `ExtraWorn` 标记，条件函数表地址不在 Address Library 里，CommonLibSSE-NG 也没有暴露，插件无法安全改写：想把这类判定骗过去，只能让衣服真的穿上（那就不是幻化了），或者给那个 mod 的对话条件打补丁。

怎么快速分辨：打开 `[Debug] ExtraLogging`，触发一次 NPC 台词；日志里若完全没有 `Perception: GetWornForm` / `WornHasKeyword` 行，说明该判定在引擎侧，Papyrus 重绑管不到（这是引擎限制，不是本插件漏做）。护甲值、perk、套装加成始终读真实护甲，这一点没有变。

## [1.0.2] - 2026-11-14

本版针对两份实机报障：快捷换装法术「一点反应没有」，以及 NPC / 脚本察觉不到幻化。

### 修复

- **所有 Papyrus 原生函数失效（根因）**：上一版 `dist/Scripts` 只打包了 5 个 pex，**漏了 SkyrimOutfitSystemNativeFuncs.pex**。这个脚本只有 Global Native 声明、没有函数体，但 Papyrus 必须先成功创建这个类，才能把原生函数绑上去；缺 pex 时日志先报 `Cannot open store for class "SkyrimOutfitSystemNativeFuncs", missing file?`，随后 63 条 `Native static function ... could find no matching static function on linked type`，于是快捷槽法术、自动换装触发器、战斗检测脚本调原生函数全部返回 None。现在 6 个脚本一起编译、一起打包，并纳入交付校验清单。
- **「Change Outfit」法术改为弹出原生快速选择窗口**：调用新增的 OpenQuickPicker()，窗口提供搜索框 + 仅收藏 + 分页 + 单击应用。原版用 Papyrus 列表菜单，超过约 100 页会栈溢出；原生窗口没有这个上限。菜单框架缺失时法术会给出明确提示，而不是静默无反应。
- **NPC 察觉不到幻化（根因）**：Actor.GetWornForm 的重绑只在 kDataLoaded 做过一次。Papyrus VM 在读档时会重建类型表，把被替换的函数还原成原版实现，所以「读档后进游戏」这条最常见的路径上，感知层其实一直是原版行为。现在 kDataLoaded / kNewGame / kPostLoadGame 各重绑一次（SOS::Perception::Install()）。
- **新增 Actor.WornHasKeyword 重绑**：大量检测脚本判定「目标是否穿着某关键词护甲」（吸血鬼 / 狼人检测、WornHasKeyword 条件、部分任务脚本）走的是这个入口，原版未覆盖。现在与 GetWornForm 共用同一份显示集判定。
- **空幻化槽按裸体处理**：显示集里没有该槽位的件时，GetWornForm 返回 None、WornHasKeyword 返回 false。空的幻化套装 = 全身裸体，符合需求。

### 新增

- **快速选择窗口**：MenuUI 新增独立窗口（MenuBridge::AddWindow），搜索 / 仅收藏 / 首页 上一页 页码 下一页 末页 分页（每页 20 条）/ 单击应用 / Random / Close。它同时作为菜单页 「Skyrim Outfit System/Quick picker」存在，菜单框架不能创建独立窗口时自动退化为普通页。
- **热键**：新增 [RandomOutfit] QuickPickerHotkey（默认 ctrl+shift+L）切换快速选择窗口；随机换装热键仍是 ctrl+shift+O。
- **Papyrus API**：OpenQuickPicker() / CloseQuickPicker() / IsQuickPickerVisible() / GetPerceivedArmor(actor, slotMask)（诊断用，返回幻化在该槽位报出的护甲，无则 None）。

### 已知限制

- 感知层只覆盖 Papyrus 入口（Actor.GetWornForm、Actor.WornHasKeyword）以及经由它们的引擎逻辑。直接调用 Actor.GetWornArmor / GetInventory 的第三方 DLL（以及 CommonLibSSE-NG 内联的这些函数）仍看到真实护甲 —— 那些是内联实现，没有可挂钩的单一入口。
- 本机无 Skyrim，以上修复均未实机验证。

## [1.0.1] - 2026-11-14

本版针对实机反馈的「载入存档卡死 / 无限载入」与架构返工。

### 修复

- **载入存档无限载入（根因）**：`OutfitService` 的读写守卫（`OutfitServiceReadGuard` / `OutfitServiceWriteGuard`）只有构造函数加锁、**析构函数没有解锁**。这两个守卫是照抄原 Rust/cxx 桥的 RAII 形态，Rust 侧靠 Drop 释放，改写成 C++ 时漏了析构。后果：任何一次 `outfit_service_get_singleton_ptr()`（存档回调、Papyrus 调用、钩子判定都会调）之后，递归互斥锁就被那个线程永久持有；读档时主线程持有它，渲染线程随后在幻化钩子里调用同一个服务 → 永久阻塞 → 载入界面卡死。现在两个守卫都补了析构解锁，且禁止拷贝。
- **读档期间触碰半成品 actor**：新增 `src/cpp/GameState.{h,cpp}`（`SOS::GameState::IsLoading()`）。`kPreLoadGame` / `kNewGame` / 存档 Revert 回调置为「加载中」，`kDataLoaded` / `kPostLoadGame` 置回。加载中 `Hooking::ShouldOverrideSkinning` 一律返回 false（五个幻化钩子走原版路径），`Actor.GetWornForm` 重绑也退回原版实现。
- **`refreshArmorFor` 守卫**：actor 没有 3D（`!Is3DLoaded()`）或没有 AIProcess 时直接跳过并记 debug，不再对读档中的 actor 调 `SetEquipFlag` / `UpdateEquipment`。
- **`kPostLoadGame` 不再立即刷新**：新逻辑 `SchedulePostLoadRefresh` 每帧重排一个任务，等玩家 3D 真正就绪（最多 600 次尝试）后再 `CreateRefreshAllArmorsTask()`。
- **`replace_with_new()`** 现在同时清空套装表（对齐原 Rust 行为），玩家指派由 `check_consistency()` 补回。
- **`remove_actor`** 增加玩家单例空指针保护，避免极早期调用崩游戏。
- **旧 co-save 迁移**：新增 `src/core/LegacySave.cpp`，手写 protobuf 解码器读取原版 v4/v5/v6 记录（`AAOS` / `cOft`），迁移套装、收藏、逐槽策略、护甲（`ResolveFormID` + `LookupForm(local, plugin)` 双路径）与 v6 的 actor 指派；v4 的套装回落到默认槽策略，v5 及更早的 handle-key 指派按原版行为丢弃并记日志。之前只是「跳过并提示」，现在老存档的套装能带过来。

### 变更

- **钩子实现合并为单套 CLIB-NG 表**：删除 `Hooks_PRE_AE.cpp` / `Hooks_AE.cpp` 两份平行实现与 `HookingPREAE` / `HookingAE` 命名空间，统一为 `src/cpp/hooking/Hooks.cpp`，全部游戏地址只用 `REL::RelocationID(seID, aeID)` 单表声明（`24232/24736`、`24220/24724`、`24231/24725`、`36979/38004`、`37589/38627` 及五个被调函数），只有「注入的机器码本身不同」的两处（`ShimWornFlags` / `CustomSkinPlayer` 的寄存器分配、`FixSkillLeveling` 的 AE 内联路径）才按 `REL::Module::IsAE()` 分支。
- `HookSupport` 改为接受 `REL::RelocationID` 的 `IdExists` / `RequireIds` / `ResolveSite` 重载。
- 日志新增读档阶段行：`New game / pre-load: outfit data reset, hooks paused ...`、`Post-load: hooks resumed ...`、`Player 3D is ready after N attempt(s); refreshing the outfits.`、`Migrating a vN record written by the original mod (N bytes).`、`Migrated co-save vN: N outfits, N actor assignments.`

## [1.0.0] - 2026-11-14

首个 CommonLibSSE-NG 移植版本，基于 Skyrim Outfit System SE 0.7.0。

### 变更

- **移植到 CommonLibSSE-NG**：同时支持 Skyrim SE 1.5.97 与 AE 1.6.1170+（Address Library 驱动），VR 不支持。
- **Rust 数据层改写为 C++**（`src/core`）：套装、指派、13 状态换装、32 槽策略 + blanket、JSON 存档，行为与原版对齐。
- **移除 Rust Web UI 与 `[Server]` 配置**，设置改为游戏内菜单。
- **设置菜单改用 SKSE Menu Framework**：Outfits / Actors / Settings 三页；运行期绑定，框架缺失不影响其它功能。
- **`SkyOutSysMCM.psc` 改为空壳存根**，只为兼容旧存档的 quest 绑定；SkyUI MCM 不再使用。
- **快捷槽法术重写**：调用原生 `RandomOutfit` / 提示打开菜单，移除 UIExtensions 依赖，不再受 Papyrus 列表菜单约 100 页限制。
- **新增随机换装热键**（默认 `Ctrl+Shift+O`）：DIK 扫描码组合键、边沿触发、可只选收藏、可排除当前套装。
- **新增感知层 Perception**：重绑 `Actor.GetWornForm`，使 NPC/脚本看到幻化件，数值仍读真实护甲。
- **新增 6 个 Papyrus 函数**：`RandomOutfit`、`GetOutfitCount`、`GetOutfitNameByIndex`、`SearchOutfits`、`IsPerceptionEnabled`、`SetPerceptionEnabled`。
- **钩子健壮化**：安装前检查 Address Library ID 是否存在、站点首字节是否为 call/jmp；失败只禁用单个钩子并记录日志。
- **独立 trampoline**：64 KB 私有内存（.text ±1 GB 内分配），不再依赖 SKSE branch pool；推迟到 `kDataLoaded` 创建。
- **静态链接**：静态 CRT + `x64-windows-static`，DLL 导入表只剩系统库。
- **日志**：`SkyrimOutfitSystemNG.log`，`[Debug] ExtraLogging` 控制 trace 级。

### 修复（相对原版 / 移植过程中）

- `GetSlotMask()` 在 NG 返回 `REX::EnumSet`，全部调用点补 `.underlying()`。
- `InventoryChanges::IItemChangeVisitor` 的 vtable 在 NG 有 `ShouldVisit` / `Unk_03`，自建 augment 补齐虚函数，避免引擎取返回值读到垃圾。
- `TESNPC::IsFemale()` 在 NG 不存在，改用 `GetSex() == SEX::kFemale`。
- `RE::DebugNotification` 在 NG 不存在，改用 `RE::SendHUDMessage::ShowHUDMessage`。
- `BipedAnim::actorRef` 在 NG 是 `ObjectRefHandle`，`FixSkillLeveling` 里按 `NiPointer` 解引用。
- `SetOutfit` 的冲突检测不再把 `FirstPersonFlag::kNone` 当 0 比较。
- `SetSelectedOutfit(actor, "")` 现在真的清除指派（原 Rust 版会创建空名套装）。

### 已知限制

- 未在真实游戏内验证（开发机无 Skyrim）。
- 旧 co-save（protobuf，记录版本 ≤ 6）不迁移，只记日志跳过。
- VR 未构建。
