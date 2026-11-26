$ErrorActionPreference = 'Stop'
# Static guard for the language gate (1.5.82). Whether Chinese can be shown is a question
# about the font the menu framework loaded, nothing else. 1.5.81 answered it by looking for a
# file named exactly like the [Fonts] PrimaryFont value; the framework appends the extension
# itself, so a player who wrote 'wqy-microhei (CN)' lost Chinese although the font was loaded.
# This plugin now asks that very font through the framework's own igGetFont and
# ImFont_FindGlyphNoFallback, the answer is three-state (unknown / no / yes), and an unknown
# answer must never switch the language off - before the menu is drawn there is no font to ask.
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

$header = Read-RepoFile 'src/cpp/include/I18n.h'
Assert-Contains $header 'void RefreshFontProbe();' 'the font can be asked again while the menu is open'
Assert-Absent $header 'looks' 'the header no longer describes looking at a font file'
Assert-Absent $header 'reads its ini and looks at the font file' 'the header no longer claims a file check'

$i18n = Read-RepoFile 'src/cpp/I18n.cpp'
Assert-Contains $i18n 'std::atomic<int> g_cjkFont{ -1 };' 'the answer is kept three-state, unknown until the font is asked'
Assert-Contains $i18n 'if (g_cjkFont.load(std::memory_order_relaxed) < 0) {' 'an unknown answer does not take Chinese away'
Assert-Contains $i18n 'if (code == "chinese" && !state.available) {' 'Chinese still only stays when the font can draw it'
Assert-Contains $i18n 'void RefreshFontProbe()' 'the probe can be re-run'
Assert-Contains $i18n 'const int answer = IG::ChineseGlyphSupport();' 'the probe asks the host font'
Assert-Absent $i18n 'std::filesystem::exists(font, ec)' 'the file check that switched Chinese off is gone'
Assert-Absent $i18n 'PluginsDir() / "Fonts" / primaryFont' 'the ini value is no longer treated as a file name'

$apiHeader = Read-RepoFile 'src/cpp/include/ImGuiApi.h'
Assert-Contains $apiHeader 'ImFont* (*GetFont)();' 'the font the host draws with can be reached'
Assert-Contains $apiHeader 'const ImFontGlyph* (*FindGlyphNoFallback)(ImFont*, ImWchar);' 'a glyph can be asked for by code point'
Assert-Contains $apiHeader 'bool hasFontProbe{ false };' 'a host without the font query leaves the language alone'
Assert-Contains $apiHeader 'inline int ChineseGlyphSupport()' 'the probe answers three states'
Assert-Contains $apiHeader 'if (!api.hasFontProbe) {' 'no font query means unknown, not no'
Assert-Contains $apiHeader 'static_cast<ImWchar>(0x4E2D)' 'the probe looks for a common CJK ideograph'
Assert-Contains $apiHeader 'static_cast<ImWchar>(0x6587)' 'a second glyph is tried before the answer is no'

$api = Read-RepoFile 'src/cpp/ImGuiApi.cpp'
Assert-Contains $api '"igGetFont"' 'the font getter is resolved by name'
Assert-Contains $api '"ImFont_FindGlyphNoFallback"' 'the glyph lookup is resolved by name'
Assert-Contains $api 'table.hasFontProbe = table.extras.GetFont != nullptr && table.extras.FindGlyphNoFallback != nullptr;' 'both exports are required before the font is asked'

$menu = Read-RepoFile 'src/cpp/MenuUI.cpp'
if (([regex]::Matches($menu, 'I18n::RefreshFontProbe\(\);')).Count -lt 2) {
    Write-Output 'FAIL the font is asked both when the menu opens and on the first frame of the picker'
    $failures++
}

$version = Read-RepoFile 'src/cpp/include/version.h'
if ($version -match 'SKYRIMOUTFITSYSTEMNG_VERSION_PATCH\s+(\d+)') {
    if ([int]$Matches[1] -lt 82) {
        Write-Output "FAIL the host font probe arrived in 1.5.82 (found 1.5.$($Matches[1]))"
        $failures++
    }
} else {
    Write-Output 'FAIL the patch version cannot be read'
    $failures++
}

if ($failures -ne 0) {
    Write-Output "FONT_PROBE_FAILED $failures"
    exit 1
}
Write-Output 'FONT_PROBE_OK'
