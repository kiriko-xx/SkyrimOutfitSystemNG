$ErrorActionPreference = 'Stop'
# Static guard for 1.5.91: identity of a hooked function is a 33 byte fingerprint, not the
# stock MSVC prologue (48 89 5C 24 10 ... occurs 1176 times in SE 1.5.97 and 1164 times in
# AE 1.6.1170, so it identifies nothing), the address library is a candidate rather than an
# answer (a second library for 1.6.1170 puts the same function elsewhere), and the illusion
# degrades by half instead of switching off as a whole.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$hooks   = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/hooking/Hooks.cpp')
$support = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/hooking/HookSupport.cpp')
$header  = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/include/hooking/HookSupport.h')
$patches = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/hooking/Patches.cpp')
$patchesH = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/include/hooking/Patches.hpp')
$version = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/include/version.h')
$fails = 0
function Assert-Contains([string]$text, [string]$needle, [string]$what) {
    if (-not $text.Contains($needle)) { Write-Host "FAIL $what (missing: $needle)"; $script:fails++ }
}
function Assert-Absent([string]$text, [string]$needle, [string]$what) {
    if ($text.Contains($needle)) { Write-Host "FAIL $what (still there: $needle)"; $script:fails++ }
}
function Fingerprint-Bytes([string]$text, [string]$name) {
    $m = [regex]::Match($text, [regex]::Escape($name) + '\[\] = \{(?<body>[^}]*)\}')
    if (-not $m.Success) { return @() }
    return ([regex]::Matches($m.Groups['body'].Value, '0x[0-9A-Fa-f]{2}') | ForEach-Object { $_.Value })
}

# 1. the fingerprints are the whole opening of the function, and the visitor's lea is masked
$armour = Fingerprint-Bytes $hooks 'kArmourPassFingerprint'
if ($armour.Count -ne 33) { Write-Host "FAIL the armour pass fingerprint should be 33 bytes, found $($armour.Count)"; $fails++ }
$expected = '48895C241048896C241857415641574883EC2033DB418BE94D8BF04C8BFA488BF9'
if ((($armour | ForEach-Object { $_.Substring(2) }) -join '').ToUpper() -ne $expected) { Write-Host 'FAIL the armour pass fingerprint is not the one measured on SE 1.5.97 and AE 1.6.1170'; $fails++ }
$visitor = Fingerprint-Bytes $hooks 'kVisitorFingerprint'
if ($visitor.Count -ne 33) { Write-Host "FAIL the worn visitor fingerprint should be 33 bytes, found $($visitor.Count)"; $fails++ }
$mask = Fingerprint-Bytes $hooks 'kVisitorMask'
if ($mask.Count -ne 33) { Write-Host "FAIL the visitor mask should have one byte per fingerprint byte, found $($mask.Count)"; $fails++ }
$zeros = ($mask | Where-Object { $_ -eq '0x00' }).Count
if ($zeros -ne 4) { Write-Host "FAIL exactly the rel32 of the lea (4 bytes) has to be masked, found $zeros masked"; $fails++ }
$maskedAt = -1
for ($i = 0; $i -lt $mask.Count; $i++) { if ($mask[$i] -eq '0x00') { $maskedAt = $i; break } }
if ($maskedAt -ne 25) { Write-Host "FAIL the masked bytes should be 25..28 (the lea displacement), found them at $maskedAt"; $fails++ }
# the eight byte prologue is a filter at most: it must not be used on its own any more
Assert-Absent $hooks 'kArmourPassPrologue' 'the 8 byte prologue is no longer the identity of the armour pass'

# 2. the support layer
Assert-Contains $header 'bool PrologueMatches(' 'the masked fingerprint compare is declared'
Assert-Contains $header 'bool EntryLooksBranched(' 'the branch test is declared on its own'
Assert-Contains $header 'inline constexpr std::size_t kEntrySearchWindow = 0x800;' 'the search window is one constant'
Assert-Contains $header 'std::uintptr_t LocateEntry(' 'the entry locator is declared'
Assert-Absent $header 'EntryMatches' 'the old entry check is gone'
Assert-Absent $support 'EntryMatches' 'the old entry check is gone from the implementation too'
Assert-Contains $support 'MaskedEqual' 'the compare skips the masked bytes'
Assert-Contains $support 'const auto mask = a_mask ? a_mask[i] : static_cast<std::uint8_t>(0xFF);' 'a null mask means whole bytes'
Assert-Contains $support 'const auto begin = std::max(regionStart, a_declared - a_window);' 'the search stays inside the committed region'
Assert-Contains $support 'if (hits == 1) {' 'a single hit is accepted'
Assert-Contains $support 'if (hits > 1) {' 'two hits are not'
Assert-Contains $support 'hold the same fingerprint' 'an ambiguous search is named'
Assert-Contains $support 'the fingerprint of the function this hook needs is at' 'the found address is reported with its distance'
Assert-Contains $support 'holds neither the fingerprint this hook was written for nor a branch' 'a miss is reported with both possible causes'
Assert-Contains $support 'not guaranteed to keep the game function' 'a branch another plugin wrote is refused and explained'

# 3. both entry hooks go through it
Assert-Contains $hooks 'const auto entry = LocateEntry("DontVanillaSkinPlayer", declared, kArmourPassFingerprint, nullptr, sizeof(kArmourPassFingerprint));' 'the armour pass is located by fingerprint'
Assert-Contains $hooks 'if (!EntryLooksBranched(declared)) {' 'the worn visitor keeps the chain when a branch is already there'
Assert-Contains $hooks 'LocateEntry("CustomSkinPlayer", declared, kVisitorFingerprint, kVisitorMask, sizeof(kVisitorFingerprint))' 'the worn visitor is located by fingerprint'

# 4. the illusion degrades by half
Assert-Contains $patchesH 'enum class IllusionMode : std::uint8_t { off = 0, attachOnly = 1, full = 2 };' 'the three states are declared'
Assert-Contains $patchesH 'void SetIllusionMode(IllusionMode a_mode);' 'the mode can be set'
Assert-Contains $patchesH 'IllusionMode IllusionState();' 'and read'
Assert-Contains $patches 'if (IllusionState() != IllusionMode::full) return false;' 'hiding is only allowed when the attach half is in place'
Assert-Contains $patches 'SetIllusionMode(a_enabled ? IllusionMode::full : IllusionMode::off);' 'the boolean switch stays for the health check'
Assert-Absent $patches 'g_illusionDisplayEnabled' 'the single boolean is gone'
Assert-Contains $hooks 'SetIllusionMode(IllusionMode::full);' 'both halves mean full'
Assert-Contains $hooks 'SetIllusionMode(IllusionMode::attachOnly);' 'the attach half alone keeps the illusion visible'
Assert-Contains $hooks 'SetIllusionMode(IllusionMode::off);' 'no attach half means off'
Assert-Contains $hooks 'the display set is attached inside the skinning pass, but the armour pass hook is missing' 'the limited session says what is missing'
Assert-Absent $hooks 'SetIllusionDisplayEnabled(illusionAvailable)' 'the all-or-nothing gate is gone'

# 5. version
if ($version -notmatch 'VERSION_PATCH 91') { Write-Host 'FAIL version.h is not at 1.5.91'; $fails++ }

if ($fails -gt 0) { Write-Host "ENTRY_IDENTITY_FAILED $fails"; exit 1 }
Write-Host 'ENTRY_IDENTITY_OK: 33 byte fingerprints (lea masked), the library is a candidate with a 0x800 window, and the illusion degrades by half instead of switching off'
