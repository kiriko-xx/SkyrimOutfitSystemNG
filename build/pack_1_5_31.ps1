$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = 'D:\Skyrim-dev\src\SkyrimOutfitSystemSE-NG'
$src = Join-Path $root 'dist'
$dst = Join-Path $root 'SkyrimOutfitSystemNG-1.5.31.zip'
if (Test-Path $dst) { throw "Refusing to overwrite existing release: $dst" }
$files = @(Get-ChildItem $src -Recurse -File | Where-Object Name -ne 'SHA256SUMS.txt')
$rows = @($files | ForEach-Object {
 $rel = $_.FullName.Substring($src.Length + 1).Replace('\','/')
 (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower() + '  ' + $rel
} | Sort-Object)
[IO.File]::WriteAllText((Join-Path $src 'SHA256SUMS.txt'), ($rows -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
$zip = [IO.Compression.ZipFile]::Open($dst, 'Create')
try {
 Get-ChildItem $src -Recurse -File | Sort-Object FullName | ForEach-Object {
  $rel = $_.FullName.Substring($src.Length + 1).Replace('\','/')
  $entry = $zip.CreateEntry($rel, [IO.Compression.CompressionLevel]::Optimal)
  $s = $entry.Open(); $f = [IO.File]::OpenRead($_.FullName)
  try { $f.CopyTo($s) } finally { $f.Dispose(); $s.Dispose() }
 }
} finally { $zip.Dispose() }
$read = [IO.Compression.ZipFile]::OpenRead($dst)
$verified = 0
try {
 foreach ($entry in $read.Entries) {
  if ($entry.FullName.Contains('\')) { throw 'Backslash entry' }
  $s = $entry.Open(); $h = [Security.Cryptography.SHA256]::Create()
  try { $actual = [Convert]::ToHexString($h.ComputeHash($s)).ToLower() } finally { $s.Dispose(); $h.Dispose() }
  $expected = (Get-FileHash (Join-Path $src $entry.FullName) -Algorithm SHA256).Hash.ToLower()
  if ($actual -ne $expected) { throw "Entry mismatch $($entry.FullName)" }
  $verified++
 }
 if ($verified -ne @(Get-ChildItem $src -Recurse -File).Count) { throw 'Entry count mismatch' }
} finally { $read.Dispose() }
foreach ($line in $rows) {
 $hash, $rel = $line -split '  ', 2
 if ((Get-FileHash (Join-Path $src $rel)).Hash.ToLower() -ne $hash) { throw 'Manifest mismatch' }
}
$hash = (Get-FileHash $dst -Algorithm SHA256).Hash.ToLower()
[IO.File]::WriteAllText($dst + '.sha256', "$hash  $([IO.Path]::GetFileName($dst))`r`n", [Text.UTF8Encoding]::new($false))
[pscustomobject]@{ZipBytes=(Get-Item $dst).Length; ZipSHA256=$hash; EntriesVerified=$verified; ManifestVerified=$rows.Count; DistBytes=(Get-ChildItem $src -Recurse -File | Measure-Object Length -Sum).Sum} | Format-List
