$ErrorActionPreference = 'Stop'
# Static guard for 1.5.92. Three things the player asked for:
#  (A) the 'auto' language follows the language picked on the SKSE Menu Framework settings
#      page, which the framework stores as the [Fonts] Enable* switches in
#      SKSEMenuFramework.ini - never the game's sLanguage;
#  (B) adding a single piece from the ESP list says which pieces of the outfit already cover
#      the same slot, naming them and their slot numbers, and draws the choice under the row
#      that was clicked;
#  (C) opening the outfits page puts the cursor on the outfit the actor is wearing right now,
#      the same way the quick picker already does.
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

function Assert-InOrder([string]$text, [string]$first, [string]$second, [string]$label) {
    $a = $text.IndexOf($first)
    $b = $text.IndexOf($second)
    if ($a -ge 0 -and $b -gt $a) { return }
    Write-Output "FAIL $label ($first at $a, $second at $b)"
    $script:failures++
}

$i18n = Read-RepoFile 'src/cpp/I18n.cpp'
$i18nHeader = Read-RepoFile 'src/cpp/include/I18n.h'
$menu = Read-RepoFile 'src/cpp/MenuUI.cpp'
$editor = Read-RepoFile 'src/cpp/MenuOutfitEditor.cpp'
$settings = Read-RepoFile 'src/core/Settings.h'
$ini = Read-RepoFile 'dist/SKSE/Plugins/SkyrimOutfitSystemNG.ini'

# A. the menu language comes from the framework's own switches
Assert-Contains $i18n 'std::string MenuLanguageCode()' 'the menu language is read from the framework'
Assert-Contains $i18n 'SKSEMenuFramework.ini' 'the framework stores it in its own ini'
Assert-Contains $i18n 'EnableChinese' 'the chinese switch is read'
Assert-Contains $i18n 'EnableJapanese' 'the japanese switch is read'
Assert-Contains $i18n 'EnableKorean' 'the korean switch is read'
Assert-Contains $i18n 'EnableCyrillic' 'the cyrillic switch is read'
Assert-Contains $i18n 'EnableThai' 'the thai switch is read'
Assert-Contains $i18n 'EnableTurkish' 'the turkish switch is read'
Assert-Contains $i18n 'EnablePolish' 'the polish switch is read'
Assert-Contains $i18n 'EnableVietnamese' 'the vietnamese switch is read'
Assert-Contains $i18n 'requested = MenuLanguageCode();' 'auto resolves through the menu language'
Assert-Absent $i18n 'GameLanguageCode' 'the game language reader is gone'
Assert-Absent $i18n 'sLanguage:General' 'nothing reads the game language any more'
Assert-InOrder $i18n 'EnableChinese' 'EnableJapanese' 'the framework order is kept (chinese first)'
Assert-Contains $i18nHeader 'SKSE Menu Framework' 'the header says what auto follows'
Assert-Contains $i18n 'Auto (menu language)' 'the dropdown entry says menu language'
Assert-Absent $i18n 'Auto (game language)' 'the old dropdown text is gone'
Assert-Contains $menu 'Auto (menu language)' 'the dropdown draws the new text'
Assert-Absent $menu 'Auto (game language)' 'the dropdown no longer offers the game language'
Assert-Contains $settings 'SKSE Menu Framework' 'the ini description says the same'
Assert-Contains $ini 'SKSE Menu Framework' 'the shipped ini says the same'

# B. the slot conflict names the pieces that are in the way
Assert-Contains $editor 'struct PendingConflict' 'the conflict list has a shape'
Assert-Contains $editor 'std::vector<PendingConflict> g_pendingConflicts;' 'the conflict list is kept'
Assert-Contains $editor 'g_pendingFromPieceList{ false };' 'the piece list is remembered as the source'
Assert-Contains $editor 'std::string SlotNumbersOfMask(' 'slot numbers are computed from a mask'
Assert-Contains $editor 'void CollectConflicts(' 'the blocking pieces are collected'
Assert-Contains $editor 'bool a_fromPieceList = false' 'only the piece list asks for the inline choice'
Assert-Contains $editor 'std::string ConflictPiecesText()' 'the blocking pieces are named in one line'
Assert-Contains $editor 'I18n::Tr("%s (slot %s)")' 'each blocking piece is named with its slot number'
Assert-Contains $editor 'void RenderConflictChoice()' 'the choice is drawn once, by one function'
Assert-Contains $editor 'if (g_pending && !g_pendingFromPieceList) {' 'the floating prompt skips the piece list'
Assert-Contains $editor 'AddArmor(a_outfit, armor, true)' 'the piece list passes its origin on'
Assert-Contains $editor 'if (g_pending == armor && g_pendingOutfit == a_outfit) {' 'the prompt is drawn under the clicked row'
Assert-Absent $editor 'void RenderPendingConflict()' 'the old prompt function is gone'
Assert-InOrder $editor 'struct PendingConflict' 'void CollectConflicts(' 'the struct is declared before it is used'
Assert-InOrder $editor 'void CollectConflicts(' 'void RenderConflictChoice()' 'collecting happens before drawing'
Assert-Contains $i18n '%s uses a body slot this outfit already covers: %s.' 'the sentence exists in english'
Assert-Contains $i18n '要用的槽位已被本预设占用' 'the sentence exists in chinese'

# C. the outfits page focuses the worn outfit when it opens
Assert-Contains $menu 'g_outfitsFocusWorn{ false };' 'the focus request is remembered'
Assert-Contains $menu 'void FocusWornOutfit()' 'there is one function that focuses the worn outfit'
Assert-Contains $menu 'g_outfitsFocusWorn = true;' 'opening the menu asks for the focus'
Assert-Contains $menu 'if (g_outfitsFocusWorn) {' 'the focus request is consumed inside the page'
Assert-Contains $menu 'g_outfitsFocusWorn = false;' 'the request is cleared once used'
Assert-Contains $menu 'current_outfit_ptr(target->GetFormID())' 'the worn outfit is taken from the service'
Assert-InOrder $menu 'void FocusWornOutfit()' 'g_outfitsFocusWorn = false;' 'focusing is defined before it is consumed'

# the translation files carry the renamed entry and the two new sentences
$en = [System.IO.File]::ReadAllText((Join-Path $root 'dist/interface/translations/SkyrimOutfitSystemNG_english.txt'), [System.Text.Encoding]::Unicode)
Assert-Contains $en '$Auto (menu language)' 'the english file renames auto'
Assert-Absent $en 'Auto (game language)' 'the english file has no old auto entry'
Assert-Contains $en 'uses a body slot this outfit already covers' 'the english file has the conflict sentence'
Assert-Contains $en '(slot %s)' 'the english file has the slot suffix'
$zh = [System.IO.File]::ReadAllText((Join-Path $root 'dist/interface/translations/SkyrimOutfitSystemNG_chinese.txt'), [System.Text.Encoding]::Unicode)
Assert-Contains $zh '$Auto (menu language)' 'the chinese file keeps the english key'
Assert-Contains $zh '自动（跟随菜单语言）' 'the chinese file renames auto'
Assert-Absent $zh 'Auto (game language)' 'the chinese file has no old auto entry'
Assert-Contains $zh '要用的槽位已被本预设占用' 'the chinese file has the conflict sentence'
Assert-Contains $zh '（槽位 %s）' 'the chinese file has the slot suffix'

# version
$version = Read-RepoFile 'src/cpp/include/version.h'
if ($version -notmatch 'VERSION_PATCH (\d+)' -or [int]$Matches[1] -lt 92) { Write-Output 'FAIL version.h is not at 1.5.92 or later'; $failures++ }

if ($failures -gt 0) { Write-Output "MENU_1_5_92_FAILED $failures"; exit 1 }
Write-Output 'MENU_1_5_92_OK: auto follows the menu framework, the slot conflict names the blocking pieces under the clicked row, and the outfits page opens on the worn outfit'