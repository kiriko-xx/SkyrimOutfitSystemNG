$ErrorActionPreference = 'Stop'
# Static guard for the one-piece-per-body-slot rule (1.5.55): every path that
# puts armour into an outfit has to go through the slot check, and the bulk
# delete has to keep its tick set in sync with the preset list.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$failures = 0

function Read-RepoFile([string]$relative) {
    $path = Join-Path $root $relative
    if (-not (Test-Path $path)) { throw "missing $relative" }
    return (Get-Content -Raw -LiteralPath $path)
}

function Assert-Contains([string]$text, [string]$needle, [string]$label) {
    if ($text.Contains($needle)) { return }
    Write-Output "FAIL $label (missing: $needle)"
    $script:failures++
}

function Assert-Absent([string]$text, [string]$needle, [string]$label) {
    if (-not $text.Contains($needle)) { return }
    Write-Output "FAIL $label (unexpected: $needle)"
    $script:failures++
}

$outfit = Read-RepoFile 'src/core/Outfit.cpp'
Assert-Contains $outfit 'std::vector<const RE::TESObjectARMO*> Outfit::slot_conflicts' 'Outfit::slot_conflicts exists'
Assert-Contains $outfit 'bool Outfit::insert_armor_slot_checked' 'Outfit::insert_armor_slot_checked exists'
Assert-Contains $outfit 'KeepSlotUnique' 'KeepSlotUnique helper exists'
Assert-Contains $outfit 'SlotsCollide' 'slot comparison goes through SlotRules'

$service = Read-RepoFile 'src/core/OutfitService.cpp'
Assert-Contains $service 'KeepSlotUnique(a_armors, &dropped)' 'create/replace filter through KeepSlotUnique'
Assert-Contains $service 'insert_armor_slot_checked(armor)' 'modify_outfit checks the slot'
Assert-Contains $service 'std::size_t OutfitService::delete_outfits' 'batch delete exists'

$papyrus = Read-RepoFile 'src/cpp/OutfitSystem.cpp'
Assert-Contains $papyrus 'insert_armor_slot_checked(armor, &conflicts)' 'AddArmorToOutfit checks the slot'
Assert-Contains $papyrus 'KeepSlotUnique(pieces, &dropped)' 'OverwriteOutfit checks the slot'

$clipboard = Read-RepoFile 'src/cpp/OutfitClipboard.cpp'
Assert-Contains $clipboard 'KeepSlotUnique' 'paste checks the slot'

$menu = Read-RepoFile 'src/cpp/MenuUI.cpp'
Assert-Contains $menu 'g_bulkMode' 'bulk delete mode exists'
Assert-Contains $menu 'g_bulkSelected' 'bulk tick set exists'
Assert-Contains $menu 'delete_outfits(names)' 'bulk delete calls the service'
Assert-Contains $menu '"已勾选 %zu 套幻化"' 'bulk tick counter is shown'

$editor = Read-RepoFile 'src/cpp/MenuOutfitEditor.cpp'
Assert-Contains $editor 'From a plugin (ESP)' 'editor has a plugin piece browser'
Assert-Contains $editor 'PluginScan::ArmorsOf' 'plugin piece browser reads plugin armour'
Assert-Contains $editor 'SlotsCollide' 'plugin piece browser marks taken slots'

if ($failures -ne 0) {
    Write-Output "SLOT_UNIQUENESS_FAILED $failures"
    exit 1
}
Write-Output 'SLOT_UNIQUENESS_OK'
