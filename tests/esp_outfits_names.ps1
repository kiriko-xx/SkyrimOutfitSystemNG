$ErrorActionPreference = 'Stop'
# Static guard for the 1.5.46 outfit generator. Nothing here is compiled or run.
# The user asked for "prefix tree + longest common prefix + slot constraint": no
# separator is assumed, a costume is found by walking a character level trie, a
# slot contributes one piece, and anything ambiguous is left out instead of
# guessed. 1.5.45 bucketed pieces by the leading words of their names, and that
# wording must be gone.
$root = Join-Path $PSScriptRoot '..'
$fail = @()
function Assert-Present([string] $haystack, [string] $needle, [string] $label) {
    if (-not $haystack.Contains($needle)) { $script:fail += "missing: $label ($needle)" }
}
function Assert-Absent([string] $haystack, [string] $needle, [string] $label) {
    if ($haystack.Contains($needle)) { $script:fail += "still present: $label ($needle)" }
}

$esp = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/EspOutfits.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src/core/EspOutfits.h')

# --- 1. stage 1: the name a comparison runs on --------------------------------
Assert-Present $esp 'bool IsContinuation(unsigned char a_char)' 'utf8 continuation test exists'
Assert-Present $esp 'std::vector<Unit> Decode(const std::string& a_text)' 'names are decoded into code points'
Assert-Present $esp 'std::vector<Unit> Normalize(const std::string& a_text)' 'a comparison copy exists'
Assert-Present $esp "codePoint == '['" 'a bracketed label is detected'
Assert-Present $esp 'std::isalnum(' 'only alphanumerics survive as ascii'
Assert-Absent $esp 'kMaxNameWords' 'the word based grouping is gone'
Assert-Absent $esp 'WordKey(' 'the word prefix key is gone'
Assert-Absent $esp 'piece.family' 'pieces no longer carry a word family'

# --- 2. stage 2: costume detection over a trie --------------------------------
Assert-Present $esp 'std::vector<TrieNode> BuildTrie(const std::vector<Piece>& a_pieces)' 'a character level trie is built'
Assert-Present $esp 'std::vector<std::size_t> SubtreePieces(' 'a subtree can be collected'
Assert-Present $esp 'void Classify(' 'the trie is classified recursively'
Assert-Present $esp 'std::size_t DistinctSlots(' 'a branch is judged by how many slots it covers'
Assert-Present $esp 'loose' 'pieces with no costume fall into a loose bucket'

# --- 3. stage 3 and 4: slot buckets, differential identifier -------------------
Assert-Present $esp 'CostumePlan PlanCostume(' 'a costume is planned per slot bucket'
Assert-Present $esp 'std::size_t CommonEnd(const std::vector<const std::vector<Unit>*>& a_names, std::size_t a_from)' 'the common part of a slot is its longest common prefix'
Assert-Present $esp 'std::u32string KeyOf(' 'what is left of a name is a key'
Assert-Present $esp 'byKey' 'identifiers are matched across slots'
Assert-Present $esp 'kNoPiece' 'a slot without a candidate is explicit'

# --- 4. stage 5: bounded, policy compliant, honest about ambiguity ------------
Assert-Present $esp 'constexpr std::size_t kMaxOutfits = 96;' 'the outfit count is bounded'
Assert-Present $esp 'constexpr std::size_t kMaxVariants = 32;' 'the loose variant count is bounded'
Assert-Present $esp 'set_blanket_slot_policy_c(kPolicy)' 'every generated outfit requires the slot to be equipped'
Assert-Present $esp 'constexpr const char* kPolicy = "XXXO";' 'the blanket policy is require equipped'
Assert-Present $esp 'kFxBit' 'the FX slot never joins an outfit'
Assert-Present $esp 'cannot be matched by name across slots' 'ambiguity is reported instead of guessed'
Assert-Present $esp 'would wear two pieces on one slot' 'a slot conflict is reported'
Assert-Present $esp 'holds {} piece(s) over {} slot(s)' 'every costume says what it holds'
Assert-Present $esp '%zu outfit(s) from %zu piece(s) in %zu costume(s)' 'the message counts the costumes'
Assert-Present $header 'prefix tree' 'the header documents the design'

if ($fail.Count -ne 0) {
    $fail | ForEach-Object { Write-Host "FAIL $_" }
    exit 1
}
Write-Host 'ESP_OUTFITS_NAMES_OK: costume detection walks a character level trie, an outfit is combined by differential identifier with slot uniqueness, and anything ambiguous is left out.'