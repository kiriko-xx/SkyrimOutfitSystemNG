# Test package for 1.5.38: one DLL, MO2 can install the zip as it is.
# Deliberately not a release: no docs, no ini, no esp, no scripts. Drop it into MO2,
# let it overwrite OutfitSystemNG.dll, and roll back by re-installing the previous zip.
param(
    [string]$Source = (Join-Path $PSScriptRoot '../dist/SKSE/Plugins/OutfitSystemNG.dll'),
    [string]$OutDir = (Split-Path $PSScriptRoot -Parent),
    [string]$Version = '1.5.38'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$Source = (Resolve-Path $Source).Path
$OutDir = (Resolve-Path $OutDir).Path
$zipPath = Join-Path $OutDir ("SkyrimOutfitSystemNG-{0}-test.zip" -f $Version)
if (Test-Path $zipPath) { throw "refusing to overwrite $zipPath" }

$dll = [IO.File]::ReadAllBytes($Source)
$dllHash = (Get-FileHash -Path $Source -Algorithm SHA256).Hash.ToLowerInvariant()

$notes = @(
    "Skyrim Outfit System NG - $Version test build",
    "",
    "What this is: a single-DLL test build for the body morph refresh. It is not a",
    "release build: no ini, no esp, no scripts, no docs.",
    "",
    "Install (MO2): install this zip as a normal mod and let it overwrite",
    "SKSE/Plugins/OutfitSystemNG.dll. The old SkyrimOutfitSystemNG.dll must not be",
    "left behind - two module names in Data load twice.",
    "",
    "Roll back: re-install the previous 1.5.37 test zip.",
    "",
    "OutfitSystemNG.dll  $($dll.Length) bytes",
    "sha256             $dllHash",
    "",
    "What changed in 1.5.38: repeated OBody preset switches crashed the game again.",
    "The same fix was made in 7f5a2ce (preserve existing BODYTRI metadata) and was",
    "lost when 1.5.33 restored src from the 1.5.29 tag, so it is back:",
    "",
    "  1. A live clone never has its BODYTRI taken away or rewritten. The target is",
    "     only attached to a node that carries none - RaceMenu, OBody or the engine",
    "     may be reading that string while we look at it.",
    "  2. The cached replay now checks the node it is about to morph is still the one",
    "     the engine draws in that slot. A node left over from an earlier outfit is a",
    "     detached subtree; skee would walk objects nobody owns any more.",
    "  3. OBody announces its own morph pass (Obody_ApplyMorph). That opens a 350 ms",
    "     window in which our replay stays off those clones, so the two passes never",
    "     walk one subtree at the same time.",
    "  4. Two replays of the same actor are at least 20 ms apart.",
    "",
    "The live follow of 1.5.37 is unchanged: dragging a body morph slider still moves",
    "the illusion while the slider moves, no menu, no Tab/Z.",
    "",
    "What to test (the case that crashed): drag a morph slider in RaceMenu, close the",
    "menu, open Screen Archer Menu, its body morph / OBody page, go back, then switch",
    "OBody presets repeatedly - 16 or more. It must not crash.",
    "",
    "What to look for in SKSE/Plugins/SkyrimOutfitSystemNG.log (log file name unchanged):",
    "  MorphRefresh: re-morphed 00000014 on N cached clone node(s) on thread ...; no pass was needed.",
    "  MorphRefresh: skipped N cached clone(s) of 00000014 the engine no longer draws; ...",
    "  MorphRefresh pipeline (session totals): ... player-changes=... cache-replays=...",
    "",
    "cache-replays climbing while you drag = the live follow is working. Skipped-clone",
    "lines are the new guard doing its job; a steady stream of them means the engine",
    "is rebuilding that actor often, which is expected during preset switches.",
    "",
    "Known boundary: the replay still runs on the thread that polls, because that is",
    "the only thread that runs while a slider is held. If the crash survives this",
    "build, the next step is to give up the live follow and move the replay to the",
    "game thread only."
) -join "`r`n"

$notesBytes = [Text.Encoding]::UTF8.GetBytes($notes)

$zip = [IO.Compression.ZipFile]::Open($zipPath, [IO.Compression.ZipArchiveMode]::Create)
try {
    $entries = @(
        @{ Name = 'SKSE/Plugins/OutfitSystemNG.dll'; Bytes = $dll },
        @{ Name = ("{0}-test-notes.txt" -f $Version); Bytes = $notesBytes }
    )
    foreach ($e in $entries) {
        $entry = $zip.CreateEntry($e.Name, [IO.Compression.CompressionLevel]::Optimal)
        $stream = $entry.Open()
        try { $stream.Write($e.Bytes, 0, $e.Bytes.Length) } finally { $stream.Dispose() }
    }
} finally { $zip.Dispose() }

$check = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $bad = 0
    foreach ($entry in $check.Entries) {
        if ($entry.FullName.Contains('\')) { ++$bad; Write-Output ("backslash entry: " + $entry.FullName) }
    }
    foreach ($entry in $check.Entries) {
        $stream = $entry.Open()
        $ms = New-Object System.IO.MemoryStream
        try { $stream.CopyTo($ms) } finally { $stream.Dispose() }
        $bytes = $ms.ToArray()
        $ms.Dispose()
        $hash = [BitConverter]::ToString(
            [Security.Cryptography.SHA256]::Create().ComputeHash($bytes)).Replace('-', '').ToLowerInvariant()
        Write-Output ("entry {0} {1} bytes {2}" -f $entry.FullName, $bytes.Length, $hash)
        if ($entry.FullName -like '*OutfitSystemNG.dll' -and $hash -ne $dllHash) { ++$bad }
    }
    $count = $check.Entries.Count
} finally { $check.Dispose() }

$zipBytes = (Get-Item $zipPath).Length
$zipHash = (Get-FileHash -Path $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -Path ($zipPath + '.sha256') -Value ("{0}  {1}" -f $zipHash, (Split-Path $zipPath -Leaf)) -Encoding ascii -NoNewline
Add-Content -Path ($zipPath + '.sha256') -Value '' -Encoding ascii

if ($bad -ne 0) { throw "package verification failed: $bad" }
Write-Output "TEST_PACKAGE_OK"
Write-Output ("zip      : {0}" -f $zipPath)
Write-Output ("bytes    : {0}" -f $zipBytes)
Write-Output ("sha256   : {0}" -f $zipHash)
Write-Output ("entries  : {0}" -f $count)
Write-Output ("dll      : {0} bytes {1}" -f $dll.Length, $dllHash)
