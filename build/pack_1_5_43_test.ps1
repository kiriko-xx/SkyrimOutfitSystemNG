$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
# Test package for MO2: only the DLL plus a short note. Install it in MO2 above the
# 1.5.39 package (or overwrite that DLL); the game picks the new DLL up on restart.
$root = 'D:\Skyrim-dev\src\SkyrimOutfitSystemSE-NG'
$src = Join-Path $root 'dist\SKSE\Plugins\OutfitSystemNG.dll'
if (-not (Test-Path $src)) { throw "missing DLL: $src" }
$dst = Join-Path $root 'SkyrimOutfitSystemNG-1.5.43-test.zip'
if (Test-Path $dst) { throw "Refusing to overwrite existing test package: $dst" }
$noteLines = @(
  'Skyrim Outfit System NG 1.5.43 - test build (2026-11-21)',
  '',
  'Fixes three defects on the golden items page reported on 1.5.42:',
  '  1. the plugin group rows in the left list were almost impossible to click;',
  '  2. every edit wrote OutfitSystemGoldenItems.json on the spot;',
  '  3. pieces of a light (ESL) plugin showed as <not in this load order>.',
  '',
  'What changed:',
  '  - group rows and the plugin picker now use an explicit row size, and the two',
  '    columns are clamped so the editor cannot cover the list;',
  '  - the editor only marks itself dirty; the Export button is the only thing',
  '    that writes the file, and the page shows [not saved] until you press it;',
  '  - golden item ids are stored with the width the plugin needs (12 bits for',
  '    light files, 24 bits otherwise) and resolved through PluginScan::ResolveForm,',
  '    so the 61 entries of your current file are repaired on load.',
  '',
  'This archive contains only SKSE\Plugins\OutfitSystemNG.dll. Install it in MO2',
  'above the package you are currently using (1.5.42 or older) and restart.',
  'The log file name is still SkyrimOutfitSystemNG.log.',
  '',
  'How to test: open the menu, go to the golden items page and click the plugin',
  'groups on the left - they must select on the first click. Add or remove a',
  'piece without pressing Export: the page must show [not saved] and the file',
  'must not change; restart and confirm the change was not saved. Then press',
  'Export and restart: the change must still be there. Finally add every piece',
  'of a light plugin (for example [SSE]Mini Bunny.esp): the right side must show',
  'real names and [slot 30/32/...] instead of <not in this load order>.',
  '',
  'Look for these lines in the log:',
  '  Menu: golden group <name> selected.                  (verbose; one per click)',
  '  Golden items: N entr(ies) resolved, 0 missing.       (expected after 1.5.43)',
  '  Golden items: ... was not found in this load order.  (should be absent)',
  '',
  'Send back SkyrimOutfitSystemNG.log and any crash-*.log if it still misbehaves.')
$notes = ($noteLines -join "`n") + "`n"
$zip = [IO.Compression.ZipFile]::Open($dst, 'Create')
try {
  $entry = $zip.CreateEntry('SKSE/Plugins/OutfitSystemNG.dll', [IO.Compression.CompressionLevel]::Optimal)
  $s = $entry.Open(); $f = [IO.File]::OpenRead($src)
  try { $f.CopyTo($s) } finally { $f.Dispose(); $s.Dispose() }
  $entry2 = $zip.CreateEntry('1.5.43-test-notes.txt', [IO.Compression.CompressionLevel]::Optimal)
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
