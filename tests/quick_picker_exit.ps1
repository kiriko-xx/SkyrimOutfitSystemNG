# Static guard for the 1.5.77 quick picker changes.
# What has to stay true:
#  - the only log file the plugin writes is OutfitSystemNG.log. SKSE::Init used to
#    be called without an InitInfo, and CommonLibSSE-NG then opens a second file
#    named after the plugin (SkyrimOutfitSystemNG.log) and puts one line in it.
#    The init call has to pass InitInfo{ .log = false } so InitializeLog() stays
#    the only owner of the log.
#  - Tab closes the quick picker exactly like Escape, also while the search box
#    has focus, and the pad keeps B for the same job.
#  - the hint under the list is one line per device (keyboard, controller) and
#    each line wraps instead of being cut off when the window is narrow.
#  - every hint literal has a dictionary entry, otherwise the Chinese UI falls
#    back to English for it.
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$script:failures = 0

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

function Get-Block([string]$text, [string]$start, [string]$end) {
    $from = $text.IndexOf($start)
    if ($from -lt 0) { throw "block start not found: $start" }
    $to = $text.IndexOf($end, $from)
    if ($to -lt 0) { throw "block end not found: $end" }
    return $text.Substring($from, $to - $from)
}

# ---- one log file only: the framework default logger is off ----
$main = Read-RepoFile "src/cpp/main.cpp"
Assert-Contains $main "SKSE::Init(a_skse, SKSE::InitInfo{ .log = false });" "the framework logger is turned off so no second log file is created"
Assert-Absent $main "SKSE::Init(a_skse);" "the bare Init call would bring the stray SkyrimOutfitSystemNG.log back"
Assert-Contains $main "InitializeLog();" "our own log setup is still what runs"

# ---- Escape and Tab both close the picker ----
$menu = Read-RepoFile "src/cpp/MenuUI.cpp"
$closeBlock = Get-Block $menu "void RenderQuickPickerBody()" "IG::Separator();"
Assert-Contains $closeBlock "IG::IsKeyPressed(ImGuiKey_Escape)" "Escape closes the quick picker"
Assert-Contains $closeBlock "IG::IsKeyPressed(ImGuiKey_Tab)" "Tab closes the quick picker as well"
Assert-Contains $closeBlock "CloseQuickPicker();" "the close path is the picker's own"

# ---- the hint under the list: one line per device, wrapped when narrow ----
$body = Get-Block $menu "void RenderQuickPickerBody()" "void RenderQuickPicker()"
Assert-Contains $body "hintLine("W/S: select   A/D: page   E: wear   R: random   X: remove   Esc/Tab: close");" "the keyboard hint lists the new exit key too"
Assert-Contains $body "hintLine("Controller: D-pad: select/page   A: wear   Y: random   X: remove   B: close");" "the controller hint is a line of its own"
Assert-Absent $body "W/S: select   A/D: page   E: wear   R: random   X: remove   Esc: close" "the single device-less hint line is gone"
Assert-Contains $body "IG::CalcTextSize(text).x > IG::ContentRegionAvail().x" "a hint line that does not fit the window is measured"
Assert-Contains $body "IG::TextWrapped("%s", text);" "the measured line is wrapped instead of cut off"
Assert-Contains $body "IG::TextDisabled("%s", text);" "a line that fits keeps the dimmed style"

# ---- both hints are translatable ----
$i18n = Read-RepoFile "src/cpp/I18n.cpp"
Assert-Contains $i18n "W/S: select   A/D: page   E: wear   R: random   X: remove   Esc/Tab: close" "the keyboard hint has a dictionary entry"
Assert-Contains $i18n "Controller: D-pad: select/page   A: wear   Y: random   X: remove   B: close" "the controller hint has a dictionary entry"
Assert-Contains $i18n "Esc/Tab：关闭" "the keyboard hint has a Chinese line"
Assert-Contains $i18n "手柄：十字键 选择/翻页" "the controller hint has a Chinese line"

if ($failures -gt 0) {
    Write-Output "QUICK_PICKER_EXIT_FAILED: $failures problem(s)"
    exit 1
}
Write-Output "QUICK_PICKER_EXIT_OK: the plugin writes exactly one log file (the framework logger is off), Escape and Tab both close the quick picker, and the hint under the list is a keyboard line plus a controller line that wraps instead of being cut off, both translatable"
exit 0
