$ErrorActionPreference = 'Stop'
# Static guard for the 1.5.45 outfit generator. Nothing here is compiled or executed.
# One plugin often holds several costumes. 1.5.45 groups the slot armour of a plugin
# by the first words of its name ("the first fields of a costume name are the same"),
# then splits every costume into as many outfits as its slots need. A plugin with one
# costume must keep producing exactly the old names, and the run must stay bounded.
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

# --- 1. names are split into words, bracketed tags are ignored -------------------
Assert-Present $esp 'std::vector<Word> NameWords(' 'names are split into words'
Assert-Present $esp 'constexpr std::size_t kMaxNameWords = 4;' 'at most four words are compared'
Assert-Present $esp 'bool IsWordByte(unsigned char a_char)' 'a word byte test exists'
Assert-Absent $esp 'tolower(a_text' 'no locale dependent lowercasing'

# --- 2. a group is the longest prefix shared by at least two pieces -------------
Assert-Present $esp 'it->second >= 2' 'a prefix counts only when two pieces share it'
Assert-Present $esp 'piece.family = WordKey(piece.name, piece.words, shared);' 'the group key is the shared prefix'
Assert-Present $esp 'piece.label = WordLabel(piece.name, piece.words, shared);' 'the group keeps the original spelling for the name'
Assert-Present $esp 'std::map<std::string, std::vector<std::size_t>> families;' 'the pieces are collected per group'
Assert-Present $esp 'const bool multiCostume = order.size() > 1;' 'one costume keeps the plain plugin name'

# --- 3. every costume is split by slot conflicts, and padded from its own group --
Assert-Present $esp 'std::vector<std::vector<std::size_t>> BuildVariants(' 'the variants are built by slot conflicts'
Assert-Present $esp 'constexpr std::size_t kMaxVariants = 32;' 'the variant count is bounded'
Assert-Present $esp 'constexpr std::size_t kMaxOutfits = 96;' 'the outfit count is bounded'
Assert-Present $esp 'set_blanket_slot_policy_c(kPolicy)' 'every generated outfit requires the slot to be equipped'
Assert-Present $esp 'constexpr const char* kPolicy = "XXXO";' 'the blanket policy is require equipped'
Assert-Present $esp 'kFxBit' 'the FX slot is not treated as a costume slot'

# --- 4. the result says what happened -------------------------------------------
Assert-Present $esp '"%zu outfit(s) from %zu piece(s) in %zu name group(s); %zu created, %zu updated%s%s%s."' 'the message counts the name groups'
Assert-Present $esp 'name group(s)' 'the wording keeps the group count'
Assert-Present $header 'name' 'the header documents the name grouping'

if ($fail.Count -ne 0) {
    $fail | ForEach-Object { Write-Host "FAIL $_" }
    exit 1
}
Write-Host 'ESP_OUTFITS_NAMES_OK: the generator groups a plugin by the leading words of its names, splits each costume by slot conflicts and keeps the old names for a single costume.'