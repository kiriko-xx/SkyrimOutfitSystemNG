$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
# Test package for MO2: only the DLL plus a short note. Install it in MO2 above the
# 1.5.39 package (or overwrite that DLL); the game picks the new DLL up on restart.
$root = 'D:\Skyrim-dev\src\SkyrimOutfitSystemSE-NG'
$src = Join-Path $root 'dist\SKSE\Plugins\OutfitSystemNG.dll'
if (-not (Test-Path $src)) { throw "missing DLL: $src" }
$dst = Join-Path $root 'SkyrimOutfitSystemNG-1.5.46-test.zip'
if (Test-Path $dst) { throw "Refusing to overwrite existing test package: $dst" }
$noteLines = @(
  'Skyrim Outfit System NG 1.5.46 - test build (2026-11-21)',
  '',
  'One change, in the outfit generator only. The skinning, morph, scene and',
  'golden items code is byte for byte the same as 1.5.45.',
  '',
  'Generating outfits from a plugin now uses a character level prefix tree, a',
  'longest common prefix and slot constraints instead of "the first four words":',
  '',
  '  - no separators are assumed. "StarDress_TopA", "StarDress TopA" and',
  '    "StarDressTopA" are all read the same way; the original spelling is only',
  '    used for the outfit name. Tags in [] and () are ignored for comparison.',
  '  - one costume is split into several only when every branch under a prefix',
  '    already forms a costume of its own (two pieces or more over two slots or',
  '    more). "StarDress" and "StarNightDress" become two costumes; "Sailor',
  '    hair / top / skirt / shoes" stays one.',
  '  - inside a costume the pieces are bucketed by slot. Slots with several',
  '    pieces lose their shared part and keep a differentiator (A / B / red /',
  '    black); slots with one piece are shared by every combination.',
  '  - combinations are built from identical differentiators across slots. When',
  '    they cannot be matched (top A/B but skirt red/black) no preset is written',
  '    for that costume at all - the log says how many were unmatched. Two pieces',
  '    on one slot drops that combination; the same name on one slot keeps the',
  '    first piece.',
  '  - names and limits are unchanged: <plugin> <costume> when the plugin holds',
  '    several, <plugin> / <plugin>_1 when it holds one; 96 outfits, 32',
  '    variants, every outfit still needs its slots equipped (XXXO).',
  '',
  'How to test:',
  '  - pick a plugin that holds two costumes whose names differ by one or two',
  '    characters. Two outfits must appear, not one.',
  '  - pick a plugin whose costume has two tops and two skirts but one hair and',
  '    one pair of shoes. Two outfits, the hair and shoes shared.',
  '  - pick a plugin with a single costume. The names must match the previous',
  '    build exactly.',
  '  - control for a costume with only single piece slots: one outfit per',
  '    costume, nothing mixed.',
  '',
  'Log lines to look for:',
  '  ESP outfits: N outfit(s) from M piece(s) in K costume(s); C created ... (info)',
  '  ESP outfits: costume <x> holds N piece(s) over M slot(s).            (debug)',
  '  ESP outfits: the alternatives of costume <x> cannot be matched ...   (warn)',
  '',
  'This archive contains only SKSE\\Plugins\\OutfitSystemNG.dll. Install it in MO2',
  'above the package you are currently using and restart. The log file name is',
  'still SkyrimOutfitSystemNG.log.',
  '',
  'Send back SkyrimOutfitSystemNG.log with the plugin name you used if the number',
  'of costumes (K) does not match what you can count, or if an outfit mixes',
  'pieces of two different costumes.')
$notes = ($noteLines -join "`n") + "`n"
$zip = [IO.Compression.ZipFile]::Open($dst, 'Create')
try {
  $entry = $zip.CreateEntry('SKSE/Plugins/OutfitSystemNG.dll', [IO.Compression.CompressionLevel]::Optimal)
  $s = $entry.Open(); $f = [IO.File]::OpenRead($src)
  try { $f.CopyTo($s) } finally { $f.Dispose(); $s.Dispose() }
  $entry2 = $zip.CreateEntry('1.5.46-test-notes.txt', [IO.Compression.CompressionLevel]::Optimal)
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
