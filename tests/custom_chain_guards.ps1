$ErrorActionPreference='Stop'
$src=Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/hooking/Hooks.cpp')
$start=$src.IndexOf('CallSiteSnapshot::Bytes originalBytes')
$end=$src.IndexOf('SetSiteHookInstalled(g_customSkinPlayerInstalled)', $start)
if($start -lt 0 -or $end -le $start){throw 'Custom chain block missing'}
$body=$src.Substring($start,$end-$start)
foreach($token in @('original.DirectTarget()','original.MatchesOriginal(currentBytes)','CustomFromInventoryModel','sub(rsp, 0x60)','ptr[rsp + 0x40], rax','ptr[rsp + 0x50], xmm0','mov(rcx, ptr[rsp + 0x20])','r15','rdi')){if(-not $body.Contains($token)){throw ('Missing chain guard: '+$token)}}
foreach($token in @('InventoryChanges_ExecuteVisitorOnWorn.address()','mov(rcx, rbx)','CustomFromInventory)')){if($body.Contains($token)){throw ('Forbidden old route: '+$token)}}
Write-Output 'CUSTOM_CHAIN_STATIC_OK: direct target retained, owner/model route, byte recheck and return slots; not ABI/runtime proof'
