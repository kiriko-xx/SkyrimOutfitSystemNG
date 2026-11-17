$ErrorActionPreference='Stop'
$src=Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/hooking/HookSupport.cpp')
$begin=$src.IndexOf('void LogExternalCallTarget(')
$end=$src.IndexOf('void LogModuleIdentity(', $begin)
$body=$src.Substring($begin,$end-$begin)
foreach($token in @('VirtualQuery','GetModuleHandleExA','GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT','MEM_COMMIT','PAGE_GUARD','PAGE_NOACCESS','std::min<std::size_t>(32','ReadProcessMemory','read > sizeof(bytes)','DIAGNOSTIC ONLY')) { if(-not $body.Contains($token)) { throw ('Missing bound: '+$token) } }
foreach($token in @('WriteProcessMemory','VirtualProtect','write_branch','memcpy','FlushInstructionCache')) { if($body.Contains($token)) { throw ('Unexpected mutation: '+$token) } }
if($body.Contains('reinterpret_cast<const std::uint8_t*>')) { throw 'Unsafe direct target byte dereference' }
Write-Output 'CALL_TARGET_DIAGNOSTICS_GUARDS_OK: bounded read-only API use; static checks, not runtime memory validation'
