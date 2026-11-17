@echo off
cd /d D:\Skyrim-dev\src\SkyrimOutfitSystemSE-NG
set "PATH=D:\Skyrim-dev\tools\PortableGit\cmd;D:\Skyrim-dev\tools\PortableGit\bin;D:\Skyrim-dev\tools\cmake-4.2.0-windows-x86_64\bin;D:\Skyrim-dev\tools\node-v22.20.0-win-x64;D:\Skyrim-dev\tools\python;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Program Files\PowerShell\7;C:\Windows\System32\WindowsPowerShell\v1.0\;C:\Windows\System32\OpenSSH\;C:\Users\Administrator\AppData\Local\Microsoft\WindowsApps;C:\Program Files\dotnet\;C:\Users\Administrator\.dotnet\tools"
set "USERPROFILE=C:\Users\Administrator"
set "LOCALAPPDATA=C:\Users\Administrator\AppData\Local"
set "APPDATA=C:\Users\Administrator\AppData\Roaming"
set "TEMP=C:\Users\Administrator\AppData\Local\Temp"
set "TMP=C:\Users\Administrator\AppData\Local\Temp"
set "VCPKG_ROOT=D:\Skyrim-dev\tools\vcpkg"
"D:\Skyrim-dev\tools\cmake-4.2.0-windows-x86_64\bin\cmake.exe" -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=D:/Skyrim-dev/tools/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DVCPKG_HOST_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release > build_configure.log 2>&1
echo CONFIGURE_EXIT=%ERRORLEVEL% >> build_configure.log
if not exist build\CMakeCache.txt exit /b 1
"D:\Skyrim-dev\tools\cmake-4.2.0-windows-x86_64\bin\cmake.exe" --build build --config Release --parallel 1 > build\build_task.log 2>&1
echo BUILD_EXIT=%ERRORLEVEL% >> build\build_task.log
