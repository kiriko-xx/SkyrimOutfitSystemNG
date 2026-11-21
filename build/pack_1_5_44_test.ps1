$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
# Test package for MO2: only the DLL plus a short note. Install it in MO2 above the
# 1.5.39 package (or overwrite that DLL); the game picks the new DLL up on restart.
$root = 'D:\Skyrim-dev\src\SkyrimOutfitSystemSE-NG'
$src = Join-Path $root 'dist\SKSE\Plugins\OutfitSystemNG.dll'
if (-not (Test-Path $src)) { throw "missing DLL: $src" }
$dst = Join-Path $root 'SkyrimOutfitSystemNG-1.5.44-test.zip'
if (Test-Path $dst) { throw "Refusing to overwrite existing test package: $dst" }
$noteLines = @(
  'Skyrim Outfit System NG 1.5.44 - test build (2026-11-21)',
  '',
  'Fixes the one thing that was still broken on 1.5.43: the plugin group rows in',
  'the golden items list could not be clicked at all.',
  '',
  'Why they could not be clicked:',
  '  the row pushed an ImGui id made from the plugin name pointer',
  '  (PushID(group.plugin.c_str())), while Golden::Groups() hands out a fresh',
  '  copy of the list every frame - so the pointer, and with it the row id,',
  '  differed between the frame the mouse went down and the frame it came up.',
  '  ImGui then drops the click. The plugin picker on the same page used a',
  '  stable integer id, which is why that one worked, and the button that adds',
  '  every piece of a plugin sets the selection from code, which is why a group',
  '  did get selected after using it.',
  '',
  'What changed:',
  '  - the group rows now push the row number as id, stable across frames;',
  '  - the selection log is now INFO and carries the row position:',
  '      Menu: golden group <name> selected (2 of 5).',
  '    so an ordinary log proves whether the click reached the code;',
  '  - everything from 1.5.43 is kept: explicit row hit box, clamped pane',
  '    widths, dirty flag with Export-only saving and the [not saved] hint,',
  '    and the light (ESL) plugin id width.',
  '',
  'This archive contains only SKSE\Plugins\OutfitSystemNG.dll. Install it in MO2',
  'above the package you are currently using and restart.',
  'The log file name is still SkyrimOutfitSystemNG.log.',
  '',
  'How to test: open the menu, go to Golden items, click a plugin group on the',
  'left. It must select on the first click and the editor on the right must',
  'switch to that group. Click a few different groups back and forth, and try the',
  'right half of a row as well as the empty space after the name. Then re-check',
  '1.5.43: add or remove one piece without pressing Export - the page must show',
  '[not saved] and the file must not change; a light (ESL) plugin must show real',
  'names and [slot 30/32/...] instead of <not in this load order>.',
  '',
  'Look for these lines in the log:',
  '  Menu: golden group <name> selected (N of M).   (one per click, info level)',
  '  Golden items: N entr(ies) resolved, 0 missing. (expected)',
  '  Golden items: ... was not found in this load order. (should be absent)',
  '',
  'Send back SkyrimOutfitSystemNG.log if a row still refuses to select.')
$notes = ($noteLines -join "`n") + "`n"
$zip = [IO.Compression.ZipFile]::Open($dst, 'Create')
try {
  $entry = $zip.CreateEntry('SKSE/Plugins/OutfitSystemNG.dll', [IO.Compression.CompressionLevel]::Optimal)
  $s = $entry.Open(); $f = [IO.File]::OpenRead($src)
  try { $f.CopyTo($s) } finally { $f.Dispose(); $s.Dispose() }
  $entry2 = $zip.CreateEntry('1.5.44-test-notes.txt', [IO.Compression.CompressionLevel]::Optimal)
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
