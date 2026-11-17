@echo off
cd /d D:\Skyrim-dev\src\SkyrimOutfitSystemSE-NG
set "PATH=D:\Skyrim-dev\tools\PortableGit\cmd;D:\Skyrim-dev\tools\PortableGit\bin;D:\Skyrim-dev\tools\cmake-4.2.0-windows-x86_64\bin;D:\Skyrim-dev\tools\node-v22.20.0-win-x64;D:\Skyrim-dev\tools\python;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Program Files\PowerShell\7;C:\Windows\System32\WindowsPowerShell\v1.0\;C:\Windows\System32\OpenSSH\"
set "SystemRoot=C:\Windows"
set "windir=C:\Windows"
set "ComSpec=C:\Windows\system32\cmd.exe"
set "PATHEXT=.COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC"
set "USERPROFILE=C:\Users\Administrator"
set "HOMEDRIVE=C:"
set "HOMEPATH=\Users\Administrator"
set "LOCALAPPDATA=C:\Users\Administrator\AppData\Local"
set "APPDATA=C:\Users\Administrator\AppData\Roaming"
set "ProgramData=C:\ProgramData"
set "ALLUSERSPROFILE=C:\ProgramData"
set "PUBLIC=C:\Users\Public"
set "SystemDrive=C:"
set "ProgramFiles=C:\Program Files"
set "ProgramFiles(x86)=C:\Program Files (x86)"
set "CommonProgramFiles=C:\Program Files\Common Files"
set "TEMP=C:\Users\Administrator\AppData\Local\Temp"
set "TMP=C:\Users\Administrator\AppData\Local\Temp"
set "NUMBER_OF_PROCESSORS=8"
set "PROCESSOR_ARCHITECTURE=AMD64"
set "OS=Windows_NT"
set "VCPKG_ROOT=D:\Skyrim-dev\tools\vcpkg"
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" build\SkyrimOutfitSystemNG.vcxproj /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /nologo /v:m /m > build\build_task.log 2>&1
echo BUILD_EXIT=%ERRORLEVEL% >> build\build_task.log
type build\build_task.log
