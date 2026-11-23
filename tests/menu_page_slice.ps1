# Guards the outfit list page slice against the wrap that crashed the game
# when an outfit was deleted while the list was on a later page (m10183).
# Run with PowerShell 7.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/MenuUI.cpp')

function Assert-Present([string]$text, [string]$what) {
    if (-not $text.Contains($what)) { throw "missing: $what" }
}
function Assert-Absent([string]$text, [string]$what) {
    if ($text.Contains($what)) { throw "must not appear: $what" }
}

# 1. the slice is clamped to the live list size
Assert-Present $ui 'std::size_t total = g_filtered.size();'
Assert-Present $ui 'std::size_t begin = std::min(total, static_cast<std::size_t>(std::max(0, (g_page - 1) * pageSize)));'
Assert-Present $ui 'std::size_t end = std::min(total, begin + static_cast<std::size_t>(pageSize));'

# 2. the old unguarded slice is gone; begin and end both come from 'total'
Assert-Absent $ui 'const std::size_t end = std::min(g_filtered.size(), begin + '
Assert-Absent $ui 'const std::size_t begin = static_cast<std::size_t>(std::max(0, (g_page - 1) * pageSize));'

# 3. the delete handler still empties the filter list, which is what made the
#    slice point past the end - keep the two facts tied together.
Assert-Present $ui 'g_filtered.clear();'
$slice = $ui.IndexOf('std::size_t total = g_filtered.size();')
$clear = $ui.IndexOf('g_filtered.clear();')
if ($slice -lt 0 -or $clear -lt 0 -or $slice -lt $clear) {
    throw 'the clamped slice must sit after the delete handler, in the same function'
}

# 4. the clamp must stay inside the row loops' source range (no negative index)
Assert-Present $ui 'for (std::size_t i = begin; i < end; ++i) {'

Write-Output 'MENU_PAGE_SLICE_OK'
