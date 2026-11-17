$ErrorActionPreference = 'Stop'
$work = Join-Path $PSScriptRoot '../build/call-site-snapshot-regression'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$include = (Resolve-Path (Join-Path $PSScriptRoot '../src/cpp/include')).Path
$cpp = @'
#include "hooking/CallSiteSnapshot.h"
#include <cassert>
#include <iostream>
#include <limits>

using Snapshot = Hooking::CallSiteSnapshot;
using Bytes = Snapshot::Bytes;
constexpr auto maxAddress = (std::numeric_limits<std::uintptr_t>::max)();
constexpr Bytes positive{ 0xE8, 0x34, 0x12, 0x00, 0x00 };
constexpr Bytes negative{ 0xE8, 0xF0, 0xFF, 0xFF, 0xFF };
constexpr Bytes zero{ 0xE8, 0, 0, 0, 0 };
constexpr Bytes maxRel{ 0xE8, 0xFF, 0xFF, 0xFF, 0x7F };
constexpr Bytes minRel{ 0xE8, 0, 0, 0, 0x80 };

// Require the production implementation to be usable as pure constexpr code too.
static_assert(Snapshot(0x1000, positive).DirectTarget() == 0x2239);
static_assert(Snapshot(0x1000, negative).DirectTarget() == 0xFF5);
static_assert(Snapshot(0x1000, zero).ResumeAddress() == 0x1005);
static_assert(!Snapshot(maxAddress - 4, zero).ResumeAddress());
static_assert(!Snapshot(0, minRel).DirectTarget());

int main()
{
    assert(Snapshot(0x1000, positive).DirectTarget() == 0x2239);
    assert(Snapshot(0x1000, negative).DirectTarget() == 0xFF5);
    assert(Snapshot(0x1000, zero).DirectTarget() == 0x1005);
    assert(Snapshot(0x1000, zero).ResumeAddress() == 0x1005);
    assert(Snapshot(0, maxRel).DirectTarget() == std::uintptr_t{0x80000004u});
    assert(Snapshot(0x80000000u, minRel).DirectTarget() == 5);
    assert(Snapshot(11, negative).DirectTarget() == 0); // zero is a valid result
    assert(!Snapshot(10, negative).DirectTarget());
    assert(!Snapshot(0, minRel).DirectTarget());
    assert(Snapshot(maxAddress - 5, zero).ResumeAddress() == maxAddress);
    assert(Snapshot(maxAddress - 5, zero).DirectTarget() == maxAddress);
    assert(!Snapshot(maxAddress - 4, zero).ResumeAddress());
    assert(!Snapshot(maxAddress - 4, zero).DirectTarget());
    assert(!Snapshot(maxAddress, negative).DirectTarget()); // reject resume wrap first
    assert(!Snapshot(maxAddress - 5, positive).DirectTarget());
    assert(Snapshot(maxAddress - 5 - 0x1234, positive).DirectTarget() == maxAddress);
    for (const auto opcode : { 0xE9, 0x90, 0xFF }) {
        auto bytes = positive;
        bytes[0] = static_cast<std::uint8_t>(opcode);
        const Snapshot snapshot(0x1000, bytes);
        assert(!snapshot.DirectTarget());
        assert(snapshot.ResumeAddress() == 0x1005);
    }
    auto liveBytes = positive;
    const Snapshot snapshot(0x1000, liveBytes);
    assert(snapshot.MatchesOriginal(liveBytes));
    for (std::size_t i = 0; i < liveBytes.size(); ++i) {
        liveBytes = positive;
        liveBytes[i] ^= 1;
        assert(!snapshot.MatchesOriginal(liveBytes));
        assert(snapshot.OriginalBytes() == positive); // owns its captured bytes
    }
    // Real local FF25 thunk + pointer slot: E8 must return the thunk, not its callee.
    std::array<std::uint8_t, 32> memory{};
    memory[16] = 0xFF;
    memory[17] = 0x25;
    const auto site = reinterpret_cast<std::uintptr_t>(memory.data());
    const auto finalTarget = site + 31;
    for (std::size_t i = 0; i < sizeof(finalTarget); ++i) {
        memory[22 + i] = static_cast<std::uint8_t>(finalTarget >> (i * 8));
    }
    const Snapshot thunkCall(site, Bytes{ 0xE8, 11, 0, 0, 0 });
    assert(thunkCall.DirectTarget() == site + 16);
    assert(thunkCall.DirectTarget() != finalTarget);
    // Fabricated/unmapped addresses work too: no target memory is dereferenced.
    assert(Snapshot(0x1000, positive).DirectTarget() == 0x2239);
    std::cout << "CALL_SITE_SNAPSHOT_OK: production header; rel32 signs/extremes, non-E8, "
                 "resume/target overflow, byte changes, immediate FF25 destination only\n";
}
'@
Set-Content -Path (Join-Path $work 'test_call_site_snapshot.cpp') -Value $cpp -Encoding utf8
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$cmd = 'call "' + $vcvars + '" >nul && cl /nologo /EHsc /std:c++20 /W4 /WX /UNDEBUG /I"' + $include + '" test_call_site_snapshot.cpp /Fe:test_call_site_snapshot.exe && test_call_site_snapshot.exe'
Push-Location $work
try {
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0) { throw 'Call-site snapshot regression failed' }
} finally {
    Pop-Location
}
