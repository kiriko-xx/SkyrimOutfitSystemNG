# Verifies that every UI literal has a dictionary entry and that the two
# shipped translation files cover the dictionary exactly.
#
#   1. parses kDictionary out of src/cpp/I18n.cpp
#   2. parses dist/interface/translations/SkyrimOutfitSystemNG_<lang>.txt the way
#      I18n::ParseTranslation reads them ($key<TAB>value, taken verbatim)
#   3. fails when a key is missing, when an extra key appears, when a value or a
#      placeholder signature differs, or when a visible literal in src has no
#      dictionary entry, or when a widget label is passed as a bare literal
$ErrorActionPreference = "Stop"
$repo = Resolve-Path (Join-Path $PSScriptRoot "..")
$i18n = Join-Path $repo "src/cpp/I18n.cpp"

function Unescape([string] $text) {
    $sb = New-Object System.Text.StringBuilder
    $backslash = [char]92
    for ($i = 0; $i -lt $text.Length; $i++) {
        $c = $text[$i]
        if ($c -eq $backslash -and $i + 1 -lt $text.Length) {
            $n = $text[$i + 1]
            if ($n -eq "n") { [void]$sb.Append("`n"); $i++; continue }
            if ($n -eq "t") { [void]$sb.Append("`t"); $i++; continue }
            if ($n -eq $backslash) { [void]$sb.Append($backslash); $i++; continue }
            if ($n -eq [char]34) { [void]$sb.Append([char]34); $i++; continue }
        }
        [void]$sb.Append($c)
    }
    return $sb.ToString()
}

function Signature([string] $text) {
    $matches = [regex]::Matches($text, "(%[-0-9.#]*[diouxXeEfgGaAcsp%])")
    return (($matches | ForEach-Object { $_.Value }) -join ",")
}

$text = Get-Content -LiteralPath $i18n -Raw -Encoding utf8
$start = $text.IndexOf("constexpr Entry kDictionary[]{")
if ($start -lt 0) { Write-Error -ErrorAction Continue "kDictionary was not found in $i18n"; exit 1 }
$end = $text.IndexOf("};", $start)
if ($end -lt 0) { Write-Error -ErrorAction Continue "the end of kDictionary was not found"; exit 1 }
$block = $text.Substring($start, $end - $start)

$entries = [ordered]@{}
foreach ($m in [regex]::Matches($block, '\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\}')) {
    $key = Unescape $m.Groups[1].Value
    $value = Unescape $m.Groups[2].Value
    if ($entries.Contains($key)) { Write-Error -ErrorAction Continue "duplicate dictionary key: $key"; exit 1 }
    $entries[$key] = $value
}
if ($entries.Count -lt 300) { Write-Error -ErrorAction Continue "only $($entries.Count) dictionary entries were parsed"; exit 1 }

$failures = 0
$languages = @{
    "english" = "english";
    "chinese" = "chinese"
}
foreach ($name in $languages.Keys) {
    $path = Join-Path $repo ("dist/interface/translations/SkyrimOutfitSystemNG_" + $name + ".txt")
    if (-not (Test-Path -LiteralPath $path)) { Write-Error -ErrorAction Continue "missing $path"; $failures++; continue }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -lt 2 -or $bytes[0] -ne 0xFF -or $bytes[1] -ne 0xFE) {
        Write-Error -ErrorAction Continue "$path is not UTF-16LE with a BOM"; $failures++
    }
    $raw = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::Unicode)
    if (-not $raw.Contains("`r`n")) { Write-Error -ErrorAction Continue "$path does not use CRLF line endings"; $failures++ }
    $fileValues = @{}
    foreach ($line in $raw -split "`n") {
        if ($line.EndsWith("`r")) { $line = $line.Substring(0, $line.Length - 1) }
        if ($line.StartsWith("#") -or $line.Trim().Length -eq 0) { continue }
        $line = $line.TrimStart()
        if (-not $line.StartsWith("$")) { Write-Error -ErrorAction Continue "$path has a line without the `$ prefix: $line"; $failures++; continue }
        $body = $line.Substring(1)
        $tab = $body.IndexOf("`t")
        if ($tab -lt 0) { Write-Error -ErrorAction Continue "$path has a line without a tab: $line"; $failures++; continue }
        $fileValues[(Unescape $body.Substring(0, $tab))] = (Unescape $body.Substring($tab + 1))
    }
    foreach ($key in $entries.Keys) {
        if (-not $fileValues.ContainsKey($key)) { Write-Error -ErrorAction Continue "$path has no line for `"$key`""; $failures++; continue }
        $expected = if ($name -eq "chinese") { $entries[$key] } else { $key }
        if ($fileValues[$key] -ne $expected) {
            Write-Error -ErrorAction Continue "$path maps `"$key`" to `"$($fileValues[$key])`" instead of `"$expected`""
            $failures++
        }
        $a = Signature $key
        $b = Signature $fileValues[$key]
        if ($a -ne $b) {
            Write-Error -ErrorAction Continue "$path changes the placeholders of `"$key`": [$a] vs [$b]"
            $failures++
        }
    }
    foreach ($key in $fileValues.Keys) {
        if (-not $entries.Contains($key)) { Write-Error -ErrorAction Continue "$path has the stale line `"$key`""; $failures++ }
    }
}

# Every visible literal the sources pass to ImGui or I18n must be translatable.
$sources = Get-ChildItem -Path (Join-Path $repo "src") -Recurse -Include *.cpp, *.h -File
$seen = @{}
foreach ($file in $sources) {
    if ($file.FullName -match '[\\/]include[\\/]') { continue }
    $content = Get-Content -LiteralPath $file.FullName -Raw -Encoding utf8
    foreach ($m in [regex]::Matches($content, '\b(?:IG::\w+|ImGui::\w+|I18n::(?:Tr|TrId|T))\(\s*"((?:[^"\\]|\\.)*)"')) {
        $literal = Unescape $m.Groups[1].Value
        if ($literal.StartsWith("##")) { continue }
        if ($literal.Contains("\")) { continue }
        if ($literal -eq "auto") { continue }
        if ($literal.Length -le 1) { continue }
        $stripped = [regex]::Replace($literal, "%[-0-9.#]*[A-Za-z%]", " ")
        if ($stripped -notmatch "[A-Za-z]{2,}") { continue }
        if ($entries.Contains($literal) -or $seen.ContainsKey($literal)) { continue }
        $seen[$literal] = $true
        Write-Error -ErrorAction Continue ("{0}: the literal `"{1}`" has no dictionary entry" -f $file.Name, $literal)
        $failures++
    }
}

# A widget label or a message that reaches ImGui as a bare literal can never be
# translated, no matter what the files say: it has to go through I18n::Tr (text)
# or I18n::TrId (labels that are also the widget id).
$bareReported = @{}
foreach ($file in $sources) {
    if ($file.FullName -match '[\\/]include[\\/]') { continue }
    $content = Get-Content -LiteralPath $file.FullName -Raw -Encoding utf8
    foreach ($m in [regex]::Matches($content, '\bIG::(\w+)\(\s*"((?:[^"\\]|\\.)*)"')) {
        $literal = Unescape $m.Groups[2].Value
        if ($literal.StartsWith("##")) { continue }
        if ($literal.Contains("\")) { continue }
        $stripped = [regex]::Replace($literal, "%[-0-9.#]*[A-Za-z%]", " ")
        if ($stripped -notmatch "[A-Za-z]{2,}") { continue }
        $identity = $file.Name + "|" + $literal
        if ($bareReported.ContainsKey($identity)) { continue }
        $bareReported[$identity] = $true
        Write-Error -ErrorAction Continue ("{0}: IG::{1}() still passes the bare literal [{2}]; wrap it in I18n::Tr or I18n::TrId" -f $file.Name, $m.Groups[1].Value, $literal)
        $failures++
    }
}

if ($failures -gt 0) { Write-Error -ErrorAction Continue "i18n checks failed: $failures problem(s)"; exit 1 }
Write-Host ("I18N_TRANSLATION_FILES_OK: {0} entries, {1} language file(s)" -f $entries.Count, $languages.Count)
