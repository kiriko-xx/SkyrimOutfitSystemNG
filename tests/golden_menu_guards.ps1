$ErrorActionPreference = 'Stop'
# Static guard for the 1.5.40 menu fixes. Nothing here is compiled or executed.
# 1. Every I18n::T(english, chinese) pair in src must use the same printf
#    placeholders in the same order: a translation whose %s/%zu positions swap
#    hands a size_t to %s and takes the game down inside printf.
# 2. I18n must keep the runtime guard that falls back to English when a
#    translated line does not match, so a hand-edited translation file cannot
#    crash the game either.
# 3. The ESP outfit list must not be emptied by the clothing test: it filters
#    on any slot armour, not on "wearable clothing".
# 4. The clothing test must look at both sexes (a female-only clothing pack has
#    no male model).
function Signature([string] $text) {
    $out = @()
    foreach ($m in [regex]::Matches($text, '%[-+ #0]*(?:[0-9]+|\*)?(?:\.(?:[0-9]+|\*))?(?:hh|h|ll|l|L|z|j|t|w)?([diouxXeEfgGaAcspn%])')) {
        $conv = $m.Groups[1].Value
        if ($conv -eq '%') { continue }
        $out += $conv
    }
    return ($out -join '')
}
$root = Join-Path $PSScriptRoot '..'
$fail = @()
function Assert-Present([string] $haystack, [string] $needle, [string] $label) {
    if (-not $haystack.Contains($needle)) { $script:fail += "missing: $label ($needle)" }
}
function Assert-Absent([string] $haystack, [string] $needle, [string] $label) {
    if ($haystack.Contains($needle)) { $script:fail += "still present: $label ($needle)" }
}

$i18n = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/I18n.cpp')
$menu = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/MenuUI.cpp')
$scan = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/PluginScan.cpp')
$esp = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/EspOutfits.cpp')

# --- 1. placeholder order of every I18n::T pair in src -----------------------
$sources = Get-ChildItem -LiteralPath (Join-Path $root 'src') -Recurse -Include *.cpp, *.h
$pairPattern = [regex]'I18n::T\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"'
$checked = 0
foreach ($file in $sources) {
    $text = Get-Content -Raw -LiteralPath $file.FullName
    if ($null -eq $text) { continue }
    foreach ($m in $pairPattern.Matches($text)) {
        $checked++
        $en = Signature $m.Groups[1].Value
        $zh = Signature $m.Groups[2].Value
        if ($en -ne $zh) {
            $fail += ("placeholder order differs in {0}: [{1}] vs [{2}]" -f $file.Name, $en, $zh)
        }
    }
}
if ($checked -lt 100) { $fail += "only $checked I18n::T pair(s) were scanned; the scan is broken" }

# --- 2. the runtime guard stays --------------------------------------------
Assert-Present $i18n 'bool PlaceholdersMatch(' 'translation placeholder check'
Assert-Present $i18n 'PlaceholderSignature(' 'placeholder signature helper'
Assert-Present $i18n 'does not use the placeholders' 'translation rejected warning'
Assert-Present $i18n 'do not match the English text' 'translation rejected summary'
Assert-Present $i18n 'PlaceholdersMatch(entry.en, entry.zh)' 'dictionary lines validated before use'
Assert-Present $i18n 'PlaceholdersMatch(key, value)' 'translation files validated before use'

# --- 3. the ESP outfit list is not emptied by the clothing test ------------
Assert-Present $esp 'entry.armors == 0' 'plugin row filtered on any slot armour'
Assert-Absent $esp 'entry.outfitArmors == 0' 'clothing-only filter that emptied the list'

# --- 4. the clothing test looks at both sexes ------------------------------
Assert-Present $scan 'RE::SEXES::kMale, RE::SEXES::kFemale' 'both sexes in the clothing test'
Assert-Present $scan 'bipedModels[sex]' 'first person and third person models'
Assert-Present $scan 'enum class OutfitVerdict' 'clothing verdict'
Assert-Present $scan 'return Verdict(a_armor) == OutfitVerdict::kWearable;' 'clothing test uses the verdict'

# --- 5. golden item lines keep the arguments in the English order ----------
Assert-Absent $menu '已添加 %s 的 %zu 件' 'swapped placeholders in the golden item line'
Assert-Absent $menu '已从 %s 导入 %zu 件' 'swapped placeholders in the golden import line'
Assert-Present $menu '已添加 %zu 件，来自 %s' 'golden item line matches the English order'
Assert-Present $menu '已导入 %zu 件，来源 %s' 'golden import line matches the English order'

if ($fail.Count -gt 0) {
    $fail | ForEach-Object { Write-Host "FAIL: $_" }
    throw "golden_menu_guards: $($fail.Count) assertion(s) failed"
}
Write-Host "GOLDEN_MENU_GUARDS_OK: $checked I18n::T pair(s) keep their placeholder order, the runtime guard is in place, the ESP list filters on slot armour, the clothing test checks both sexes."
