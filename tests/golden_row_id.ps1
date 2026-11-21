$ErrorActionPreference = 'Stop'
# Static guard for the 1.5.44 golden list fix. Nothing here is compiled or executed.
# The group rows could not be clicked because their ImGui id was built from
# group.plugin.c_str(): Golden::Groups() returns a fresh vector every frame, so the
# pointer changed between the frame the mouse went down and the frame it came up and
# ImGui dropped the click. Every list row must use an id that is stable across frames,
# and a click must leave an info level line so the next log can prove it.
$root = Join-Path $PSScriptRoot '..'
$fail = @()
function Assert-Present([string] $haystack, [string] $needle, [string] $label) {
    if (-not $haystack.Contains($needle)) { $script:fail += "missing: $label ($needle)" }
}
function Assert-Absent([string] $haystack, [string] $needle, [string] $label) {
    if ($haystack.Contains($needle)) { $script:fail += "still present: $label ($needle)" }
}

$menu = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/MenuUI.cpp')
$goldenHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/GoldenItems.h')

# --- 1. Groups() really hands out a fresh copy (the reason ids must not be pointers)
Assert-Present $goldenHeader 'std::vector<Group> Groups();' 'Groups returns by value'

# --- 2. the group rows use a frame stable id -------------------------------------
Assert-Absent $menu 'PushID(group.plugin.c_str())' 'no pointer id for the group rows'
Assert-Present $menu 'IG::PushID(static_cast<int>(rowIndex));' 'the group rows are pushed by row number'
Assert-Present $menu 'std::size_t rowIndex = 0;' 'the row index exists'
Assert-Present $menu '++rowIndex;' 'the row index advances'
# no other place may push a string pointer as an id
$pointerPushes = [regex]::Matches($menu, 'IG::PushID([A-Za-z_][A-Za-z0-9_.]*.c_str())')
if ($pointerPushes.Count -ne 0) { $fail += "a row still pushes a string pointer as its id (" + $pointerPushes.Count + " place(s))" }

# --- 3. every click is provable from an info level log ---------------------------
Assert-Present $menu 'SKSE::log::info("Menu: golden group' 'selecting a group logs at info level'
Assert-Present $menu 'selected ({} of {}).' 'the log carries the row position'
Assert-Absent $menu 'SKSE::log::debug("Menu: golden group' 'the selection log is no longer debug only'

# --- 4. the rows keep an explicit hit box (1.5.43 behaviour must survive) --------
Assert-Present $menu 'const float width = std::max(IG::ContentRegionAvail().x, IG::CalcTextSize(a_label).x + 32.0f);' 'rows keep an explicit width'
Assert-Present $menu 'const float height = IG::CalcTextSize("A").y + 8.0f;' 'rows keep an explicit height'
Assert-Present $menu 'GoldenRow(label, false)' 'the plugin picker uses the same row helper'
Assert-Present $menu 'GoldenRow(label, isSelected)' 'the group list uses the same row helper'

if ($fail.Count -ne 0) {
    $fail | ForEach-Object { Write-Host "FAIL: $_" }
    exit 1
}
Write-Host 'GOLDEN_ROW_ID_OK: the golden group rows use a frame stable id, every click logs at info level and the rows keep their explicit hit box.'
