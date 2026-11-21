$ErrorActionPreference = 'Stop'
# 1.5.46: runs the real Stage 1 name helpers of src/core/EspOutfits.cpp.
# The block between the two stage 1 markers is pure (no engine types), so this test
# copies that exact text, compiles it with cl and checks what the trie design needs:
# separators never matter, a bracketed pack label is ignored, the original spelling
# survives, a three byte character is one unit, and a common prefix stops where the
# names stop agreeing.
$nl = [string][char]10
$root = Join-Path $PSScriptRoot '..'
$srcPath = Join-Path $root 'src/core/EspOutfits.cpp'
$src = [IO.File]::ReadAllText($srcPath)
$begin = $src.IndexOf('// --- stage 1 begin')
$end = $src.IndexOf('// --- stage 1 end')
if ($begin -lt 0 -or $end -le $begin) { Write-Host "FAIL the stage 1 markers are missing from $srcPath"; exit 1 }
$snippet = $src.Substring($begin, $end - $begin)
$cc = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
if (-not (Test-Path -LiteralPath $cc)) { Write-Host "SKIP no vcvars at $cc"; exit 0 }
$dir = Join-Path $root 'build/esp-outfits-trie'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
$cpp = Join-Path $dir 'trie_test.cpp'
$head = @'
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
'@
$tail = @'
static std::u32string Key(const std::string& a_text) { return KeyOf(Normalize(a_text), 0); }

static int s_fail = 0;
static void Check(bool a_ok, const char* a_what)
{
    if (!a_ok) {
        std::printf("FAIL %s" "\n", a_what);
        ++s_fail;
    }
}

static int run()
{
    // 1. separators never matter: space, underscore, dash, dot and case collapse.
    Check(Key("Sailor_Top") == Key("sailor-top"), "underscore and dash compare equal");
    Check(Key("Sailor Top") == Key("SAILOR_TO P"), "space and case compare equal");
    Check(Key("Star.Dress.Top") == Key("stardresstop"), "dots are dropped");

    // 2. a bracketed pack label is removed before comparing.
    Check(Key("[Caenarvon]StarDressTop") == Key("StarDressTop"), "a bracketed label is ignored");
    Check(Key("(Bunny)StarDressTop") == Key("StarDressTop"), "a parenthesised label is ignored");

    // 3. the original spelling is what an outfit is named with.
    {
        const std::string original = "StarDress_Top";
        const auto units = Normalize(original);
        Check(LabelRange(original, units, 0, units.size()) == original, "LabelRange keeps the original spelling");
        Check(!LabelRange(original, units, 0, units.size() / 2).empty(), "a partial label is not empty");
        Check(LabelRange(original, units, units.size(), units.size()).empty(), "an empty range is empty");
    }

    // 4. a three byte character is one unit and keeps its byte offsets.
    {
        const std::string wide = "\xE6\x98\x9F" "A";
        const auto units = Decode(wide);
        Check(units.size() == 2, "one CJK character plus one letter is two units");
        Check(units[0].length == 3 && units[0].begin == 0, "the CJK character keeps its three bytes");
        Check(units[1].begin == 3, "the letter starts after those three bytes");
        Check(units[0].codePoint == 0x661Fu, "the code point is decoded");
    }

    // 5. the common prefix stops where the names stop agreeing.
    {
        const auto hairA = Normalize("StarDressHairA");
        const auto topA = Normalize("StarDressTopA");
        const auto topB = Normalize("StarDressTopB");
        const auto skirtB = Normalize("StarDressSkirtB");
        const std::vector<const std::vector<Unit>*> all{ &hairA, &topA, &skirtB };
        Check(CommonEnd(all, 0) == 9, "three pieces share the costume name only");
        const std::vector<const std::vector<Unit>*> tops{ &topA, &topB };
        Check(CommonEnd(tops, 0) == 12, "two tops share everything but the identifier");
        const std::vector<const std::vector<Unit>*> mixed{ &hairA, &topA };
        Check(CommonEnd(mixed, 0) == 9, "a hair and a top only share the costume name");
        const auto shifted = Normalize("StarNightDressHairA");
        const std::vector<const std::vector<Unit>*> near{ &hairA, &shifted };
        Check(CommonEnd(near, 0) == 4, "a different costume only shares the very start");
        const std::vector<const std::vector<Unit>*> none{};
        Check(CommonEnd(none, 3) == 3, "no names mean no shared part");
        Check(CommonEnd(mixed, 400) == 400, "a start past the end is returned unchanged");
    }

    if (s_fail == 0) {
        std::printf("the name helpers are separator independent, label tolerant and prefix exact." "\n");
    }
    return s_fail == 0 ? 0 : 1;
}
'@
$text = ($head -join $nl) + $nl + 'namespace sos_test {' + $nl + $snippet + $nl + ($tail -join $nl) + $nl + '}  // namespace sos_test' + $nl + 'int main() { return sos_test::run(); }' + $nl
[IO.File]::WriteAllText($cpp, $text, (New-Object System.Text.UTF8Encoding($false)))
$bat = Join-Path $dir 'build.bat'
$batText = '@echo off' + $nl + ('call "' + $cc + '"') + $nl + 'cl /nologo /utf-8 /std:c++20 /EHsc /W4 /Fe:trie_test.exe trie_test.cpp' + $nl
[IO.File]::WriteAllText($bat, $batText, (New-Object System.Text.ASCIIEncoding))
Push-Location $dir
try {
    $build = cmd /c build.bat 2>&1
    $buildCode = $LASTEXITCODE
    $build | ForEach-Object { Write-Host $_ }
    if ($buildCode -ne 0) { Write-Host "FAIL the extracted helpers did not compile"; exit 1 }
    $run = cmd /c trie_test.exe 2>&1
    $runCode = $LASTEXITCODE
    $run | ForEach-Object { Write-Host $_ }
    if ($runCode -ne 0) { Write-Host "FAIL the name helpers do not behave as the trie design needs"; exit 1 }
    Write-Host 'ESP_OUTFITS_TRIE_OK: the Stage 1 name helpers of EspOutfits.cpp behave as the trie design needs.'
} finally { Pop-Location }
