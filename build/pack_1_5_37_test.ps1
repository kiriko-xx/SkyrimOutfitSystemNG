# Test package for 1.5.37: one DLL, MO2 can install the zip as it is.
# Deliberately not a release: no docs, no ini, no esp, no scripts. Drop it into MO2,
# let it overwrite OutfitSystemNG.dll, and roll back by re-installing the previous zip.
param(
    [string]$Source = (Join-Path $PSScriptRoot '../dist/SKSE/Plugins/OutfitSystemNG.dll'),
    [string]$OutDir = (Split-Path $PSScriptRoot -Parent),
    [string]$Version = '1.5.37'
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
    "Roll back: re-install the previous 1.5.34 zip (or disable this mod).",
    "",
    "OutfitSystemNG.dll  $($dll.Length) bytes",
    "sha256             $dllHash",
    "",
    "What changed in 1.5.37: the poll no longer waits for a pass. 1.5.36 queued the",
    "bake and latched the queue; on the reporting machine the task was never run while",
    "a slider was held, so one stuck task swallowed the whole drag (49 detected",
    "changes, 3 bakes). The clones the last full bake found are now held by reference",
    "and re-morphed straight from the poll, on whatever thread the poll runs on.",
    "",
    "What to look for in SKSE/Plugins/SkyrimOutfitSystemNG.log (log file name unchanged):",
    "  MorphRefresh: re-morphed 00000014 on N cached clone node(s) on thread ...; no pass was needed.",
    "  MorphRefresh: the body morphs of 00000014 changed; following them.",
    "  MorphRefresh pipeline (session totals): ... player-changes=... cache-replays=...",
    "",
    "cache-replays climbing while you drag is this build working. If it stays 0 while",
    "player-changes climbs, the clone list was empty - send the log.",
    "",
    "Dragging a body morph slider (Screen Archer Menu or RaceMenu) must now move the",
    "illusion while the slider moves, with no menu opened and no Tab/Z press.",
    "",
    "Known boundary: the poll only follows what the morph provider reports. If a",
    "menu moves the vanilla body without writing the RaceMenu morph values, the",
    "illusion cannot see it."
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
