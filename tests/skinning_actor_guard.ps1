$ErrorActionPreference = 'Stop'
$src = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/hooking/Patches.cpp')
$start = $src.IndexOf('bool ShouldOverrideSkinning(')
$end = $src.IndexOf('class EquippedArmorVisitor', $start)
$gate = $src.Substring($start, $end - $start)
foreach ($required in @('IsReadableObject(target)', 'GetFormType() != RE::FormType::ActorCharacter', 'RE::TESForm::LookupByID(target->GetFormID()) != target', 'target->As<RE::Actor>()', 'reported.exchange(true')) { if (-not $gate.Contains($required)) { throw "Missing identity guard: $required" } }
if ($gate.Contains('skyrim_cast')) { throw 'RTTI conversion remains in entry gate' }
if ($gate.IndexOf('LookupByID') -gt $gate.IndexOf('target->As<RE::Actor>()')) { throw 'Conversion precedes registry identity check' }
$start = $src.IndexOf('namespace DontVanillaSkinPlayer')
$end = $src.IndexOf('namespace ShimWornFlags', $start)
$hide = $src.Substring($start, $end - $start)
if ($hide.Contains('skyrim_cast')) { throw 'Hiding callback still converts with RTTI' }
if (-not $hide.Contains('keeping vanilla skinning.')) { throw 'Missing fail-open policy' }
foreach ($required in @('PAGE_READONLY', 'PAGE_READWRITE', 'PAGE_WRITECOPY', 'PAGE_EXECUTE_READ', 'PAGE_EXECUTE_READWRITE', 'PAGE_EXECUTE_WRITECOPY')) { if (-not $src.Contains($required)) { throw "Missing readable access type: $required" } }
Write-Output 'SKINNING_ACTOR_GUARDS_OK: static ordering only, not an engine lifetime or register proof'
