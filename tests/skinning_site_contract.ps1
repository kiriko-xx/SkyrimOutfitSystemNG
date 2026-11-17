# Source-contract regression only: no compiler, build, executable patching or game launch.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
$header = Get-Content (Join-Path $root 'src/cpp/include/hooking/HookSupport.h') -Raw
$support = Get-Content (Join-Path $root 'src/cpp/hooking/HookSupport.cpp') -Raw
$hooks = Get-Content (Join-Path $root 'src/cpp/hooking/Hooks.cpp') -Raw

# Extract the actual production return expressions, not independent copies of the rules.
# This deliberately supports only the small pure expression grammar used here.
function Get-ProductionPredicate([string] $name, [string] $parameters) {
    $match = [regex]::Match($header, ('constexpr bool ' + $name + '\([^)]*\)\s*\{\s*return ([^;]+);\s*\}'))
    if (-not $match.Success) { throw "Cannot extract production predicate $name" }
    $expression = $match.Groups[1].Value
    if ($expression -match '[^a-zA-Z0-9_\s&=<>+\-]') { throw "Unsupported predicate syntax: $expression" }
    $expression = [regex]::Replace($expression, '\b(a_\w+)\b', { param($m) '$' + $m.Groups[1].Value })
    $expression = $expression.Replace('&&', ' -and ').Replace('==', ' -eq ').Replace('>=', ' -ge ').Replace('<=', ' -le ')
    return [scriptblock]::Create("param($parameters) return ($expression)")
}
$contains = Get-ProductionPredicate 'ContainsRange' '$a_begin, $a_size, $a_address, $a_width'
$accepts = Get-ProductionPredicate 'IsExpectedCallSite' '$a_site, $a_nominal, $a_validRange, $a_opcode, $a_reachesExpected'
$script:checks = 0
function Check([string] $name, [bool] $actual, [bool] $expected) {
    if ($actual -ne $expected) { throw "FAIL: $name (actual $actual, expected $expected)" }
    $script:checks++
    Write-Output "PASS: $name"
}

$nominal = 0x1302
Check 'nominal E8 reaching expected succeeds' (& $accepts $nominal $nominal $true 0xE8 $true) $true
Check 'same callee at host +3A8 is NOT a substitute' (& $accepts 0x13A8 $nominal $true 0xE8 $true) $false
Check 'nominal wrong callee rejected even if +3A8 reaches expected' (& $accepts $nominal $nominal $true 0xE8 $false) $false
Check 'nominal E9 rejected even when it reaches expected' (& $accepts $nominal $nominal $true 0xE9 $true) $false
Check 'nominal NOP rejected' (& $accepts $nominal $nominal $true 0x90 $true) $false
Check 'third-party or unknown callee rejected' (& $accepts $nominal $nominal $true 0xE8 $false) $false
Check 'invalid range rejects before patching' (& $accepts $nominal $nominal $false 0xE8 $true) $false
Check 'contained five-byte site' (& $contains 0x1000 0x1000 $nominal 5) $true
Check 'site below text rejected' (& $contains 0x1000 0x1000 0xFFF 5) $false
Check 'site at end rejected' (& $contains 0x1000 0x1000 0x2000 5) $false
Check 'partial call at end rejected' (& $contains 0x1000 0x1000 0x1FFC 5) $false
Check 'empty range rejected' (& $contains 0x1000 0 $nominal 5) $false
Check 'width exceeding range rejected' (& $contains 0x1000 4 0x1000 5) $false
Check 'high address cannot wrap into text' (& $contains 0x1000 0x1000 ([uint64]::MaxValue) 5) $false

# Verify production wiring; the predicate alone is insufficient if the caller scans first.
$finder = [regex]::Match($support, '(?s)std::uintptr_t FindExpectedCallAtOffset\([^\n]+\)\s*\{(.*?)\n\t\}\s*\n\s*void LogSiteBytes').Groups[1].Value
if (-not $finder) { throw 'Strict finder not found' }
Check 'DontVanilla uses fixed host +302' ($hooks.Contains('DontVanillaSkinPlayer_Hook = FindExpectedCallAtOffset("DontVanillaSkinPlayer", DontVanillaSkinPlayer_Hook_ID, TESObjectARMO_ApplyArmorAddon, 0x302);')) $true
Check 'production uses extracted range and site predicates' ($finder.Contains('SiteContract::ContainsRange(') -and $finder.Contains('SiteContract::IsExpectedCallSite(nominal, nominal, validRange, opcode, reachesExpected)')) $true
Check 'production retains Reaches check' ($finder.Contains('Reaches(nominal, expected)')) $true
Check 'production restricts first-hop target to module text' ($finder.Contains('SiteContract::ContainsRange(begin, size, target, 5)')) $true
Check 'failure logs nominal bytes and +/-20 window' ($finder.Contains('LogSiteBytes(a_hook, nominal);') -and $finder.Contains('LogCodeWindow(a_hook, nominal, 0x20, 0x20);')) $true
$diagnostic = $finder.Substring($finder.IndexOf('const auto candidates = CallsReaching'))
Check 'scan cannot return a candidate' ($diagnostic -notmatch 'return\s+(?!0\s*;)' -and $diagnostic -match 'return 0;') $true
Check 'only nonzero finder return is nominal' ((([regex]::Matches($finder, 'return\s+(?!0\s*;)([^;]+);') | ForEach-Object { $_.Groups[1].Value }) -join ',') -eq 'nominal') $true
Write-Output "PASS: $script:checks source-contract checks. No C++ build or SE/AE runtime validation performed."
