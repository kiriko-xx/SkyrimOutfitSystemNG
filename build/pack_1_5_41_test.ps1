$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
# Test package for MO2: only the DLL plus a short note. Install it in MO2 above the
# 1.5.39 package (or overwrite that DLL); the game picks the new DLL up on restart.
$root = 'D:\Skyrim-dev\src\SkyrimOutfitSystemSE-NG'
$src = Join-Path $root 'dist\SKSE\Plugins\OutfitSystemNG.dll'
if (-not (Test-Path $src)) { throw "missing DLL: $src" }
$dst = Join-Path $root 'SkyrimOutfitSystemNG-1.5.41-test.zip'
if (Test-Path $dst) { throw "Refusing to overwrite existing test package: $dst" }
$noteLines = @(
  'Skyrim Outfit System NG 1.5.41 - test build (2026-11-21)',
  '',
  'Fixes the start-up crash reported on 1.5.41:',
  '  the golden items store was locked with a plain std::mutex, and the import',
  '  path locked it twice on the same thread. MSVC reports that as',
  '  std::system_error("resource deadlock would occur"); nothing caught it in',
  '  the SKSE message handler, so the game closed on every start.',
  '',
  'What changed: the store lock is recursive, locked code paths use',
  'TotalCountLocked(), and both the start-up load and the message handler now',
  'catch anything they throw. The engine side is untouched.',
  '',
  'This archive contains only SKSE\Plugins\OutfitSystemNG.dll. Install it in MO2',
  'above the 1.5.41 package (or overwrite that DLL) and restart the game.',
  'The log file name is still SkyrimOutfitSystemNG.log.',
  '',
  'Look for these lines in the log:',
  '  Golden items: N entr(ies) loaded from ...            (normal)',
  '  Golden items: the store could not be loaded (..) ...  (fallback, send it back)',
  '  Message handler: an exception escaped ...             (should be absent)',
  '',
  'Send back the Golden items line, and any crash-*.log if the game still closes.'
)
$notes = ($noteLines -join "`n") + "`n"
$zip = [IO.Compression.ZipFile]::Open($dst, 'Create')
try {
  $entry = $zip.CreateEntry('SKSE/Plugins/OutfitSystemNG.dll', [IO.Compression.CompressionLevel]::Optimal)
  $s = $entry.Open(); $f = [IO.File]::OpenRead($src)
  try { $f.CopyTo($s) } finally { $f.Dispose(); $s.Dispose() }
  $entry2 = $zip.CreateEntry('1.5.41-test-notes.txt', [IO.Compression.CompressionLevel]::Optimal)
  $s2 = $entry2.Open()
  $bytes = [Text.UTF8Encoding]::new($false).GetBytes($notes)
  try { $s2.Write($bytes, 0, $bytes.Length) } finally { $s2.Dispose() }
} finally { $zip.Dispose() }
$read = [IO.Compression.ZipFile]::OpenRead($dst)
$count = 0
try {
  foreach ($e in $read.Entries) {
    if ($e.FullName.Contains('\')) { throw 'Backslash entry' }
    $count++
  }
} finally { $read.Dispose() }
if ($count -ne 2) { throw "expected 2 entries, got $count" }
$read = [IO.Compression.ZipFile]::OpenRead($dst)
try {
  $e = $read.GetEntry('SKSE/Plugins/OutfitSystemNG.dll')
  $s = $e.Open(); $h = [Security.Cryptography.SHA256]::Create()
  try { $actual = [Convert]::ToHexString($h.ComputeHash($s)).ToLower() } finally { $s.Dispose(); $h.Dispose() }
} finally { $read.Dispose() }
$expected = (Get-FileHash $src -Algorithm SHA256).Hash.ToLower()
if ($actual -ne $expected) { throw 'DLL entry hash mismatch' }
$hash = (Get-FileHash $dst -Algorithm SHA256).Hash.ToLower()
[IO.File]::WriteAllText($dst + '.sha256', "$hash  $([IO.Path]::GetFileName($dst))`r`n", [Text.UTF8Encoding]::new($false))
[pscustomobject]@{TestZipBytes=(Get-Item $dst).Length; TestZipSHA256=$hash; DllBytes=(Get-Item $src).Length; DllSHA256=$expected; Entries=$count} | Format-List
