$ErrorActionPreference = 'Stop'
# Static guard only: proves the scene module switches outfits the same way the
# location/state switching does, and that the old bespoke machinery is gone.
# It does not compile or run anything and says nothing about engine behaviour.
$root = Join-Path $PSScriptRoot '..'
$scene = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/SceneOutfits.cpp')
$sceneHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/SceneOutfits.h')
$auto = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/AutoSwitch.cpp')
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/main.cpp')
$hotkeys = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/NGHotkeys.cpp')
$fail = @()
function Assert-Absent([string] $haystack, [string] $needle, [string] $label) {
    if ($haystack.Contains($needle)) { $script:fail += "still present: $label ($needle)" }
}
function Assert-Present([string] $haystack, [string] $needle, [string] $label) {
    if (-not $haystack.Contains($needle)) { $script:fail += "missing: $label ($needle)" }
}

# 1. The bespoke scene transport must be gone.
foreach ($token in @('QueueSceneEvent', 'DrainSceneEvents', 'ScheduleDrain', 'g_pendingEvents',
        'g_generation', 'g_drainScheduled', 'void Tick()', 'GetTaskInterface', 'AddTask',
        'std::deque', 'GameState::IsLoading')) {
    Assert-Absent $scene $token 'scene transport'
}
Assert-Absent $sceneHeader 'void Tick();' 'scene Tick declaration'
Assert-Absent $main 'SceneOutfits::Tick();' 'main.cpp Tick call'
Assert-Absent $hotkeys 'SceneOutfits::Tick();' 'NGHotkeys.cpp Tick call'

# 2. No compensation layers may come back.
foreach ($token in @('RunIllusionCycle', 'RequestOutfitPasses', 'rebuildActorModel', 'Update3DModel',
        'ClearBipedPart', 'refreshArmorForOutfitUsers', 'set_outfit_c')) {
    Assert-Absent $scene $token 'scene side mechanism'
}

# 3. The switch is the two steps the state switching runs, in the same order.
Assert-Present $scene 'OutfitSystem::setOutfitByName(service->inner(), a_actor, a_name.c_str())' 'scene assigns through setOutfitByName'
$assign = $scene.IndexOf('OutfitSystem::setOutfitByName')
$refresh = $scene.IndexOf('OutfitSystem::refreshArmorFor(a_actor);')
if ($refresh -lt 0 -or $assign -lt 0 -or $refresh -le $assign) { $fail += 'the outfit refresh must follow the assignment' }
Assert-Present $auto 'OutfitSystem::setOutfitUsingState(service->inner(), a_actor);' 'state switching assignment'
$autoAssign = $auto.IndexOf('setOutfitUsingState')
$autoRefresh = $auto.IndexOf('OutfitSystem::refreshArmorFor(a_actor);')
if ($autoRefresh -lt 0 -or $autoAssign -lt 0 -or $autoRefresh -le $autoAssign) { $fail += 'AutoSwitch no longer assigns then refreshes' }
if ($scene -match '(?s)set_outfit_c|has_outfit\([^)]*\)\s*\)\s*\{[^}]*setOutfitByName') { }

# 4. The scene outfit is only a name: "<current>_sex", exact then ignoring case.
Assert-Present $scene 'current + "_sex"' 'scene outfit name'
Assert-Present $scene 'ToLower(name) == wanted' 'case insensitive fallback'
Assert-Present $scene 'EndsWithSexSuffix(current)' 'already-scene guard'
foreach ($token in @('ostim_start', 'ostim_end', 'PlayerAnimationStart', 'PlayerAnimationEnd')) {
    Assert-Present $scene $token 'mod event name'
}
Assert-Present $scene 'StartScene("OStim")' 'inline OStim switch'
Assert-Present $scene 'EndScene("SexLab")' 'inline SexLab restore'
Assert-Present $scene 'g_switchMutex' 'scene switch serialisation'

# 5. The service entry point exists exactly once and mirrors the state one.
$svc = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/OutfitSystem.cpp')
$svcHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/include/OutfitSystem.h')
Assert-Present $svc 'bool setOutfitByName(OutfitService& service, RE::Actor* actor, const char* name) {' 'setOutfitByName definition'
Assert-Present $svcHeader 'bool setOutfitByName(SOS::OutfitService& a_service, RE::Actor* a_actor, const char* a_name);' 'setOutfitByName declaration'
if (([regex]::Matches($svc, 'bool setOutfitByName\(')).Count -ne 1) { $fail += 'setOutfitByName must be defined exactly once' }

if ($fail.Count -gt 0) {
    $fail | ForEach-Object { Write-Error $_ }
    throw 'SCENE_OUTFIT_SWITCH_GUARD_FAILED'
}
Write-Output 'SCENE_OUTFIT_SWITCH_OK: scene switching = name lookup + setOutfitByName + refreshArmorFor, same two steps as the state switching; no queue/Tick/side mechanisms. Static only: engine behaviour unverified.'
