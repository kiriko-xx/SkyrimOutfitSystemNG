$ErrorActionPreference = 'Stop'
# Static guard for the 1.5.41 start-up crash. Nothing here is compiled or run.
# 1.5.40 died on start-up with
#     std::system_error 'resource deadlock would occur'
# raised inside Golden::LoadFromDisk -> ImportText: the store took g_mutex and
# then called the public TotalCount(), which takes g_mutex again. A std::mutex
# reports that re-entry as an exception on MSVC and an exception escaping the
# SKSE message handler closes the game.
# 1. The store lock must stay recursive.
# 2. Code paths that already hold the lock must use TotalCountLocked().
# 3. Loading the store at start-up must be wrapped, so a broken store cannot
#    take the game down.
# 4. MessageHandler must catch whatever a phase throws.
$root = Join-Path $PSScriptRoot '..'
$fail = @()
function Assert-Present([string] $haystack, [string] $needle, [string] $label) {
    if (-not $haystack.Contains($needle)) { $script:fail += "missing: $label ($needle)" }
}
function Assert-Absent([string] $haystack, [string] $needle, [string] $label) {
    if ($haystack.Contains($needle)) { $script:fail += "still present: $label ($needle)" }
}

$golden = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/GoldenItems.cpp')
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/main.cpp')

# --- 1. the store lock is recursive -----------------------------------------
Assert-Present $golden 'std::recursive_mutex        g_mutex;' 'recursive store lock'
Assert-Absent $golden 'std::mutex                  g_mutex;' 'plain store lock'

# --- 2. no locking getter is called from a locked path ----------------------
Assert-Present $golden 'std::size_t TotalCountLocked()' 'locked count helper'
Assert-Absent $golden 'std::to_string(TotalCount())' 'locking getter while the message is built'
$importedLine = ($golden -split "`n" | Where-Object { $_ -match 'entr\(ies\) imported' }) -join ''
Assert-Present $importedLine 'TotalCountLocked()' 'ImportText counts without the lock'
$loadedLine = ($golden -split "`n" | Where-Object { $_ -match 'entr\(ies\) loaded from' }) -join ''
Assert-Present $loadedLine 'TotalCountLocked()' 'LoadFromDisk counts without the lock'

# --- 3. start-up load is wrapped -------------------------------------------
$loadCall = $main.IndexOf('SOS::Golden::LoadFromDisk();')
if ($loadCall -lt 0) { $fail += 'missing: start-up load call' }
else {
    $before = $main.Substring([Math]::Max(0, $loadCall - 400), [Math]::Min(400, $loadCall))
    if (-not $before.Contains('try {')) { $fail += 'start-up load is not inside a try block' }
}
Assert-Present $main 'the golden list stays empty for this session' 'start-up load fallback log'
Assert-Present $main 'void HandleMessage(' 'message handling body'

# --- 4. the SKSE callback cannot throw out --------------------------------
Assert-Present $main 'void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {' 'message wrapper'
Assert-Present $main 'HandleMessage(a_msg);' 'message wrapper body'
Assert-Present $main 'an exception escaped' 'message wrapper log'

if ($fail.Count -gt 0) {
    Write-Host 'GOLDEN_LOCK_GUARDS_FAILED'
    $fail | ForEach-Object { Write-Host (' - ' + $_) }
    exit 1
}
Write-Host 'GOLDEN_LOCK_GUARDS_OK'
Write-Host 'Recursive store lock, no locking getter on a locked path, wrapped start-up load, and an SKSE callback that cannot throw out.'
