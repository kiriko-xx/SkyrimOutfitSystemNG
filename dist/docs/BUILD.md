# 构建说明（BUILD）

## 1.5.46 构建与交付复核状态

**验证状态（1.5.46）**：尚未实机验证。Release 重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_46.log`）。

- **本版改动面**：`src/core/EspOutfits.cpp`（整文件重写，782 行：`Decode` / `Normalize` / `LabelRange` / `CommonEnd` / `KeyOf` / `Piece` / `BuildTrie` / `SubtreePieces` / `Classify` / `PlanCostume` / `BuildVariants`）、`src/core/EspOutfits.h`（头注释改五阶段说明）、`src/cpp/include/version.h`（PATCH 46）、`CMakeLists.txt`（1.5.46）。**`MenuUI.cpp` / `PluginScan` / `GoldenItems` / 钩子层 / 贴件 / 槽清理 / `.tri` / 场景套装全部未改。**
- **产物**：`OutfitSystemNG.dll` 2,097,664 B / sha256 `220994b5b2eeb1d8bbe7c4735fd1ee23c115982110daac8bc51574c7b9957015` / FileVersion=ProductVersion 1.5.46.0（`build/Release` 与 `dist/SKSE/Plugins` 一致；旧名 `SkyrimOutfitSystemNG.dll` 不得留在 Data）。
- **构建后必查（字符串）**：应命中 `costume(s)`、`cannot be matched by name across slots`、`would wear two pieces on one slot`、`with the same name and slot; the first one is used.`、`more alternatives than fit in one run`、`1.5.46`；应 **0 命中** `name group(s)`（1.5.45 的旧分组行）。注意 `PlanCostume` / `BuildTrie` 等内部符号在 Release 会被内联，不会出现在字符串表里（这是正常的，别据此判断改没改）。
- **阶段 1 有哨兵注释**：`// --- stage 1 begin: tests/esp_outfits_trie.ps1 copies this block verbatim ---` 与 `// --- stage 1 end ---` 包住 `struct Unit` 到 `KeyOf`。**改这一段时必须保留这两行**，否则 `tests/esp_outfits_trie.ps1` 抽不到块会直接失败。
- **静态守卫**：`tests/esp_outfits_names.ps1` 打印 `ESP_OUTFITS_NAMES_OK`、`tests/esp_outfits_trie.ps1` 打印 `ESP_OUTFITS_TRIE_OK`（后者会用 `vcvars64.bat` + `cl /std:c++20` 真编译一次，cl 不在 PATH 时脚本自己写 `build.bat` 调 vcvars）。跑法：`C:/Program Files/PowerShell/7/pwsh.exe -NoProfile -File tests/<名字>.ps1`，env 需带 `PATHEXT=.COM;.EXE;.BAT;.CMD`。
- **交付两种包**：①全量包 `SkyrimOutfitSystemNG-1.5.46.zip`（`build/pack_1_5_46.ps1`，31 条目，dist 全量 + `SHA256SUMS.txt`）；②测试包 `SkyrimOutfitSystemNG-1.5.46-test.zip`（`build/pack_1_5_46_test.ps1`，2 条目：`SKSE/Plugins/OutfitSystemNG.dll` + 说明文本），可直接用 MO2 装。
## 1.5.45 构建与交付复核状态（历史，已被 1.5.46 取代）

**验证状态（1.5.45）**：尚未实机验证。Release 全量重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_45.log`）。

- **本版改动面**：`src/core/EspOutfits.cpp`（整文件重写：切词 / 最长共享前缀分组 / `BuildVariants` / `kMaxOutfits`）、`src/core/EspOutfits.h`（头注释）、`src/cpp/MenuUI.cpp`（删 `ParseLocalId` 与 `##goldenlocalid`，新增 `RenderGoldenPieceList` / `AddedIdsOf`，两处入口改列表）、`src/cpp/I18n.cpp`（11 条文案）、`src/cpp/include/version.h`、`CMakeLists.txt`、两个新守卫脚本。
- **构建后必查**：源码里 `ParseLocalId(` 与 `##goldenlocalid` 必须 **0 处**；`RenderGoldenPieceList(` 必须 **3 处**（1 定义 + 2 调用）；DLL 里应能搜到 `Add one piece from a plugin` / `Search pieces...` / `slot armour record(s) in this plugin` / `[in the list] %s  %s` / `name group(s)` / `Menu: golden piece ` / `1.5.45`；`Add one piece by id...` 应 **0 命中**。
- **静态守卫**：`tests/golden_piece_picker.ps1` 打印 `GOLDEN_PIECE_PICKER_OK`、`tests/esp_outfits_names.ps1` 打印 `ESP_OUTFITS_NAMES_OK`。跑法：`C:/Program Files/PowerShell/7/pwsh.exe -NoProfile -File tests/<名字>.ps1`，env 需带 `PATHEXT=.COM;.EXE;.BAT;.CMD`。
- **交付两种包**：①全量包 `SkyrimOutfitSystemNG-1.5.45.zip`（`build/pack_1_5_45.ps1`，31 条目，dist 全量 + `SHA256SUMS.txt`）；②测试包 `SkyrimOutfitSystemNG-1.5.45-test.zip`（`build/pack_1_5_45_test.ps1`，2 条目：`SKSE/Plugins/OutfitSystemNG.dll` + 说明文本），可直接用 MO2 装。
## 1.5.44 构建与交付复核状态（历史，金件页仍要求手输 FormID、套装不按服装名分组）

**验证状态（1.5.44）**：尚未实机验证。Release 全量重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_44.log`，末尾有 `Deploying OutfitSystemNG.dll to dist/SKSE/Plugins`）。

- **本版改动面（只修金件组行的 id）**：`src/cpp/MenuUI.cpp`（`RenderGolden()` 组行循环：`std::size_t rowIndex = 0;` + `IG::PushID(static_cast<int>(rowIndex));` + 循环尾 `++rowIndex;`，并把选中日志从 `debug` 提到 `info` 带位置）、`src/cpp/include/version.h`（PATCH 44）、`CMakeLists.txt`（VERSION 1.5.44）、新增 `tests/golden_row_id.ps1`。
- **构建后必查**：源码里 `PushID(group.plugin.c_str())` 必须 **0 处**；DLL 里应能搜到 `selected ({} of {})` / `Menu: golden group ` / `[not saved] The changes live in memory only` / `1.5.44`；DLL 大小 2,042,368 字节、sha256 `859d81d55db6de8b91224a96f998f03f32caf34dd96d8344f9d4134d0640914f`、FileVersion `1.5.44.0`。
- **静态守卫**：`tests/golden_row_id.ps1` 必须打印 `GOLDEN_ROW_ID_OK`。跑法：`C:/Program Files/PowerShell/7/pwsh.exe -NoProfile -File tests/golden_row_id.ps1`，`env.PATHEXT` 要有 `.COM;.EXE;.BAT;.CMD`（PowerShell 5.1 会让部分脚本假失败）。
- **交付两种包**：①全量包 `SkyrimOutfitSystemNG-1.5.44.zip`（`build/pack_1_5_45.ps1`，31 条目，dist 全量 + `SHA256SUMS.txt`）；②测试包 `SkyrimOutfitSystemNG-1.5.44-test.zip`（`build/pack_1_5_44_test.ps1`，2 条目：`SKSE/Plugins/OutfitSystemNG.dll` + 说明文本），可直接用 MO2 装。

## 1.5.43 构建与交付复核状态（历史，其组行仍用指针当 ImGui id，金件列表点不中）

**验证状态（1.5.43）**：尚未实机验证。Release 全量重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_43.log`，末尾有 `Deploying OutfitSystemNG.dll to dist/SKSE/Plugins`）。守卫测试 18 项 PASS（新增 `tests/golden_local_id.ps1`，并同步修掉 `tests/esp_outfits_filter.ps1` 里因重构失效的单件查找断言），另 2 项是既有环境失败（`hair_tint_read_only.ps1` 锚点已随代码迁移消失、`skse_listener_replay.ps1` 需现场 `bug/skse64.log`），不是本版回归。

- **本版改动面（只修金件页三处）**：`src/core/PluginScan.h` / `.cpp`（`Index` 增 `light` 表，新增 `IsLightPlugin()` / `LocalIdFor()` / `ResolveForm()`，`ResolveArmor()` 改走它们）、`src/core/GoldenItems.cpp`（`Normalise()` 归一化 id、`AddArmor()` / `AddAllFromPlugin()` / `AddLocal()` / `Resolve()` 改走新接口）、`src/cpp/MenuUI.cpp`（`g_goldenDirty`、`GoldenRow()`、双栏宽度夹紧、四处自动保存改置脏、单件行改用 `ResolveForm()`、`Remove` 按钮挪到行首）、`src/cpp/include/version.h` PATCH 43、`CMakeLists.txt` VERSION 1.5.43，外加 `tests/golden_local_id.ps1` 与 `tests/esp_outfits_filter.ps1`。`MorphRefresh.cpp` / `SceneOutfits.cpp` / `Patches.cpp` / `Hooks.cpp` / `HookSupport.cpp` / `I18n.cpp` / `main.cpp` 必须是空 diff（`main.cpp` 的旧 ini 一次性迁移仍会自己 `SaveToDisk()` 一次，属刻意保留）。
- **构建后必查**：DLL 里能搜到 `[not saved] The changes live in memory only` / `Changes reach the file only when you press Export.` / `Menu: golden group ` / `No armour with that id was found` / `1.5.43`；旧串 `No slot armour with that id was found` 必须 **0 命中**；`LocalIdFor` / `IsLightPlugin` / `ResolveForm` 是内部符号，Release 内联后通常**搜不到**（属正常，别当成没改）。版本资源应为 `1.5.43.0`，DLL 2,041,856 字节 / `349a3e0f…`。
- **静态守卫**：`tests/golden_local_id.ps1` 必须打印 `GOLDEN_LOCAL_ID_OK`（light 分支掩 `0xFFF`、`ResolveForm` 先 compact 后 wide、`ResolveArmor` 不再裸 `LookupForm`、金件三处 `LocalIdFor`、`RenderGolden()` 内 `Golden::SaveToDisk()` 恰好 1 次、`g_goldenDirty` 与两条提示串、`GoldenRow(` 用于两处列表、`std::max(240.0f, avail - 320.0f)` 夹紧）。跑法：`C:/Program Files/PowerShell/7/pwsh.exe -NoProfile -File tests/golden_local_id.ps1`（必须 pwsh 7，且 spawn 时 env 要带 `PATHEXT=.COM;.EXE;.BAT;.CMD`，否则 cl mock 用例假失败）。
- **交付两种包**：①全量包 `SkyrimOutfitSystemNG-1.5.43.zip`（`build/pack_1_5_43.ps1`，31 条目，dist 全量 + `SHA256SUMS.txt`）；②测试包 `SkyrimOutfitSystemNG-1.5.43-test.zip`（`build/pack_1_5_43_test.ps1`，2 条目：`SKSE/Plugins/OutfitSystemNG.dll` + `1.5.43-test-notes.txt`），可直接用 MO2 装。
## 1.5.42 构建与交付复核状态（历史，删除崩溃已由 1.5.43 修；其金件页仍每次点击自动写盘）

**验证状态（1.5.42）**：尚未实机验证。Release 全量重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_42.log`，末尾有 `Deploying OutfitSystemNG.dll to dist/SKSE/Plugins`）。守卫测试 16 项 PASS（新增 `tests/menu_page_slice.ps1`），另 2 项是既有环境失败（`hair_tint_read_only.ps1` 锚点已随代码迁移消失、`skse_listener_replay.ps1` 需要现场 `bug/skse64.log`）。

- **本版改动面（只修删除套装闪退）**：`src/cpp/MenuUI.cpp` 的 `RenderOutfits()` —— 取页切片前先 `const std::size_t total = g_filtered.size();`，`begin = min(total, max(0, (g_page - 1) * pageSize))`、`end = min(total, begin + pageSize)`；`src/cpp/include/version.h` PATCH 42；`CMakeLists.txt` VERSION 1.5.42；新增 `tests/menu_page_slice.ps1`。引擎侧（注入壳 / 贴件 / 槽清理 / `.tri` / 场景套装 / 钩子）与金件存库一行未改。
- **构建后必查**：`git diff v1.5.41 HEAD -- src` 应只有 `MenuUI.cpp` + `version.h`（外加 `CMakeLists.txt` 与 `tests/`），`MorphRefresh.cpp` / `SceneOutfits.cpp` / `Patches.cpp` / `GoldenItems.cpp` 必须是空 diff；DLL 版本资源应为 `1.5.42.0`。
- **守卫测试跑法**：`C:/Program Files/PowerShell/7/pwsh.exe -NoProfile -File tests/menu_page_slice.ps1`（必须 pwsh 7；PS 5.1 的 `String.Split(string)` 会退化成按字符拆分导致假失败）。
- **交付两种包**：①全量包 `SkyrimOutfitSystemNG-1.5.42.zip`（`build/pack_1_5_42.ps1`，31 条目，dist 全量 + `SHA256SUMS.txt`）；②测试包 `SkyrimOutfitSystemNG-1.5.42-test.zip`（`build/pack_1_5_42_test.ps1`，2 条目：`SKSE/Plugins/OutfitSystemNG.dll` + 说明文本），可直接用 MO2 安装。
## 1.5.41 构建与交付复核状态（历史，启动闪退已由 1.5.42 修）

**验证状态（1.5.41）**：尚未实机验证。Release 全量重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_41.log`，末尾 `Deploying OutfitSystemNG.dll to dist/SKSE/Plugins`）。DLL `OutfitSystemNG.dll` 2,037,760 字节 / `39f6b65e…fd967ad0` / `1.5.41.0`。

- **本版改动面（只修启动闪退 + 两道兜底）**：`src/core/GoldenItems.cpp`（`std::mutex`→`std::recursive_mutex`、新增 `TotalCountLocked()`、三条持锁路径改用它）、`src/cpp/main.cpp`（`kDataLoaded` 金件块包 try/catch、`MessageHandler` 拆成 `HandleMessage` + try/catch 壳、加 `#include <exception>`）、`src/cpp/include/version.h`（`_VERSION_PATCH 41`）、`CMakeLists.txt`（`VERSION 1.5.41`）、新增 `tests/golden_lock_guards.ps1`。**引擎侧与菜单功能零改动。**
- **构建后必查**：DLL 里必须能搜到 `the golden list stays empty for this session` / `an exception escaped` / `1.5.41`；`TotalCountLocked` 因 Release 内联通常**搜不到**（属正常，别当成没改）。
- **静态守卫**：`tests/golden_lock_guards.ps1` 必须打印 `GOLDEN_LOCK_GUARDS_OK`（锁是递归、无 `std::to_string(TotalCount())`、启动加载在 `try {` 内、`HandleMessage` / `MessageHandler` 壳与 `an exception escaped` 都在）；当前 16 项 PASS，另 2 项既有环境失败（`hair_tint_read_only` 锚点消失、`skse_listener_replay` 缺 `bug/skse64.log`）。
- **交付两种包**：①全量包 `SkyrimOutfitSystemNG-1.5.41.zip`（`build/pack_1_5_41.ps1`，31 条目，dist 全量 + `SHA256SUMS.txt`）；②测试包 `SkyrimOutfitSystemNG-1.5.41-test.zip`（`build/pack_1_5_41_test.ps1`，2 条目：`SKSE/Plugins/OutfitSystemNG.dll` + `1.5.41-test-notes.txt`），可直接用 MO2 装。
## 1.5.40 构建与交付复核状态（历史，启动闪退已由 1.5.41 修）

**验证状态（1.5.40）**：尚未实机验证。Release 全量重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_40.log`，末尾 `Deploying OutfitSystemNG.dll to dist/SKSE/Plugins`）。DLL `OutfitSystemNG.dll` 2,036,224 字节，SHA256 `95c2b38848bf3d3f2a0577f2b611b8cbc06c7ad76f653674eb89fbcb5a5618bf`，FileVersion/ProductVersion `1.5.40.0`，`build/Release` 与 `dist` 两份逐字节一致。

- **本版改动面（只修 1.5.39 的两处回归）**：`src/cpp/MenuUI.cpp`（两处金件中文串改为与英文占位符同序）、`src/cpp/I18n.cpp`（新增 `PlaceholderSignature()` / `PlaceholdersMatch()`，`Apply()` 里对字典与翻译文件逐条校验，不一致回退英文）、`src/core/PluginScan.cpp`（新增 `enum class OutfitVerdict` + `Verdict()`，模型判据看男女、名字判据放宽、`CollectArmors` 末尾加统计行）、`src/core/EspOutfits.cpp`（列表过滤改 `entry.armors`）、`src/cpp/include/version.h`（PATCH 40）、`CMakeLists.txt`（VERSION 1.5.40），新增守卫测试 `tests/golden_menu_guards.ps1`。**钩子层、注入壳、贴件语义、槽清理、`.tri` 推导、`SceneOutfits`、模块文件名零改动**。
- **构建陷阱（本版踩过）**：`Verdict()` 与其 enum 必须定义在 `CollectArmors()` **之前**，否则 MSVC 报 `error C3861: 'Verdict': 找不到标识符` + 一串 `C2653 / C2051`（22 个 error）。匿名命名空间里的定义顺序就是可见性顺序。
- **构建后必查**：DLL 里必须能搜到 `count as clothing` / `does not use the placeholders` / `were ignored because their placeholders` / `已添加 %zu 件，来自 %s。` / `已导入 %zu 件，来源 %s` / `1.5.40`；旧错串 `已添加 %s 的 %zu 件` / `已从 %s 导入 %zu 件` 必须 **0 命中**（用 utf8 读 DLL 再比对，latin1 读中文会假阴性）。
- **静态守卫**：`tests/golden_menu_guards.ps1` 必须通过（全仓 `I18n::T(en, zh)` 双串占位符同序、`I18n` 护栏在位、`EspOutfits` 用 `entry.armors` 过滤且**不得**出现 `entry.outfitArmors == 0`、`PluginScan` 双性别判据、金件两行中文串同序）；当前 15 项 PASS，另 2 项既有环境失败（`hair_tint_read_only.ps1` 锚点已随代码迁移消失、`skse_listener_replay.ps1` 需要 `bug/skse64.log`）不是回归。
- **交付两种包**：①全量包 `SkyrimOutfitSystemNG-1.5.40.zip`（`build/pack_1_5_40.ps1`，31 条目，dist 全量 + `SHA256SUMS.txt`）；②测试包 `SkyrimOutfitSystemNG-1.5.40-test.zip`（`build/pack_1_5_40_test.ps1`，2 条目：`SKSE/Plugins/OutfitSystemNG.dll` + `1.5.40-test-notes.txt`），可直接用 MO2 装、替换已有 DLL 后重启即可测。
## 1.5.39 构建与交付复核状态（历史，两处回归已由 1.5.40 修）

**验证状态（1.5.39）**：尚未实机验证。Release 全量重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_39.log`，末尾 `Deploying OutfitSystemNG.dll to dist/SKSE/Plugins`）。DLL `OutfitSystemNG.dll` 2,030,592 字节，SHA256 `c72a60a6f4a400bbb4cf06a9b2e1c81a86baca487fbf901119e264421aee83da`，FileVersion/ProductVersion `1.5.39.0`，`build/Release` 与 `dist` 两份逐字节一致。

- **本版改动面**：新增 `src/core/PluginScan.h/.cpp`（加载顺序视图 + `IsOutfitArmor()`）、`src/core/EspOutfits.h/.cpp`（按 ESP 生成套装）、`src/core/GoldenItems.h/.cpp`（金件，独立 JSON）；改 `src/cpp/MenuUI.cpp`（幻化页「从 ESP 生成套装」块、金件页 `RenderGolden()`、设置页「套装生成」段、全部文案走 `I18n::T()`）、`src/cpp/MenuOutfitEditor.cpp`（逐槽策略展开状态改 `g_slotPolicyOpen`）、`src/cpp/I18n.cpp`（新增约 26 条中英对照）、`src/core/Settings.h/.cpp`（`espOutfitAutoFavorite` / `goldenIniMigrated`）、`src/cpp/main.cpp`（旧 ini 金件一次性迁移）、`src/cpp/include/version.h`（PATCH 39）、`CMakeLists.txt`（VERSION 1.5.39），新增守卫测试 `tests/esp_outfits_filter.ps1`。**钩子层、注入壳、贴件语义、槽清理、`.tri` 推导、`SceneOutfits`、模块文件名零改动**。
- **构建后必查**：DLL 里 `OutfitSystemGoldenItems.json` / `ESP outfits: ` / `wearable clothing` / `FX effects and other non clothing records were left out` / `skipped as not clothing` / `Golden items: ` / `PluginScan: ` / `1.5.39` 必须能搜到；`1.5.34` 的 `handed to the game thread` / `scene started;` 与 1.5.30/31 已删机件仍保持原状态。`src/core/*.cpp` 是新文件，注意 `CMakeLists` 的 `file(GLOB_RECURSE … CONFIGURE_DEPENDS)` 会自动收进编译 —— 若编译日志里看不到 `PluginScan.cpp` / `EspOutfits.cpp` / `GoldenItems.cpp` 的 `CC` 行，说明 glob 没刷新，删掉 `build/` 里的缓存重 configure。
- **静态守卫**：`tests/esp_outfits_filter.ps1` 必须通过（套装生成走 `PluginScan::IsOutfitArmor()`、分桶剔除 `kFxBit`、`GoldenItems.cpp` **不得**出现 `IsOutfitArmor`）；其余 6 项既有守卫（`scene_outfit_switch` / `skinning_stale_slots` / `morph_paths` / `morph_read_only` / `morph_slider_queue` / `morph_thread_gate`）应全部 PASS。跑法：`C:/Program Files/PowerShell/7/pwsh.exe`（**不要**用 PowerShell 5.1），env 带 `PATHEXT=.COM;.EXE;.BAT;.CMD`。`morph_cache_replay.ps1` 属 1.5.38 候选，不在本分支。
- **打包**：`build/pack_1_5_39.ps1`（由 `pack_1_5_34.ps1` 改版本号）—— `[IO.Compression.ZipFile]` Open Create + 正斜杠 `CreateEntry` + 逐条回读校验，**禁用 `Compress-Archive`**；包名 `SkyrimOutfitSystemNG-1.5.39.zip`，31 条目、0 个反斜杠条目名。

## 1.5.34 构建与交付复核状态（历史）

**验证状态（1.5.34）**：尚未实机验证。Release 全量重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_34.log`，末尾 `Deploying OutfitSystemNG.dll to dist/SKSE/Plugins`）。DLL `OutfitSystemNG.dll` 1,937,408 字节，SHA256 `e1cdb9518149d0e1f7a23d1835317620b8258c1e030d6d987b658e4718283782`，FileVersion/ProductVersion `1.5.34.0`，`build/Release` 与 `dist` 两份逐字节一致。

- **本版改动面**：`src/cpp/SceneOutfits.cpp`（`ModEventSink` 离线程事件改走 `SKSE::GetTaskInterface()->AddTask([eventName]{ Dispatch(eventName); })`；新增 `void Dispatch(const std::string& a_event)` 单一入口；无任务接口时只 warn 降级）、`src/cpp/include/version.h`（PATCH 34）、`CMakeLists.txt`（VERSION 1.5.34）、`tests/scene_outfit_switch.ps1`（守卫改为「恰好一次 `AddTask` 交接」）。**钩子层、注入壳、贴件语义、槽清理、`.tri` 推导、模块文件名、换装调用序列零改动**（`AutoSwitch.*` 仍为空 diff）。
- **构建后必查**：DLL 里 `handed to the game thread` / `came from another thread` / `there is no task interface` / `scene started;` / `setOutfitByName` / `1.5.34` 必须能搜到；`arrived off the game thread`（1.5.33 的丢事件行）以及 1.5.30/1.5.31 的 `repeating the equip pass` / `follow-up` / `Model Rebuild` / `rebuild the model of` / `RequestOutfitPasses` 必须**一条都搜不到**。
- **静态守卫**：`tests/scene_outfit_switch.ps1`、`skinning_stale_slots.ps1`、`morph_paths.ps1`、`morph_read_only.ps1`、`morph_slider_queue.ps1` 全部 PASS（pwsh 7，env 需带 `PATHEXT`）。
## 1.5.33 构建与交付复核状态（历史，已被 1.5.34 取代）

**验证状态（1.5.33）**：尚未实机验证。Release **全量重编** `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log` 149 行、36 个 `.cpp` 全部 CC，留档 `build/build_1_5_33.log`）。DLL `OutfitSystemNG.dll` 1,934,848 字节，SHA256 `8c23e5eec3ae0f4561125ab2cabb71e63ef7c21d01e6a8b85755687454716aaf`，FileVersion/ProductVersion `1.5.33.0`，`build/Release` 与 `dist` 两份逐字节一致。

- **本版改动面（源码基线 = `git checkout v1.5.29 -- src`）**：`SceneOutfits.cpp`（整文件重写：只剩名字解析 + `SwitchTo()`，删掉全部队列 / `Tick()` 机件）、`include/SceneOutfits.h`（删 `Tick()` 声明）、`include/OutfitSystem.h` + `OutfitSystem.cpp`（新增 `setOutfitByName()`）、`main.cpp:279` 与 `NGHotkeys.cpp:255`（删 `SceneOutfits::Tick()` 调用）、`version.h`（PATCH 33）、`CMakeLists.txt`（VERSION 1.5.33）。**钩子层、注入壳、贴件语义、槽清理、`.tri` 推导、模块文件名零改动。**
- **构建后必查**：DLL 里 `setOutfitByName` / `SceneOutfits: ` / `create an outfit with exactly that name` / `the scene switch is skipped` / `1.5.33` 必须能搜到；`repeating the equip pass` / `follow-up` / `Model Rebuild` / `rebuild the model of` / `RequestOutfitPasses` / `scene event dropped` 必须**一条都搜不到**（1.5.30/1.5.31 与 1.5.32 的队列机件都已删除）。
- **对比 1.5.32 的差异面**：`git diff v1.5.32 HEAD -- src` 应只含 `SceneOutfits.cpp/.h`、`OutfitSystem.cpp/.h`、`main.cpp`、`NGHotkeys.cpp`、`version.h`，并且 `AutoSwitch.cpp/.h` **必须为空 diff**（用户点名的「按地点/状态自动切换」本版没动）。
- **模块文件名仍为 `OutfitSystemNG.dll`**（1.5.30 起），安装包里不得出现旧名 `SkyrimOutfitSystemNG.dll`。
- **配置注意**：`buildtask_msbuild.bat` 内部的 `ZERO_CHECK` 会重跑 cmake configure（自带完整环境，vcpkg 走缓存），因此 `build/SkyrimOutfitSystemNG.vcxproj` 由 `CMakeLists.txt` 生成 —— 不要手改，也不要裸跑 `cmake -S . -B build`（本机 vswhere 返回空，会失败并改写 `CMakeCache.txt`）。
- **守卫测试**：`tests/*.ps1` 必须用 PowerShell 7（`C:/Program Files/PowerShell/7/pwsh.exe`）；用 Node `spawnSync` 跑时 env 里必须带 `PATHEXT`（如 `.COM;.EXE;.BAT;.CMD`），否则依赖 `cmd` 的 mock 用例会假失败。本版新增 `tests/scene_outfit_switch.ps1`，删除 `tests/scene_outfit_queue.ps1`（后者断言的机件已不存在）。
- 安装包 `SkyrimOutfitSystemNG-1.5.33.zip`：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小与哈希只写包外 `VERSIONS.md` 与同名 `.zip.sha256`。

## 1.5.32 构建与交付复核状态（历史，其场景入口写法已被 1.5.33 取代）

**验证状态（1.5.32）**：尚未实机验证。Release 重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log` 149 行，留档 `build/build_1_5_32.log`）。DLL `OutfitSystemNG.dll` 1,937,920 字节，SHA256 `da9ec8995c599f00cb249b611d66a13c2f3f030f845951b3d6b5eacdca05f875`，FileVersion/ProductVersion `1.5.32.0`，`build/Release` 与 `dist` 两份逐字节一致。

- **本版改动面**：`MorphRefresh.cpp` / `include/MorphRefresh.h`（删除补跑随访机件）、`OutfitSystem.cpp` / `include/OutfitSystem.h`（删除 `rebuildActorModel`）、`NGFeatures.cpp`（`RefreshActor` 回到只调 `refreshArmorFor`）、`SceneOutfits.cpp`（场景开始/结束只调 `Features::AssignOutfit`，事件改游戏线程同步执行，名字查找放宽）、`version.h`（PATCH 32）、`CMakeLists.txt`（VERSION 1.5.32）。**钩子层、注入壳、贴件语义、槽清理、`.tri` 推导、模块文件名零改动。**
- **构建后必查**：DLL 里 `repeating the equip pass` / `follow-up` / `Model Rebuild` / `rebuild the model of` / `RequestOutfitPasses` 必须**一条都搜不到**（1.5.30/1.5.31 的机制已整体删除）；`SceneOutfits: ` 与 `1.5.32` 必须能搜到。
- **模块文件名仍为 `OutfitSystemNG.dll`**（1.5.30 起），安装包里不得出现旧名 `SkyrimOutfitSystemNG.dll`。
- **配置注意**：`buildtask_msbuild.bat` 内部的 `ZERO_CHECK` 会重跑 cmake configure（自带完整环境，vcpkg 走缓存），因此 `build/SkyrimOutfitSystemNG.vcxproj` 由 `CMakeLists.txt` 生成 —— 不要手改，也不要裸跑 `cmake -S . -B build`（本机 vswhere 返回空，会失败并改写 `CMakeCache.txt`）。
- 安装包 `SkyrimOutfitSystemNG-1.5.32.zip`：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小与哈希只写包外 `VERSIONS.md` 与同名 `.zip.sha256`。

## 1.5.31 构建与交付复核状态（历史，其修法已被用户否决）

**验证状态（1.5.31）**：尚未实机验证。Release 重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_31.log`）。DLL `OutfitSystemNG.dll` 1,941,504 字节，SHA256 `c8c8ae5db004c0017b029110b23dd707e1e519789c2acf81ba637a5ed63ad6b2`，FileVersion/ProductVersion `1.5.31.0`，`build/Release` 与 `dist` 两份逐字节一致。

- **本版改动面**：`OutfitSystem.h/.cpp`（新增 `rebuildActorModel`）、`MorphRefresh.cpp`（随访链第 2 步前重建模型）、`SceneOutfits.cpp`（场景开始/结束改调 `Features::AssignOutfit`）、`version.h`（PATCH 31）、`CMakeLists.txt`（VERSION 1.5.31）。**钩子层、注入壳、贴件语义、槽清理、`.tri` 推导、模块文件名零改动。**
- **模块文件名仍为 `OutfitSystemNG.dll`**（1.5.30 起），安装包里不得出现旧名 `SkyrimOutfitSystemNG.dll`。
- **配置注意**：`buildtask_msbuild.bat` 内部的 `ZERO_CHECK` 会重跑 cmake configure（自带完整环境，vcpkg 走缓存），因此 `build/SkyrimOutfitSystemNG.vcxproj` 由 `CMakeLists.txt` 生成 —— 不要手改，也不要裸跑 `cmake -S . -B build`（本机 vswhere 返回空，会失败并改写 `CMakeCache.txt`）。
- 安装包 `SkyrimOutfitSystemNG-1.5.31.zip`：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小与哈希只写包外 `VERSIONS.md` 与同名 `.zip.sha256`。

## 1.5.30 构建与交付复核状态（历史）

**验证状态（1.5.30）**：尚未实机验证。Release 重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`）。最终 DLL `dist/SKSE/Plugins/OutfitSystemNG.dll` = `build/Release/OutfitSystemNG.dll` = `1,939,456` 字节，FileVersion/ProductVersion `1.5.30.0`，SHA256 `2d0d968953abe0c3715cf1e2e8b882b2d410d937fc30e0a8659dda451d79cfda`。

- **模块改名（1.5.30 起）**：`CMakeLists.txt` 用 `set_target_properties(... OUTPUT_NAME "OutfitSystemNG")`，POST_BUILD 部署到 `dist/SKSE/Plugins/OutfitSystemNG.dll`；工程名与 `buildtask_msbuild.bat` 编的 `build\SkyrimOutfitSystemNG.vcxproj` **不变**。`dist` 与 `build/Release` 里旧名 `SkyrimOutfitSystemNG.dll` 已删除，避免双加载。插件内部身份不变：日志仍写 `SkyrimOutfitSystemNG.log`，翻译文件前缀、Papyrus 名、存档标识都不动。
- **配置注意**：`buildtask_msbuild.bat` 内部的 `ZERO_CHECK` 会重跑 cmake configure（自带完整环境，vcpkg 走缓存），因此 `build/SkyrimOutfitSystemNG.vcxproj` 由 `CMakeLists.txt` 重新生成；**不要**在裸 `run_code` 里单独跑 `cmake -S . -B build`（本机 `vswhere` 返回空 → `Unable to find a valid Visual Studio instance` / `No CMAKE_CXX_COMPILER could be found`，且会改写 `CMakeCache.txt`）。
- 安装包 `SkyrimOutfitSystemNG-1.5.30.zip`（包名沿用，模块文件名已改）：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小与哈希只写包外 `VERSIONS.md` 与同名 `.zip.sha256`。

## 1.5.29 构建与交付复核状态（历史）

**验证状态（1.5.29）**：尚未实机验证。Release 重编 `BUILD_EXIT=0`，0 error / 0 warning（`cmd /c buildtask_msbuild.bat`，日志 `build/build_task.log`，留档 `build/build_1_5_29.log`）。最终 DLL `1,936,384` 字节，FileVersion/ProductVersion `1.5.29.0`，SHA256 `4f0bcae1e5926763c3d9592ab01812c8f7b968d6e8c7ef37d0ac002c3cb84c92`，`build/Release` 与 `dist` 副本一致。源码守卫测试：`pwsh` 7 下 `tests/*.ps1` 12/14 通过（2 条失效断言是既有问题）。安装包 `SkyrimOutfitSystemNG-1.5.29.zip`：31 条目、0 个反斜杠条目名、解包逐文件哈希与 dist 全等；最终 zip 大小与哈希只写包外 `VERSIONS.md` 与同名 `.zip.sha256`。

> 注意：`tests/*.ps1` 必须用 **PowerShell 7**（`pwsh`）运行。Windows PowerShell 5.1 里 `String.Split(string)` 会退化成"按字符拆分"，让 `hair_recovery_guards.ps1` 之类只做字符串计数的用例假失败。

## 1.5.28 构建与交付复核状态（历史）

**验证状态（1.5.28）**：尚未实机验证，不能声称已实机修好。生产函数路径回归 6/6 通过；最终 Release BUILD_EXIT=0，DLL 版本/哈希与交付副本一致。包内逐文件清单见 SHA256SUMS.txt；最终 zip 大小、哈希及解包复核记录见包外 VERSIONS.md 与同名 .zip.sha256，避免自引用。

- 构建日志 build/build_1_5_28.log；路径回归脚本 tests/morph_paths.ps1（从生产源码提取函数）。
- 最终 DLL 已由主 agent 核对：1,878,016 字节，FileVersion `1.5.28.0`，SHA256 `fe5c79b5b1f1e1fc3153870dd6b61cf293081805cab407ad3ea41ed1621bb553`，build/Release 与 dist 一致，`BUILD_EXIT=0`。ProductVersion=1.5.28.0，dumpbin 确认三个 SKSE 导出与五个系统依赖；未实机加载，最终 zip 数字及逐文件复核见包外 VERSIONS.md；zip SHA256 仅写包外文件，不写包内以免自引用。以下 1.5.27 打包命令与数字保留作历史示例，不是 1.5.28 已交付结果。
- 路径回归已通过 6 例；非法/空 BODYTRI、1P/3P、多 ARMA、回调重入与 provider 前签名仍需按 TESTING 最新章节审查/实测，不将 Release 编译成功当成游戏验收。
- 发布包不得加入/覆盖用户的 skee64.ini；公开配置排障见 README 的 1.5.28 说明。

## 1. 环境

| 组件 | 版本/路径（本项目实测） |
| --- | --- |
| Visual Studio 2022 BuildTools | MSVC 14.44.35207，Windows 10/11 SDK |
| CMake | 3.21+（实测 4.2.0） |
| vcpkg | 任意近期版本，`builtin-baseline` 固定为 `ee12231b20c95013c6638d845d04c91559a1d1ff` |
| CommonLibSSE-NG | `extern/CommonLibSSE-NG`（随仓库，`alandtse` fork 的 `ng` 分支 16.4.0，commit 9955ae3） |
| Papyrus 编译器 | 仅打包脚本时需要（见 §6） |

依赖库（vcpkg.json）：directxmath、directxtk、fmt、nlohmann-json、rapidcsv、simpleini、spdlog、toml11、xbyak。

## 2. 配置

必须使用**静态 triplet**（`x64-windows-static`）：DLL 用静态 CRT 与静态依赖，运行期只导入系统 DLL，这样在缺少 VC 运行库、缺少菜单框架的机器上也能被 SKSE 正常加载。

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DVCPKG_HOST_TRIPLET=x64-windows ^
  -DCMAKE_BUILD_TYPE=Release
```

首次配置约 20 秒（vcpkg 有二进制缓存时），随后编译 CommonLibSSE-NG 的 535 个 .cpp 需要 15–30 分钟。之后增量编译只编本项目的十几个文件，约 1–3 分钟。

## 3. 编译

```bat
cmake --build build --config Release --parallel
```

产物：`build/Release/OutfitSystemNG.dll`（1.5.30 起模块名改为 `OutfitSystemNG`，工程名仍是 `SkyrimOutfitSystemNG`），POST_BUILD 会自动复制到 `dist/SKSE/Plugins/OutfitSystemNG.dll`。

若出现编译错误，注意 MSBuild 输出是本地代码页（中文系统会乱码），用 `findstr /C:"error "` 过滤即可。

## 4. 与菜单框架的关系（不要加回来）

本插件**不链接** `SKSEMenuFramework.lib`、**不导入** `imgui.dll`：

- `MenuBridge` 运行期用 `GetModuleHandleW(L"SKSEMenuFramework.dll")` + `GetProcAddress("AddSectionItem")` 绑定；
- `ImGuiApi` 运行期绑定 32 个 cimgui `ig*` 函数指针；
- 因此 DLL 的导入表只有 KERNEL32/USER32/ole32/VERSION/SHELL32，没有延迟导入，任何机器上都能加载。

如果以后有人重新加 `target_link_libraries(... SKSEMenuFramework)` 或 `/DELAYLOAD`，`LoadLibrary` 失败的 126 错误会再次出现。

## 5. 常见错误

| 错误 | 原因 | 修法 |
| --- | --- | --- |
| `C1083: 无法打开包含文件 "imgui.h"` | 头文件路径未加入 | 确认 `target_include_directories` 有 `src/cpp/include/imgui` |
| `C2027: 使用了未定义类型 RE::NativeFunction` | Papyrus 回调首参不是合法 base | 骨架里已用 `RE::BSScript::IVirtualMachine*` + stackId 形式，新增函数照抄 |
| `C2662: 不能将 this 从 const 转换为非 const` | 用 `const auto& service->inner()` 后调了写接口 | 改 `auto&` |
| `C2039 "X" 不是 "RE::Y" 的成员` | CommonLibSSE-NG 与旧版 API 不同 | 查 `extern/CommonLibSSE-NG/include/RE/**` 真实签名 |
| `LNK2001 __pfnDliNotifyHook2` | 不要写延迟加载钩子 | 本项目不需要延迟加载 |
| 链接失败但只差 CommonLibSSE.lib | `add_subdirectory(extern/CommonLibSSE-NG)` 没执行 | 检查 `EXCLUDE_FROM_ALL` 与 `ENABLE_SKYRIM_*` 缓存变量 |

## 6. Papyrus 脚本编译

`papyrus.exe` 的 `-h` 只接受**单个**目录（分号分隔会报 `Invalid path specified`）。做法是先把工具自带头文件与项目脚本合并到一个目录：

```bat
mkdir build\papyrus_include
copy <papyrus-tools>\h\*.psc build\papyrus_include\
copy src\papyrus\*.psc build\papyrus_include\
cd src\papyrus
<papyrus-tools>\papyrus.exe compile -h ..\..\build\papyrus_include -i skyoutsysquicksloteffect.psc -o ..\..\build\papyrus_out
```

需要编译的脚本（6 个，缺一不可 —— 只写 `Global Native` 声明的 `skyrimoutfitsystemnativefuncs` 也必须编译，否则整个类的原生函数都不绑定）：`skyoutsysautoswitchtrigger`、`skyoutsysmcm`、`skyoutsysplayercombatdetection`、`skyoutsysquicksloteffect`、`skyoutsysquickslotmanager`、`skyrimoutfitsystemnativefuncs`。

## 7. 无人值守构建

`buildtask.bat`（全量：删 build → 配置 → 编译，日志 `build_configure.log` 与 `build/build_task.log`，末尾写 `CONFIGURE_EXIT=` / `BUILD_EXIT=`）与 `buildtask_inc.bat`（增量，不删 build）。脚本内已固定 PATH 与 `USERPROFILE/LOCALAPPDATA/APPDATA/TEMP/VCPKG_ROOT`。

另有 **`buildtask_msbuild.bat`（1.5.20 起首选）**：不重新 configure、不删 build，直接用 MSBuild 编 `build\SkyrimOutfitSystemNG.vcxproj`（**必须加 `/p:BuildProjectReferences=false`，否则会带上 `ZERO_CHECK.vcxproj` 并在 FileTracker 初始化处报 `MSB4018`**）。本机实测 `buildtask.bat` / `buildtask_inc.bat` 已失效：前者 configure 阶段报「Generator Visual Studio 17 2022 could not find specified instance of Visual Studio: C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools」（该实例不在 VS Installer 注册表里），后者同样走 configure；直接编 `.sln` 也会因 `ZERO_CHECK` 崩。`buildtask_msbuild.bat` 里显式设齐了 `SystemRoot` / `ProgramData` / `ALLUSERSPROFILE` / `USERPROFILE` / `TEMP` 等变量（自动化会话里 `process.env` 可能为空，缺一个 FileTracker 就抛 `System.TypeInitializationException`），日志同样写 `build/build_task.log` 并追加 `BUILD_EXIT=`。

在无交互环境里用计划任务启动（直接 spawn 的进程会被宿主杀掉）：

```bat
schtasks /create /tn SOSNG /tr "cmd /c D:\Skyrim-dev\src\SkyrimOutfitSystemSE-NG\buildtask.bat" /sc once /st 23:59 /f
schtasks /run /tn SOSNG
```

## 8. 打包

> **当前版本（1.5.43）用封装好的脚本**：`build/pack_1_5_43.ps1`（由上一版脚本改版本号；测试包用 `build/pack_1_5_43_test.ps1`），直接 `C:/Program Files/PowerShell/7/pwsh.exe -NoProfile -File build/pack_1_5_43.ps1` 即可 —— 它会重算 `dist/SHA256SUMS.txt`、用 `[IO.Compression.ZipFile]` 逐条正斜杠 `CreateEntry` 压包、回读校验条目数与逐文件哈希、最后打印 `ZipBytes` / `ZipSHA256` / `EntriesVerified` / `ManifestVerified` / `DistBytes`。下面这段是 1.5.27 的原始记录，步骤等价，仅作参考（包名按当前版本替换）。

```powershell
Remove-Item SkyrimOutfitSystemNG-1.5.27.zip -ErrorAction SilentlyContinue
# 6 个 Papyrus 脚本必须全部编译并拷进 dist\Scripts（漏一个 pex 会让整类原生函数失效）
Copy-Item build\papyrus_out\*.pex dist\Scripts -Force
Compress-Archive -Path dist\* -DestinationPath SkyrimOutfitSystemNG-1.5.27.zip -Force
```

打包前先按 dist 全量重算 `dist/SHA256SUMS.txt`（每行 `<sha256>两个空格<反斜杠相对路径>`，按 hash 行字典序排列、**不含自身**），再压包；包内**不包含**菜单框架（用户自备）。压包**不要用 `Compress-Archive`**（旧版 PowerShell 会写反斜杠条目名，且可能夹带外层目录），用 `[IO.Compression.ZipFile]` 逐条 `CreateEntry` 并显式给正斜杠条目名，条目数应为 `dist` 文件数（1.5.27 为 31 条 = 30 个文件 + `SHA256SUMS.txt`）；压完必须回读校验：条目数与 dist 一致、无 `\` 条目名、逐个文件 sha256 与 dist 相同。

参考实现（1.5.27 实际使用）：

```powershell
Add-Type -AssemblyName System.IO.Compression.FileSystem
$src = "D:\Skyrim-dev\src\SkyrimOutfitSystemSE-NG\dist"
$dst = "SkyrimOutfitSystemNG-1.5.27.zip"
Remove-Item $dst -ErrorAction SilentlyContinue
$zip = [IO.Compression.ZipFile]::Open((Resolve-Path .).Path + "\" + $dst, "Create")
Get-ChildItem $src -Recurse -File | ForEach-Object {
  $rel = $_.FullName.Substring($src.Length + 1).Replace("\", "/")
  $e = $zip.CreateEntry($rel, [IO.Compression.CompressionLevel]::Optimal)
  $s = $e.Open(); $f = [IO.File]::OpenRead($_.FullName); $f.CopyTo($s); $f.Dispose(); $s.Dispose()
}
$zip.Dispose()
```

### 8.1 语言文件

`dist/interface/translations/SkyrimOutfitSystemNG_english.txt` 与 `_chinese.txt` 不是手写的：它们由 `src/cpp/I18n.cpp` 的 `kDictionary` 生成（Node 脚本解析 C++ 字面量并反转义），格式为 UTF-16LE + BOM、每行 `$<英文原文><TAB><译文>`，两份都必须 237 条（不含注释头）。

改了 `kDictionary` 之后必须重跑生成脚本，否则外置文件里缺新键（界面能显示，但用户改不了那一行）。**生成时必须写 TAB 而不是 `=`**：插件的解析是 TAB 优先、`=` 兜底，这样键里带 `=` 的句子（例如 `Chinese needs [Fonts] EnableChinese = true ...`）也不会被截断。


## 9. 静态验证（无游戏环境时）

1. **导出表**：字符串里必须有 `SKSEPlugin_Load` / `SKSEPlugin_Query` / `SKSEPlugin_Version`。
2. **导入表**：只允许 KERNEL32/USER32/ole32/VERSION/SHELL32，且 delay-import 目录 RVA = 0。
3. **独立加载**：把 DLL 复制到空目录，`LoadLibraryExW(path, 0)` 返回非零句柄且 `GetLastError() == 0`。
4. **版本号**：`src/cpp/include/version.h` 的 MAJOR/MINOR/PATCH 与 `CMakeLists.txt` 的 `project(... VERSION ...)`、`vcpkg.json` 的 `version-string` 三处一致；插件启动时日志会打印 `v1.1.0.0`（DLL 文件属性里没有版本资源，属正常）。
