$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
# Test package for MO2: only the DLL plus a short note. Install it in MO2 above the
# 1.5.39 package (or overwrite that DLL); the game picks the new DLL up on restart.
$root = 'D:\Skyrim-dev\src\SkyrimOutfitSystemSE-NG'
$src = Join-Path $root 'dist\SKSE\Plugins\OutfitSystemNG.dll'
if (-not (Test-Path $src)) { throw "missing DLL: $src" }
$dst = Join-Path $root 'SkyrimOutfitSystemNG-1.5.45-test.zip'
if (Test-Path $dst) { throw "Refusing to overwrite existing test package: $dst" }
$noteLines = @(
  'Skyrim Outfit System NG 1.5.45 - test build (2026-11-21)',
  '',
  'Two changes, both in the menu only. The skinning, morph and scene code is',
  'byte for byte the same as 1.5.44.',
  '',
  '1. Golden items: "add one piece by FormID" is gone. Typing a local form id',
  '   (000800) was not usable - nobody knows those ids. Now:',
  '     - press "Add one piece from a plugin..." -> a plugin list appears;',
  '     - pick a plugin -> the page lists every slot armour of that plugin,',
  '       each row with its name, form id and [slot 30/32/...], already added',
  '       rows marked [in the list];',
  '     - click a row to add that single piece to that plugin group.',
  '   Selecting a golden group on the left now shows the same list at the',
  '   bottom of the editor, limited to the plugin of that group.',
  '   Nothing is written to disk until you press Export (still 1.5.43 behaviour).',
  '',
  '2. Outfits from a plugin: one plugin often holds several costumes. The',
  '   generator now groups the slot armour by the leading words of the name',
  '   (a prefix shared by at least two pieces, at most four words), splits each',
  '   costume into as many outfits as its slots need, and names them',
  '   "<plugin> <costume>". A plugin with a single costume keeps exactly the old',
  '   names (<plugin>, <plugin>_1, ...). Every generated outfit still needs the',
  '   slot equipped to show the illusion (XXXO).',
  '',
  'How to test:',
  '  - golden items: open the piece list from the button and from a group, click',
  '    a piece; then export and restart to see whether it survived.',
  '  - outfits: pick a plugin with several costumes, generate, and check that the',
  '    log line says "in K name group(s)" where K is the number of costumes you',
  '    can count, and that each outfit holds the pieces of one costume only.',
  '  - control: pick a plugin with a single costume and compare the names with',
  '    what the previous build generated - they must match.',
  '',
  'Log lines to look for:',
  '  Menu: golden piece <plugin>:<id> <name>                          (info)',
  '  ESP outfits: N outfit(s) from M piece(s) in K name group(s); ... (info)',
  '  [not saved] The changes live in memory only; press Export ...    (expected)',
  '',
  'This archive contains only SKSE\Plugins\OutfitSystemNG.dll. Install it in MO2',
  'above the package you are currently using and restart.',
  'The log file name is still SkyrimOutfitSystemNG.log.',
  '',
  'Send back SkyrimOutfitSystemNG.log if the piece list does not open or a',
  'generated outfit mixes pieces of two different costumes.')
$notes = ($noteLines -join "`n") + "`n"
$zip = [IO.Compression.ZipFile]::Open($dst, 'Create')
try {
  $entry = $zip.CreateEntry('SKSE/Plugins/OutfitSystemNG.dll', [IO.Compression.CompressionLevel]::Optimal)
  $s = $entry.Open(); $f = [IO.File]::OpenRead($src)
  try { $f.CopyTo($s) } finally { $f.Dispose(); $s.Dispose() }
  $entry2 = $zip.CreateEntry('1.5.45-test-notes.txt', [IO.Compression.CompressionLevel]::Optimal)
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
