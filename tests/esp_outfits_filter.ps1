$ErrorActionPreference = 'Stop'
# Static guard only: proves which records each generator is allowed to take.
# "Generate outfits from a plugin" must skip anything that is not ordinary
# clothing (FX effects, unnamed records, records with no model); golden items
# deliberately take any record. Nothing here is compiled or executed.
$root = Join-Path $PSScriptRoot '..'
$esp = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/EspOutfits.cpp')
$espHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/EspOutfits.h')
$scan = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/PluginScan.cpp')
$scanHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/PluginScan.h')
$golden = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/GoldenItems.cpp')
$fail = @()
function Assert-Absent([string] $haystack, [string] $needle, [string] $label) {
    if ($haystack.Contains($needle)) { $script:fail += "still present: $label ($needle)" }
}
function Assert-Present([string] $haystack, [string] $needle, [string] $label) {
    if (-not $haystack.Contains($needle)) { $script:fail += "missing: $label ($needle)" }
}

# 1. The outfit generator goes through the clothing test.
Assert-Present $esp 'PluginScan::IsOutfitArmor(armor)' 'clothing test on the outfit path'
Assert-Absent $esp 'kFxBit' 'do not erase or filter the FX bit again when combining ordinary pieces'
Assert-Present $esp 'armor->GetSlotMask().underlying() != item.mask' 'confirmation checks the full mask'
Assert-Present $esp '(occupied & item.mask) != 0' 'all occupied slots take part in overlap checks'
Assert-Present $esp 'not clothing' 'skipped-record report'
Assert-Present $espHeader 'outfitArmors' 'clothing count in the plugin row'

# 2. The clothing test itself looks at all three things the user named.
Assert-Present $scan 'bool IsOutfitArmor(const RE::TESObjectARMO* a_armor)' 'clothing test definition'
Assert-Present $scan 'GetFormEditorID()' 'editor id test'
Assert-Present $scan 'const char* fullName = a_armor->GetFullName();' 'use the display name when the runtime EditorID is unavailable'
Assert-Present $scan '&& (!fullName || fullName[0]' 'only skip when neither name is available'
Assert-Present $scan 'if ((mask & ~kFxBit) == 0)' 'exclude pure FX without excluding ordinary mixed-slot pieces'
Assert-Present $scan 'constexpr std::uint32_t kFxBit = 1u << 31;' 'FX slot mask'
Assert-Present $scan 'armorAddons.size() == 0' 'armour addon test'
Assert-Present $scan 'GetModel()' 'model test'
Assert-Present $scanHeader 'bool IsOutfitArmor' 'clothing test declaration'

# 3. Golden items are exempt: no clothing test anywhere on their path.
Assert-Absent $golden 'IsOutfitArmor' 'clothing test on the golden path'
Assert-Present $golden 'PluginScan::ArmorsOf(a_plugin)' 'golden items take the whole plugin'
# 1.5.43: the single piece lookup goes through ResolveForm so an id of a light
# plugin is read in its own width, and so a piece without a display slot can be
# named in the editor instead of showing "<not in this load order>".
Assert-Present $golden 'PluginScan::ResolveForm(a_plugin, a_localId)' 'single piece lookup stays'
Assert-Present $golden 'form->As<RE::TESObjectARMO>()' 'single piece lookup still wants an armour record'

if ($fail.Count -gt 0) {
    $fail | ForEach-Object { Write-Host "FAIL: $_" }
    throw "esp_outfits_filter: $($fail.Count) assertion(s) failed"
}
Write-Host 'ESP_OUTFITS_FILTER_OK: outfits take ordinary clothing only (FX, unnamed and model-less records skipped); golden items stay unfiltered.'
