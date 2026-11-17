param(
    [string] $LogPath = (Join-Path $PSScriptRoot '../../../bug/skse64.log'),
    [switch] $ModelOnly
)
$ErrorActionPreference = 'Stop'

# Replay SKSE v2.0.20 PluginManager.cpp:488-563, including its
# duplicate-listener continue (which bypasses idx++). 'of N' is numPlugins,
# NOT vector size. This test never loads a DLL or changes the supplied log.
$script:Buckets = [System.Collections.Generic.List[object]]::new()
function Register-Replayed([int] $Listener, [string] $Sender, [int] $NumPlugins, [hashtable] $Names) {
    if ($script:Buckets.Count -lt $NumPlugins) {
        while ($script:Buckets.Count -lt ($NumPlugins + 5)) {
            $script:Buckets.Add([System.Collections.Generic.List[int]]::new())
        }
    }
    if ($Sender -eq '(null)') {
        $idx = 0
        foreach ($bucket in $script:Buckets) {
            if ($idx -ne 0 -and $idx -ne $Listener) {
                if ($bucket.Contains($Listener)) { continue }
                $bucket.Add($Listener)
            }
            $idx++
        }
    } else {
        if (-not $Names.ContainsKey($Sender)) { throw "Unknown sender name: $Sender" }
        $target = [int] $Names[$Sender]
        if (-not $script:Buckets[$target].Contains($Listener)) {
            $script:Buckets[$target].Add($Listener)
        }
    }
}
function Assert-That([bool] $Condition, [string] $Message) {
    if (-not $Condition) { throw "ASSERT FAILED: $Message" }
}
if (-not $ModelOnly) {
$lines = Get-Content -LiteralPath $LogPath
$names = @{ SKSE = 0 }
foreach ($line in $lines) {
    if ($line -match '\([0-9A-Fa-f]{8} (.+) [0-9A-Fa-f]{8}\) loaded correctly \(handle (\d+)\)') {
        $names[$Matches[1]] = [int] $Matches[2]
    }
}
Assert-That ($names['SkyrimOutfitSystemNG'] -eq 139) 'Expected current capture outfit handle139'
Assert-That ($names['skee'] -eq 132) 'Expected current capture skee handle132'
Assert-That ($names['OBody'] -eq 95) 'Expected current capture OBody handle95'
$registrationCount = 0
$lineNumber = 0
foreach ($line in $lines) {
    $lineNumber++
    if ($line -match '^registering plugin listener for (.*?) at (\d+) of (\d+)$') {
        $senderName = $Matches[1]
        $listener = [int] $Matches[2]
        $numPlugins = [int] $Matches[3]
        $before = $script:Buckets.Count
        Register-Replayed $listener $senderName $numPlugins $names
        $registrationCount++
        if (($listener -ge 130 -and $listener -le 139 -and $lineNumber -lt 500) -or
            ($senderName -eq '(null)' -and $listener -eq 95)) {
            Write-Output "line $lineNumber listener=$listener sender=$senderName numPlugins=$numPlugins vectorSize=$before->$($script:Buckets.Count)"
        }
        if ($senderName -eq '(null)' -and $listener -eq 132) {
            Assert-That ($script:Buckets.Count -eq 133) 'skee early registration vector size133'
        }
    }
}
$actual = $script:Buckets[139] -join ','
$expected = '24,35,39,45,66,91,95,140'
Assert-That ($actual -eq $expected) "sender139 ordered bucket expected $expected; got $actual"
Assert-That (-not $script:Buckets[139].Contains(132)) 'skee132 absent from sender139 bucket'
Assert-That ($script:Buckets[95].Contains(132)) 'skee132 present in OBody95 sender bucket'
Assert-That ($script:Buckets[139].Contains(95)) 'OBody95 present in outfit139 sender bucket'
Write-Output "PASS capture: $registrationCount registrations; sender139=[$actual]; 139->skee132 impossible; 95->skee132 and 139->OBody95 registered."

}

# Hypothetical ACTUAL load order, not a filename sorting guarantee:
# our real handle H=95 loads before skee S=132. The latter's wildcard
# registration includes H's already-existing bucket. No forged sender.
$script:Buckets = [System.Collections.Generic.List[object]]::new()
$hypothetical = @{ SKSE = 0; SkyrimOutfitSystemNG = 95; skee = 132 }
Register-Replayed 95 'SKSE' 96 $hypothetical
Register-Replayed 132 'SKSE' 133 $hypothetical
Register-Replayed 132 '(null)' 133 $hypothetical
Assert-That ($script:Buckets[95].Contains(132)) 'Earlier genuine outfit H95 can target registered skee S132'
Write-Output 'PASS hypothetical actual H95<S132: senderH bucket contains skeeS. Does NOT prove VFS/filename sorting, runtime interface acquisition, or morph application.'
