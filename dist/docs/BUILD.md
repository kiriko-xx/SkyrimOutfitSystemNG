# 构建说明（BUILD）

## 1.5.28 构建与交付复核状态

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

产物：`build/Release/SkyrimOutfitSystemNG.dll`，POST_BUILD 会自动复制到 `dist/SKSE/Plugins/SkyrimOutfitSystemNG.dll`。

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
