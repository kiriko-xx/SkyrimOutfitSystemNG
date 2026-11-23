# Static guard: the post-load outfit refresh must never keep the task queue busy.
#
# SKSE drains the task queue until it is empty, so a task that queues itself again
# while its condition is not met never lets the frame end: the game freezes with no
# rendering and no cursor (1.5.62 shipped exactly that). The wait has to live on the
# wakeup timer instead.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$main = Get-Content -Raw -LiteralPath (Join-Path $root "src/cpp/main.cpp")

function Get-Block([string]$text, [string]$start, [string]$end) {
  $i = $text.IndexOf($start)
  if ($i -lt 0) { throw "guard: cannot find [$start]" }
  $j = $text.IndexOf($end, $i)
  if ($j -lt 0) { throw "guard: cannot find [$end] after [$start]" }
  return $text.Substring($i, $j - $i)
}

$probe = Get-Block $main "void PostPostLoadProbe()" "void BeginPostLoadRefresh()"
if ($probe -notmatch "AddTask") { throw "guard: the probe does not queue a task" }
if ($probe -match "PostPostLoadProbe\(\)\s*;") {
  throw "guard: the probe queues itself again; SKSE drains the queue until it is empty, so this freezes the game"
}
if ($probe -notmatch "Is3DLoaded") { throw "guard: the probe no longer checks the player 3D" }
if ($probe -notmatch "CreateRefreshAllArmorsTask") { throw "guard: the probe never refreshes the outfits" }

$service = Get-Block $main "void ServicePostLoadRefresh()" "void CALLBACK WakeRefreshQueues"
if ($service -notmatch "kPostLoadProbeIntervalMs") { throw "guard: the timer side lost its probe interval" }
if ($service -notmatch "kPostLoadGiveUpMs") { throw "guard: the timer side lost its give up budget" }
if ($service -notmatch "PostPostLoadProbe\(\)") { throw "guard: the timer never posts a probe" }

$wake = Get-Block $main "void CALLBACK WakeRefreshQueues" "void StartRefreshWakeup"
if ($wake -notmatch "ServicePostLoadRefresh\(\)") { throw "guard: the wakeup timer does not pump the post-load wait" }
if ($wake -match "Is3DLoaded") { throw "guard: the timer thread must not touch game data" }

if ($main -match "SchedulePostLoadRefresh") { throw "guard: the old self requeueing chain is back" }
if ($main -notmatch "BeginPostLoadRefresh\(\);") { throw "guard: kPostLoadGame no longer starts the wait" }
if ($main -notmatch "CancelPostLoadRefresh\(\);") { throw "guard: a load no longer cancels a pending wait" }
if ($main -notmatch "kPostLoadGiveUpMs = \d+") { throw "guard: the give up budget disappeared" }

# No task anywhere may queue the function that posted it again.
$files = Get-ChildItem -Recurse -Path (Join-Path $root "src") -Include *.cpp,*.h,*.hpp
foreach ($file in $files) {
  $text = Get-Content -Raw -LiteralPath $file.FullName
  foreach ($hit in [regex]::Matches($text, "AddTask\(")) {
    $before = $text.Substring(0, $hit.Index)
    $owner = [regex]::Matches($before, "(?m)^[\w:<>,&\*\s]+?\s(\w+)\s*\([^;]*$")
    if ($owner.Count -eq 0) { continue }
    $name = $owner[$owner.Count - 1].Groups[1].Value
    if ($name.Length -lt 4) { continue }
    $after = $text.Substring($hit.Index, [Math]::Min(3000, $text.Length - $hit.Index))
    if ($after -match ("(?<![\w:])" + [regex]::Escape($name) + "\s*\(")) {
      throw ("guard: " + $file.Name + " queues [" + $name + "] from inside its own task; that never lets the frame end")
    }
  }
}

Write-Output "POSTLOAD_WAIT_OK: the post-load wait is timer driven and no task re-queues itself"