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
    [void]$publics.AppendLine(('public fn{0:d5}() {{ return {0}; }}' -f $i))
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
