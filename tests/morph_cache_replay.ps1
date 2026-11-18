$ErrorActionPreference = 'Stop'
# 1.5.37: a bake that is only queued needs a thread that services the task queue or
# the engine's worn pass, and on the reporting machine neither ran while a morph
# slider was held (1.5.36: 49 detected changes, 3 bakes). The poll now re-morphs the
# clones the last full bake found, from a list of reference-held nodes, on whatever
# thread it was asked from.
#
# 1.5.38: that replay crashed the game on repeated OBody preset switches, because a
# cached node can be a subtree the engine has already detached and replaced, and
# because extra data on a live clone must never be rewritten. These are the rules
# that keep the replay inside what a third-party morph pass can tolerate.
$s = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$start = $s.IndexOf('std::size_t ReplayCachedMorphs(')
if ($start -lt 0) { throw 'Missing cached replay entry point' }
$end = $s.IndexOf('// skee v5, called once per morphed shape', $start)
if ($end -le $start) { throw 'Cannot delimit the cached replay' }
$f = $s.Substring($start, $end-$start)

# It may only write vertices through the morph interface, and only while the
# self-write flag is up, so the skee callback cannot mistake our write for a morph.
if (-not $f.Contains('g_api->ApplyVertexDiff(a_actor, node, false)')) { throw 'The replay does not apply the morphs' }
if (-not $f.Contains('morphPush::g_selfWrite.store(true')) { throw 'The replay does not raise the self-write flag' }
if (-not $f.Contains('morphPush::g_selfWrite.store(false')) { throw 'The replay never lowers the self-write flag' }

# 1.5.38: a clone the engine no longer draws in that slot is a detached subtree;
# handing it to skee makes skee walk objects nobody owns.
if (-not $f.Contains('partClone.get() != node')) { throw 'The replay does not check the node is still the drawn clone' }
if (-not $f.Contains('skipped {} cached clone(s)')) { throw 'The replay does not report the skipped nodes' }

# Still no writes of any kind off the game thread: no attach, no clear, no create.
foreach ($forbidden in @('AttachBodyTri(', 'AddExtraData', 'RemoveExtraData', 'ClearBipedPart', 'NiNode::Create', 'CreateNiObject', 'g_cloneNodes[')) {
  if ($f.Contains($forbidden)) { throw "The cached replay must not do this off the game thread: $forbidden" }
}

# A body morph provider runs its own pass over the same clones; ours stays off
# them for a moment after it does (the 1.5.38 crash overlapped the two).
if (-not $f.Contains('morphPush::ReplaysHeld()')) { throw 'The replay ignores the hold window of a provider morph pass' }
if (-not $s.Contains('morphPush::HoldReplays(morphPush::kReplayHoldAfterMorphPassMs)')) { throw 'The OBody morph event does not open a hold window' }
if (-not $f.Contains('morphPush::kMinReplayGap')) { throw 'The replay has no floor between two passes' }

# 7f5a2ce: never take a BODYTRI away from a live node. The fix was made once, then
# lost when 1.5.33 restored src from the 1.5.29 tag, and the crash came back.
if ($s.Contains('RemoveExtraData("BODYTRI")')) { throw 'A live BODYTRI is being removed again' }
if (-not $s.Contains('if (a_node->GetExtraData("BODYTRI")) {')) { throw 'AttachBodyTri no longer refuses a node that already has a target' }

# The node list has to be reference-held, otherwise the engine can free a clone
# while a worker thread writes into it. The slot and the eye are cached with it so
# the replay can tell whether the engine still draws that node there.
if (-not $f.Contains('std::vector<CachedClone> clones;')) { throw 'The replay does not copy the cached clones' }
if (-not $s.Contains('std::unordered_map<RE::FormID, std::vector<CachedClone>> g_cloneNodes')) { throw 'The cache does not hold the clones by reference' }
if (-not $s.Contains('struct CachedClone')) { throw 'The cache entry lost the slot and eye it was recorded for' }
if (-not $s.Contains('holders.push_back(CachedClone{ holder, i, firstPerson });')) { throw 'The bake does not record which slot it held' }

# One writer: the full bake publishes the list, the replay only reads it.
$stores = ([regex]::Matches($s, 'g_cloneNodes\[id\] = ')).Count + ([regex]::Matches($s, 'g_cloneNodes.erase\(')).Count
if ($stores -ne 2) { throw "Only the full bake may publish the list (found $stores sites)" }
if (-not $s.Contains('g_cloneNodes.clear();')) { throw 'The list is not cleared on reset' }

# The task latch that swallowed a whole drag must be gone, replaced by a stamp.
if ($s.Contains('g_taskQueued')) { throw 'The one-shot task latch is back' }
if (-not $s.Contains('std::chrono::milliseconds(250)')) { throw 'The task post is no longer throttled by a stamp' }

Write-Output 'MORPH_CACHE_REPLAY_OK: the replay morphs reference-held clones the engine still draws, never rewrites live extra data, and stays off a provider pass.'
