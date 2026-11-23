@echo off
setlocal enabledelayedexpansion
echo Generating heavy realistic RP gamemode test (80,000 functions + 80,000 calls)...
echo This will take a couple of minutes in CMD, please wait...

set FILE=realistic_gm.pwn

echo // Realistic RP Gamemode Imitation > %FILE%
echo #include ^<a_samp^> >> %FILE%
echo. >> %FILE%
echo #define MAX_HOUSES 5000 >> %FILE%
echo #define MAX_BIZ 2000 >> %FILE%
echo. >> %FILE%
echo enum pInfo { >> %FILE%
echo     pID, pName[24], pMoney, pScore, pLevel, pAdmin, >> %FILE%
echo     Float:pPos[3], pInterior, pVW >> %FILE%
echo }; >> %FILE%
echo new PlayerInfo[MAX_PLAYERS][pInfo]; >> %FILE%
echo. >> %FILE%
echo new HugeDataArray[7000000]; // Нагрузка на I/O и блочную запись >> %FILE%
echo. >> %FILE%

echo // --- Генерируем 80 000 функций --- >> %FILE%
for /l %%i in (1,1,80000) do (
    set /a "mod=%%i %% 5"
    if !mod! equ 0 (
        echo stock MathFunc_%%i^(a, b^) { >> %FILE%
        echo     new result = ^(a * b^) + %%i; >> %FILE%
        echo     if ^(result ^> 100^) return result %% 50; >> %FILE%
        echo     return result; >> %FILE%
        echo } >> %FILE%
    ) else if !mod! equ 1 (
        echo stock LoopFunc_%%i^(arr[], size^) { >> %FILE%
        echo     new sum = 0; >> %FILE%
        echo     for ^(new j = 0; j ^< size; j++^) { >> %FILE%
        echo         arr[j] = ^(j * %%i^) + sum; >> %FILE%
        echo         sum += arr[j]; >> %FILE%
        echo     } >> %FILE%
        echo     return sum; >> %FILE%
        echo } >> %FILE%
    ) else if !mod! equ 2 (
        echo stock CondFunc_%%i^(val^) { >> %FILE%
        echo     if ^(val ^> 0^) { >> %FILE%
        echo         if ^(val %% 2 == 0^) return val * %%i; >> %FILE%
        echo         else return val + %%i; >> %FILE%
        echo     } else { >> %FILE%
        echo         return -1; >> %FILE%
        echo     } >> %FILE%
        echo } >> %FILE%
    ) else if !mod! equ 3 (
        echo stock StringFunc_%%i^(^) { >> %FILE%
        echo     new string[256]; >> %FILE%
        echo     format^(string, sizeof^(string^), "Generated string number %%i with text."^); >> %FILE%
        echo     return strlen^(string^); >> %FILE%
        echo } >> %FILE%
    ) else (
        echo stock SwitchFunc_%%i^(playerid^) { >> %FILE%
        echo     switch^(playerid^) { >> %FILE%
        echo         case 1: return %%i; >> %FILE%
        echo         case 2: return %%i + 1; >> %FILE%
        echo         case 3: return %%i + 2; >> %FILE%
        echo         case 4: return %%i * 2; >> %FILE%
        echo         case 5: return %%i * 3; >> %FILE%
        echo         default: return 0; >> %FILE%
        echo     } >> %FILE%
        echo     return 0; >> %FILE%
        echo } >> %FILE%
    )
)

echo // --- Точка входа: вызываем все 80 000 функций --- >> %FILE%
echo main^(^) { >> %FILE%
echo     HugeDataArray[0] = 1; >> %FILE%
for /l %%i in (1,1,80000) do (
    set /a "mod=%%i %% 5"
    if !mod! equ 0 (
        echo     MathFunc_%%i^(2, 3^); >> %FILE%
    ) else if !mod! equ 1 (
        echo     LoopFunc_%%i^(HugeDataArray, 10^); >> %FILE%
    ) else if !mod! equ 2 (
        echo     CondFunc_%%i^(10^); >> %FILE%
    ) else if !mod! equ 3 (
        echo     StringFunc_%%i^(^); >> %FILE%
    ) else (
        echo     SwitchFunc_%%i^(1^); >> %FILE%
    )
)
echo } >> %FILE%
echo Done! %FILE% created.
pause
