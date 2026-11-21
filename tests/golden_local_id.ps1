$ErrorActionPreference = 'Stop'
# Static guard for the 1.5.43 golden item fixes. Nothing here is compiled or executed.
# 1. A light plugin (ESL) keeps a 12 bit object id; storing 24 bits put the light
#    index inside the saved id and every golden item of such a plugin showed up
#    as '<not in this load order>'. The id width must be derived from the plugin.
# 2. Old files written by 1.5.39-1.5.42 must be repaired when they are read, so
#    the existing list starts working again instead of staying broken.
# 3. The golden page must not write the store on every click any more: only the
#    Export button saves, and the page says so.
# 4. The rows of both lists must be given an explicit size and the two panes must
#    stay inside the window, otherwise clicks fall through to nothing.
$root = Join-Path $PSScriptRoot '..'
$fail = @()
function Assert-Present([string] $haystack, [string] $needle, [string] $label) {
    if (-not $haystack.Contains($needle)) { $script:fail += "missing: $label ($needle)" }
}
function Assert-Absent([string] $haystack, [string] $needle, [string] $label) {
    if ($haystack.Contains($needle)) { $script:fail += "still present: $label ($needle)" }
}

$scan = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/PluginScan.cpp')
$scanHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/PluginScan.h')
$golden = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/GoldenItems.cpp')
$menu = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/MenuUI.cpp')

# --- 1. the id width follows the plugin -----------------------------------
Assert-Present $scanHeader 'std::uint32_t LocalIdFor(' 'LocalIdFor is declared'
Assert-Present $scanHeader 'RE::TESForm* ResolveForm(' 'ResolveForm is declared'
Assert-Present $scan 'a_index.light.emplace(' 'the light flag of every plugin is indexed'
Assert-Present $scan 'return IsLightPlugin(a_plugin) ? (a_formId & 0x00000FFFu) : (a_formId & 0x00FFFFFFu);' 'light plugins keep 12 bits'
Assert-Present $scan 'const std::uint32_t compact = LocalIdFor(named->second, a_localId);' 'ResolveForm uses the plugin width'
Assert-Present $scan 'const std::uint32_t wide = a_localId & 0x00FFFFFFu;' 'ResolveForm still tries the over-wide id'
Assert-Absent $scan 'auto* form = handler->LookupForm(a_localId, named->second);' 'ResolveArmor no longer looks the raw id up'

# --- 2. stored ids are written and repaired in the plugin width ------------
Assert-Present $golden 'entry.localId = PluginScan::LocalIdFor(entry.plugin, entry.localId);' 'reading repairs a stored id'
Assert-Present $golden 'auto* form = PluginScan::ResolveForm(entry.plugin, entry.localId);' 'resolving goes through ResolveForm'
Assert-Present $golden 'const std::uint32_t localId = PluginScan::LocalIdFor(plugin, full);' 'AddArmor stores the plugin width'
Assert-Present $golden 'entry.localId = PluginScan::LocalIdFor(entry.plugin, armor->GetFormID());' 'AddAllFromPlugin and AddLocal store the plugin width'
Assert-Absent $golden 'entry.localId = armor->GetFormID() & 0x00FFFFFFu;' 'no 24 bit truncation of a form id is left'
Assert-Absent $golden 'entry.localId = full & 0x00FFFFFFu;' 'AddArmor no longer truncates to 24 bits'

# --- 3. only the export button writes the file ----------------------------
$saves = ([regex]::Matches($menu, 'Golden::SaveToDisk\(\)')).Count
if ($saves -ne 1) { $fail += "the golden page must save on export only; found $saves Golden::SaveToDisk() call(s)" }
Assert-Present $menu 'bool        g_goldenDirty{ false };' 'the page tracks unsaved changes'
Assert-Present $menu '[not saved] The changes live in memory only' 'the page warns about unsaved changes'
Assert-Present $menu 'Changes reach the file only when you press Export.' 'the page says how saving works'

# --- 4. rows and panes ----------------------------------------------------
Assert-Present $menu 'bool GoldenRow(const char* a_label, bool a_selected)' 'a helper builds a hittable row'
Assert-Present $menu 'return IG::Selectable(a_label, a_selected, 0, ImVec2(width, height));' 'the row is given an explicit size'
Assert-Present $menu 'if (GoldenRow(label, isSelected)) {' 'the plugin groups use that row'
Assert-Present $menu 'if (GoldenRow(label, false)) {' 'the plugin picker uses that row'
Assert-Absent $menu 'if (IG::Selectable(label, isSelected)) {' 'no group row keeps the zero size selectable'
Assert-Present $menu 'const float editorWidth = std::max(320.0f, IG::ContentRegionAvail().x);' 'the editor pane gets a real width'
Assert-Present $menu 'std::max(240.0f, avail - 320.0f)' 'the list width is clamped to the free space'

if ($fail.Count -gt 0) {
    Write-Output 'GOLDEN_LOCAL_ID_FAILED'
    $fail | ForEach-Object { Write-Output (' - ' + $_) }
    exit 1
}
Write-Output 'GOLDEN_LOCAL_ID_OK'
