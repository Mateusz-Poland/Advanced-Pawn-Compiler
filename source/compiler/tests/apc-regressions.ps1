param(
    [Parameter(Mandatory)] [string] $Compiler,
    [Parameter(Mandatory)] [string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
$Compiler = (Resolve-Path -LiteralPath $Compiler).Path
$scratch = New-Item -ItemType Directory -Force -Path $OutputDirectory
$scratch = New-Item -ItemType Directory -Path (Join-Path $scratch.FullName ([guid]::NewGuid().ToString('N')))

function Invoke-PawnCase {
    param([string] $Name, [string] $Source, [string[]] $Flags = @(),
          [string] $ExpectedError, [string] $Output = (Join-Path $scratch.FullName "$Name.amx"))
    $inputFile = Join-Path $scratch.FullName "$Name.pwn"
    [IO.File]::WriteAllText($inputFile, $Source)
    $messages = (& $Compiler $inputFile "-o$Output" @Flags 2>&1 | Out-String)
    $code = $LASTEXITCODE
    if ($ExpectedError) {
        if ($code -ne 1 -or $messages -notmatch $ExpectedError -or (Test-Path -LiteralPath $Output)) {
            throw "$Name did not fail as expected (exit $code). $messages"
        }
    } elseif ($code -ne 0 -or !(Test-Path -LiteralPath $Output)) {
        throw "$Name failed (exit $code). $messages"
    }
    [pscustomobject]@{ Amx = $Output; Messages = $messages }
}

$validName = 'a' * 31
$longName = 'a' * 32
[void](Invoke-PawnCase -Name 'pragma-unused-boundary' -Source @"
main() {
    new $validName, other;
    #pragma unused $validName, other
    return 42;
}
"@)
foreach ($pragma in 'codepage','library','rational','unused','option') {
    [void](Invoke-PawnCase -Name "pragma-$pragma-too-long" -ExpectedError 'error 093' -Source @"
#pragma $pragma $longName
main() { return 42; }
"@)
}
[void](Invoke-PawnCase -Name 'output-open' -Source 'main() { return 42; }' -Output (Join-Path $scratch.FullName 'missing/out.amx') -ExpectedError 'fatal error 101')
[void](Invoke-PawnCase -Name 'syntax' -Source 'main() { new n = ; }' -ExpectedError 'error [0-9]+')
[void](Invoke-PawnCase -Name 'multidimensional-array' -Source @'
static cube[2][3][2] = {{{1,2},{3,4},{5,6}},{{7,8},{9,10},{11,12}}};
stock getrow(row[]) { return row[0] + row[1]; }
main() { return getrow(cube[1][2]) + cube[0][1][1]; }
'@)
[void](Invoke-PawnCase -Name 'symbol-hash-collision' -Source @'
new symbol037507 = 7;
new symbol075776 = 9;
main() { return symbol037507 * 10 + symbol075776; }
'@)

$macroSource = @(
    '#define APC_VALUE 7'
    '#define APC_VALUE 7'
    '#define APC_VALUE_LONG 11'
    '#define APC_WRAP(%0) ((%0) + APC_VALUE)'
    '#if !defined APC_VALUE'
    '#error macro lookup failed'
    '#endif'
    '#undef APC_VALUE'
    '#define APC_VALUE 19'
)
$macroSource += 1..200 | ForEach-Object { "#define APC_GROWTH_$_ $_" }
$macroSource += @(
    '#undef APC_GROWTH_70'
    '#define APC_GROWTH_70 700'
    '#if APC_WRAP(1) != 20 || APC_VALUE_LONG != 11 || APC_GROWTH_70 != 700 || APC_GROWTH_200 != 200'
    '#error macro expansion failed'
    '#endif'
    'main() { return APC_WRAP(APC_GROWTH_70); }'
)
[void](Invoke-PawnCase -Name 'macro-prefix-mutation-growth' -Source ($macroSource -join [Environment]::NewLine))

$tagSource = 0..99 | ForEach-Object { 'stock apcTag{0}:apcFn{0}() {{ return apcTag{0}:{0}; }}' -f $_ }
$tagSource += 'main() { return _:apcFn0() + _:apcFn31() + _:apcFn99(); }'
[void](Invoke-PawnCase -Name 'tag-index-growth' -Source ($tagSource -join [Environment]::NewLine))

[void](Invoke-PawnCase -Name 'switch-unsorted-range' -Source @"
main() {
    new value = 42;
    switch (value) {
        case 100, 120: return 1;
        case -50 .. -1: return 2;
        case 40 .. 50: return 3;
        case 3, 0: return 4;
        default: return 0;
    }
}
"@)
[void](Invoke-PawnCase -Name 'switch-duplicate-range' -ExpectedError 'error 040' -Source @"
main() {
    new value = 42;
    switch (value) {
        case 100, 42: return 1;
        case 40 .. 50: return 2;
    }
    return 0;
}
"@)

$include = New-Item -ItemType Directory -Path (Join-Path $scratch.FullName 'include')
[void](New-Item -ItemType Directory -Path (Join-Path $include.FullName 'from_inc'))
[IO.File]::WriteAllLines((Join-Path $include.FullName 'from_inc.inc'), @('#define FROM_INC 1'))
[IO.File]::WriteAllLines((Join-Path $include.FullName 'from_p.p'), @('#define FROM_P 2'))
[IO.File]::WriteAllLines((Join-Path $include.FullName 'from_pawn.pawn'), @('#define FROM_PAWN 3'))
[IO.File]::WriteAllLines((Join-Path $include.FullName 'order.inc'), @('#define ORDER 36'))
[IO.File]::WriteAllLines((Join-Path $include.FullName 'order.p'), @('#error wrong extension precedence'))
[void](Invoke-PawnCase -Name 'include-fallbacks' -Flags "-i$($include.FullName)" -Source @'
#include <from_inc>
#include <from_p>
#include <from_pawn>
#include <order>
#tryinclude <not_present>
main() { return FROM_INC + FROM_P + FROM_PAWN + ORDER; }
'@)
$missing = @'
#include <not_present>
main() { return 42; }
'@
[void](Invoke-PawnCase -Name 'missing-include' -Source $missing -ExpectedError 'fatal error 100')

$left = New-Item -ItemType Directory -Path (Join-Path $include.FullName 'apc_long_prefix_compatibility/left')
$right = New-Item -ItemType Directory -Path (Join-Path $include.FullName 'apc_long_prefix_compatibility/right')
[IO.File]::WriteAllLines((Join-Path $left.FullName 'shared.inc'), @(
    '#if defined APC_LEFT'
    '#error qualified include was read twice'
    '#endif'
    '#define APC_LEFT 15'
))
[IO.File]::WriteAllLines((Join-Path $right.FullName 'shared.inc'), @('#define APC_RIGHT 27'))
[IO.File]::WriteAllLines((Join-Path $include.FullName 'apc_bare.inc'), @(
    '#if defined APC_BARE'
    '#error bare include was read twice'
    '#endif'
    '#define APC_BARE 1'
))
[void](Invoke-PawnCase -Name 'compatibility-include-identity' -Flags @('-Z+', "-i$($include.FullName)") -Source @"
#include <apc_long_prefix_compatibility/left/shared.inc>
#include <apc_long_prefix_compatibility/right/shared.inc>
#include <apc_long_prefix_compatibility\left\.\shared.inc>
#include <apc_bare>
#include <apc_bare>
#if !defined _inc_apc_bare
#error legacy bare include marker missing
#endif
#if APC_LEFT + APC_RIGHT != 42
#error either include was skipped
#endif
main() { return APC_LEFT + APC_RIGHT; }
"@)
[void](Invoke-PawnCase -Name 'upstream-constexpr-chain' -Source @"
#if !(30 < 40 < 50) || (30 < 40 < 35)
#error comparison chain evaluation failed
#endif
main() { return 0; }
"@)
[void](Invoke-PawnCase -Name 'upstream-multidimensional-size' -ExpectedError 'error 009' -Source @"
new invalid[][2];
main() { return invalid[0][0]; }
"@)
[void](Invoke-PawnCase -Name 'upstream-local-self-init' -ExpectedError 'error 017' -Source 'main() { new self = self; return self; }')
[void](Invoke-PawnCase -Name 'upstream-skipped-directive' -Source @"
#if 0
#unknown_directive "literal"
#endif
main() { return 0; }
"@)
$warning = Invoke-PawnCase -Name 'upstream-empty-warning' -Source "#warning$([Environment]::NewLine)main() { return 0; }"
if ($warning.Messages -notmatch 'warning 237') { throw 'Missing empty warning diagnostic' }
[void](Invoke-PawnCase -Name 'upstream-symbol-suggestion' -ExpectedError 'did you mean "knownValue"' -Source @"
main() {
    new knownValue = 42;
    return knownValu;
}
"@)
$emitted = Invoke-PawnCase -Name 'upstream-native-operand' -Source @"
native Plain();
main() {
    __emit sysreq.c Plain;
    return 0;
}
"@
$nativeAmx = [IO.File]::ReadAllBytes($emitted.Amx)
if ([BitConverter]::ToInt32($nativeAmx, 40) - [BitConverter]::ToInt32($nativeAmx, 36) -ne 8) {
    throw 'Native referenced from __emit missing from AMX'
}


[void](Invoke-PawnCase -Name 'upstream-addressof' -Source @"
const INDEX = 1;
new grid[3][2];
main() {
    new local[2];
    new a = __addressof local;
    #pragma unused local, a
    return __addressof(grid[INDEX][1]) - __addressof(grid);
}
"@)

$encodingSource = @"
new samples[] = {0, 63, -64, 64, -65, 127, -128, 0x12345678, 0x80000000, 0xFFFFFFFF};
main() { return samples[0]; }
"@
$compact = Invoke-PawnCase -Name 'compact-cell-boundaries' -Source $encodingSource -Flags @('-C+','-d0')
$raw = Invoke-PawnCase -Name 'raw-cell-boundaries' -Source $encodingSource -Flags @('-C-','-d0')
$compactBytes = [IO.File]::ReadAllBytes($compact.Amx)
$rawBytes = [IO.File]::ReadAllBytes($raw.Amx)
if (([BitConverter]::ToUInt16($compactBytes, 8) -band 4) -eq 0) { throw 'Expected compact AMX' }
$code = [BitConverter]::ToInt32($compactBytes, 12)
$end = [BitConverter]::ToInt32($compactBytes, 0)
$rawEnd = [BitConverter]::ToInt32($rawBytes, 20)
$cursor = $code
for ($p = $code; $p -lt $rawEnd; $p += 4) {
    if ($cursor -ge $end) { throw 'Compact AMX ended early' }
    $unit = $compactBytes[$cursor++]
    $sign = if ($unit -band 64) { -1L } else { 0L }
    $value = ($sign -shl 7) -bor ($unit -band 127)
    $groups = 1
    while ($unit -band 128) {
        if ($cursor -ge $end -or ++$groups -gt 5) { throw 'Invalid compact AMX cell' }
        $unit = $compactBytes[$cursor++]
        $value = ($value -shl 7) -bor ($unit -band 127)
    }
    if (($value -band 4294967295L) -ne [BitConverter]::ToUInt32($rawBytes, $p)) {
        throw "Compact AMX differs from the raw cell at byte $p"
    }
}
if ($cursor -ne $end) { throw 'Trailing compact AMX cells' }


$cacheA = New-Item -ItemType Directory -Path (Join-Path $include.FullName 'cache-first')
$cacheB = New-Item -ItemType Directory -Path (Join-Path $include.FullName 'cache-second')
[IO.File]::WriteAllText((Join-Path $cacheA.FullName 'choice.p'), '#define APC_CHOICE 7')
[IO.File]::WriteAllText((Join-Path $cacheB.FullName 'choice.inc'), '#define APC_CHOICE 99')
foreach ($n in 'a','b') {
    $folder = New-Item -ItemType Directory -Path (Join-Path $include.FullName "local-$n")
    [IO.File]::WriteAllText((Join-Path $folder.FullName 'include.inc'), '#include "entry"')
    [IO.File]::WriteAllText((Join-Path $folder.FullName 'entry.inc'), "#define APC_ENTRY_$n 1")
}
[void](Invoke-PawnCase -Name 'include-cache-search-order' -Flags @(
    "-i$($cacheA.FullName)", "-i$($cacheB.FullName)", "-i$($include.FullName)"
) -Source @"
#tryinclude <absent_from_both>
#include <choice>
#include <choice>
#include <local-a\include.inc>
#include <local-b\include.inc>
#tryinclude <absent_from_both>
#if APC_CHOICE != 7 || !defined APC_ENTRY_a || !defined APC_ENTRY_b
#error include resolution changed
#endif
main() { return APC_CHOICE; }
"@)

$retrySource = [Text.StringBuilder]::new()
[void]$retrySource.AppendLine('new cells[4800] = {')
for ($i=0; $i -lt 4800; $i+=12) {
    $row = @(for ($j=$i; $j -lt $i+12; $j++) { '0x{0:x8}' -f (0x71230000+$j) }) -join ', '
    if ($i+12 -lt 4800) { $row += ',' }
    [void]$retrySource.AppendLine($row)
}
[void]$retrySource.AppendLine('};')
[void]$retrySource.AppendLine('main() { if (cells[1]) return cells[2]; return cells[3]; }')
$retryCompact = Invoke-PawnCase -Name 'assembler-compact-retry' -Flags @('-C+','-d0') -Source $retrySource.ToString()
$retryRaw = Invoke-PawnCase -Name 'assembler-compact-retry-raw' -Flags @('-C-','-d0') -Source $retrySource.ToString()
if ($retryCompact.Messages -notmatch 'warning 232' -or
    (Get-FileHash -LiteralPath $retryCompact.Amx).Hash -ne (Get-FileHash -LiteralPath $retryRaw.Amx).Hash) {
    throw 'Assembly retry did not reproduce the raw AMX'
}


$recursion = @'
direct(n) { return n ? direct(n - 1) + 1 : 0; }
first(n) { return n ? second(n - 1) + 1 : 0; }
second(n) { return n ? first(n - 1) + 1 : 0; }
main() { return direct(20) + first(22); }
'@
$plain = Invoke-PawnCase -Name 'recursion' -Source $recursion -Flags '-v2'
if ($plain.Messages -notmatch 'unknown, due to recursion') {
    throw "Recursion was not reported without -R+. $($plain.Messages)"
}
$detailed = Invoke-PawnCase -Name 'recursion-detailed' -Source $recursion -Flags @('-R+','-v2')
if ($detailed.Messages -notmatch 'directly calls itself' -or $detailed.Messages -notmatch 'indirectly calls itself') {
    throw "A recursion chain was missing. $($detailed.Messages)"
}

$chain = [Text.StringBuilder]::new()
[void]$chain.AppendLine('f00000() { return 42; }')
for ($i = 1; $i -le 20000; $i++) {
    [void]$chain.AppendLine(('f{0:d5}() {{ return f{1:d5}(); }}' -f $i, ($i - 1)))
}
[void]$chain.AppendLine('main() { return f20000(); }')
$deep = Invoke-PawnCase -Name 'deep-call-chain' -Source $chain.ToString() -Flags '-v2'
if ($deep.Messages -notmatch 'estimated max. usage=\d+ cells') { throw 'Missing chain stack estimate.' }

$count = 32769
$publics = [Text.StringBuilder]::new()
for ($i = 0; $i -lt $count; $i++) {
    $unsorted = ($i * 131) % $count
    [void]$publics.AppendLine(('public fn{0:d5}() {{ return {0}; }}' -f $unsorted))
}
$exported = Invoke-PawnCase -Name 'public-table' -Source $publics.ToString() -Flags @('-w235-','-d2')
$bytes = [IO.File]::ReadAllBytes($exported.Amx)
$size = [BitConverter]::ToInt32($bytes, 0)
$flags = [BitConverter]::ToInt16($bytes, 8)
$stride = [BitConverter]::ToInt16($bytes, 10)
$codeOffset = [BitConverter]::ToInt32($bytes, 12)
$dataOffset = [BitConverter]::ToInt32($bytes, 16)
$publicOffset = [BitConverter]::ToInt32($bytes, 32)
$nativeOffset = [BitConverter]::ToInt32($bytes, 36)
$nameTable = [BitConverter]::ToInt32($bytes, 52)
if ($stride -ne 8 -or $nativeOffset - $publicOffset -ne $stride * $count) {
    throw 'Wrong AMX public table size.'
}
if (($flags -band 2) -ne 0 -or $size -ne $bytes.Length -or $exported.Messages -notmatch 'warning 240') {
    throw 'The oversized debug section was not handled correctly.'
}
for ($i = 0; $i -lt $count; $i++) {
    $entry = $publicOffset + $i * $stride
    $address = [BitConverter]::ToInt32($bytes, $entry)
    $name = [BitConverter]::ToInt32($bytes, $entry + 4)
    $expected = 'fn{0:d5}' -f $i
    if ($address -lt 0 -or $address -ge ($dataOffset - $codeOffset) -or
        $name -lt $nameTable + 2 -or $name + $expected.Length -ge $codeOffset -or
        [Text.Encoding]::ASCII.GetString($bytes, $name, $expected.Length) -ne $expected -or
        $bytes[$name + $expected.Length] -ne 0) {
        throw "Bad AMX public entry $i (address $address, name offset $name)."
    }
}

Write-Output "APC regressions passed. Fixtures and AMX files are in $($scratch.FullName)"
