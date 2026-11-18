$ErrorActionPreference = 'Stop'
# 1.5.37: a bake that is only queued needs a thread that services the task queue or
# the engine's worn pass, and on the reporting machine neither ran while a morph
# slider was held (1.5.36: 49 detected changes, 3 bakes). The poll now re-morphs the
# clones the last full bake found, from a list of reference-held nodes, on whatever
# thread it was asked from. This guards the rules that make that safe.
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

# Off the game thread, so: no biped walk, no extra-data writes, no node creation.
foreach ($forbidden in @('GetBiped', 'partClone', 'AttachBodyTri(', 'AddExtraData', 'NiNode::Create', 'CreateNiObject')) {
  if ($f.Contains($forbidden)) { throw "The cached replay must not do this off the game thread: $forbidden" }
}

# The node list has to be reference-held, otherwise the engine can free a clone
# while a worker thread writes into it.
if (-not $f.Contains('RE::NiPointer<RE::NiAVObject>> nodes')) { throw 'The replay does not copy the held nodes' }
if (-not $s.Contains('std::unordered_map<RE::FormID, std::vector<RE::NiPointer<RE::NiAVObject>>> g_cloneNodes')) { throw 'The cache does not hold the clones by reference' }
if (-not $s.Contains('RE::NiPointer<RE::NiAVObject> holder = biped->objects[i].partClone')) { throw 'The bake does not hold the clone it walks' }

# One writer: the full bake publishes the list, the replay only reads it.
$stores = ([regex]::Matches($s, 'g_cloneNodes\[id\] = ')).Count + ([regex]::Matches($s, 'g_cloneNodes.erase\(')).Count
if ($stores -ne 2) { throw "Only the full bake may publish the list (found $stores sites)" }
if (-not $s.Contains('g_cloneNodes.clear();')) { throw 'The list is not cleared on reset' }

# The task latch that swallowed a whole drag must be gone, replaced by a stamp.
if ($s.Contains('g_taskQueued')) { throw 'The one-shot task latch is back' }
if (-not $s.Contains('std::chrono::milliseconds(250)')) { throw 'The task post is no longer throttled by a stamp' }

Write-Output 'MORPH_CACHE_REPLAY_OK: the poll re-morphs reference-held clones on any thread, without walking the biped or writing extra data.'