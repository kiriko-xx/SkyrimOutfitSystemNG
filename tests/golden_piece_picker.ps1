$ErrorActionPreference = 'Stop'
# Static guard for the 1.5.45 golden piece picker. Nothing here is compiled or executed.
# 1.5.45 replaced the hand typed local form id with two lists: one opened from the
# "Add one piece from a plugin" button, one inside the editor of a group. Both must
# list the slot armour of one plugin and add a piece when its row is clicked, so a
# wrong id can no longer be typed in and every plugin of the load order is reachable.
$root = Join-Path $PSScriptRoot '..'
$fail = @()
function Assert-Present([string] $haystack, [string] $needle, [string] $label) {
    if (-not $haystack.Contains($needle)) { $script:fail += "missing: $label ($needle)" }
}
function Assert-Absent([string] $haystack, [string] $needle, [string] $label) {
    if ($haystack.Contains($needle)) { $script:fail += "still present: $label ($needle)" }
}

$menu = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/MenuUI.cpp')
$i18n = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/I18n.cpp')

# --- 1. the typed form id is gone ------------------------------------------------
Assert-Absent $menu '"##goldenlocalid"' 'the typed form id box is gone'
Assert-Absent $menu 'ParseLocalId(' 'the form id parser is gone'
Assert-Absent $menu 'Add a piece of this plugin by its local form id in hex' 'the typed id hint is gone'

# --- 2. one shared list renderer exists ------------------------------------------
Assert-Present $menu 'void RenderGoldenPieceList(const std::string& a_plugin, const std::vector<std::uint32_t>& a_added' 'the piece list renderer exists'
Assert-Present $menu 'const auto armors = PluginScan::ArmorsOf(a_plugin);' 'the list is built from the plugin scan'
Assert-Present $menu 'Golden::AddLocal(a_plugin, localId, message)' 'a click adds a single piece'
Assert-Present $menu 'IG::PushID(a_scopeId);' 'the two lists keep separate ids'
Assert-Present $menu 'IG::PushID(static_cast<int>(rowIndex));' 'piece rows use a frame stable id'
Assert-Present $menu 'IG::InputTextWithHint("##goldenpiecefilter"' 'the list has its own search box'
Assert-Present $menu 'SKSE::log::info("Menu: golden piece' 'adding a piece logs at info level'
Assert-Present $i18n '"[in the list] %s  %s", "[已加入] %s  %s"' 'the added marker keeps both placeholders in order'

# --- 3. both entry points call it ------------------------------------------------
$calls = [regex]::Matches($menu, 'RenderGoldenPieceList\(').Count
if ($calls -ne 3) { $fail += "expected one definition and two calls of RenderGoldenPieceList, found $calls" }
Assert-Present $menu 'g_goldenPiecePlugin = it->name;' 'picking a plugin opens its piece list'
Assert-Present $menu 'RenderGoldenPieceList(g_goldenPiecePlugin, AddedIdsOf(groups, g_goldenPiecePlugin), 1,' 'the picker lists the chosen plugin'
Assert-Present $menu 'RenderGoldenPieceList(selected->plugin, selected->localIds, 2,' 'the editor lists the selected group'
Assert-Present $menu 'AddedIdsOf(groups, g_goldenPiecePlugin)' 'pieces already in the store are resolved'
Assert-Present $menu 'Add one piece from a plugin...' 'the button no longer says by id'
Assert-Absent $menu 'Add one piece by id...' 'the old button label is gone'

# --- 4. the rule of the page stays: nothing is written without Export ------------
$saves = [regex]::Matches($menu, 'Golden::SaveToDisk\(\)').Count
if ($saves -ne 1) { $fail += "the golden page must save from the export path only, found $saves SaveToDisk call(s)" }
Assert-Present $menu 'g_goldenDirty = true;' 'adding a piece marks the store dirty instead of writing it'

if ($fail.Count -ne 0) {
    $fail | ForEach-Object { Write-Host "FAIL $_" }
    exit 1
}
Write-Host 'GOLDEN_PIECE_PICKER_OK: the single piece is picked out of a plugin list in both places, rows use stable ids, nothing is written without Export.'