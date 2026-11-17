$ErrorActionPreference = 'Stop'
$src = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/HairTintRecovery.cpp')
$header = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/include/HairTintRecovery.h')
foreach ($required in @('kMaxRoots = 8', 'kMaxObjects = 512', 'kMaxGeometries = 64', 'kMaxDepth = 64', 'std::atomic<std::uintptr_t> g_pending', 'GetNiNodeUpdateEventSource()', 'static NodeSink sink', 'g_pending.exchange(0', 'if (!identity) { return; }', 'RE::PlayerCharacter::GetSingleton()', 'IsMenuOpen("RaceSex Menu")', 'GetHeadPartObject(RE::BGSHeadPart::HeadPartType::kHair)', 'GetBiped(firstPerson)', 'a_mask & (1u << slot)', 'partClone.get()', 'if (root == source)', 'BSShaderMaterial::Type::kLighting', 'BSShaderMaterial::Feature::kHairTint', 'netimmerse_cast<RE::BSLightingShaderProperty*>', 'std::isfinite', 'haveColor && color != sample', 'targets.forbiddenRoot = roots.values[0]', 'if (targets.overlap)', 'Seen(source.geometry, source.geometries, targets.geometry[i])', 'Seen(source.material, source.geometries, material)', 'Seen(written, count, material)', 'current.Collect(a_player, a_coveredSlotMask)', 'current.values != roots.values', 'hair->tintColor = color;', 'eventConsumed=1 sourceSampled={}', 'g_pending.store(0')) {
 if (-not $src.Contains($required)) { throw "Missing recovery guard: $required" }
}
$begin = $src.IndexOf('class NodeSink')
$end = $src.IndexOf('enum class Skip', $begin)
$sink = $src.Substring($begin, $end - $begin)
foreach ($forbidden in @('GetSingleton', 'AddTask', 'SKSE::log', 'tintColor', 'a_event->reference->', 'GetFormID', 'GetActorBase')) {
 if ($sink.Contains($forbidden)) { throw "Callback must publish identity only: $forbidden" }
}
foreach ($forbidden in @('GetActorBase', 'SetHairColor', 'UpdateHairColor', 'ApplyVertexDiff', 'ApplyBodyMorphs', 'QueueNiNodeUpdate', 'SetVertex', 'memcpy(', 'memset(', 'sleep_for', 'AddTask')) {
 if ($src.Contains($forbidden)) { throw "Unexpected recovery operation: $forbidden" }
}
$write = $src.IndexOf('hair->tintColor = color;')
$collect = $src.IndexOf('targets.Visit(roots.values[i])')
$budget = $src.IndexOf('if (budget.truncated)', $collect)
$reacquire = $src.IndexOf('Roots current;', $collect)
if ($collect -lt 0 -or $budget -le $collect -or $reacquire -le $budget -or $write -le $reacquire) { throw 'Collect, budget check, reacquire must all precede writes' }
if (($src.Split('hair->tintColor = color;').Count - 1) -ne 1) { throw 'Expected a single narrowly scoped render write site' }
foreach ($required in @('void Install();', 'void Drain(RE::Actor* a_player, std::uint32_t a_coveredSlotMask);', 'void Reset();', 'caller owns task scheduling')) {
 if (-not $header.Contains($required)) { throw "Missing public contract: $required" }
}
Write-Output 'HAIR_RECOVERY_GUARDS_OK: static only; event contract, compilation and engine behavior unverified'
