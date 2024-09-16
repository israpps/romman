@echo off
cd /d "%~dp0"
title PS2 ROM Rebuilder

set "ROM=rom.bin"
set "ROME=ext_rom.bin"
set "ROMN=new_rom.bin"
set "REBUILT=rebuilt_rom.bin"
set "ROMMAN=..\..\build\romman.exe"
set "SCRIPT=rebuild.romscript"
if not exist %ROM% (
    echo ERROR: %ROM% not found
    goto err
)
if not exist %ROMMAN% (
    echo ERROR: %ROMMAN% not found
    goto err
)
REM mkdir new_%ROM% 2>nul >nul

echo # Script created on '%DATE% - %TIME%' >%SCRIPT%
echo CreateImage^("%REBUILT%"^) >>%SCRIPT%

echo -- Extracting boot rom
%ROMMAN% --silent -x %ROM%
echo -- removing image stub
del %ROME%\- 2>nul >nul

setlocal EnableDelayedExpansion
echo Listing ROM Contents
echo # Original ROM Contents ^(without stubs^)>>%SCRIPT%
for /f "eol=# tokens=1,2,3,4" %%a in ('%ROMMAN% --silent -l %ROM%') do (
REM <Ignore RESET and Stubs>
    if %%b neq - (
        set "xxx=%%a"
        if "!xxx:~0,1!"=="f" (
            echo %%b            %%d
            if %%b neq RESET (
                echo AddFixedFile^("%ROME%/%%b", %%d^)>>%SCRIPT%
            ) else (
                echo AddFile^("%ROME%/%%b"^)>>%SCRIPT%
            )
        ) else (
            echo %%b
            echo AddFile^("%ROME%/%%b"^)>>%SCRIPT%
        )
    )
)
setlocal DisableDelayedExpansion
echo SortBySize^(^)>>%SCRIPT%
echo WriteImage^(^)>>%SCRIPT%

echo template script created. now modify the script or the files. when youre ready. hit continue to build the image!
pause
pause
timeout /t 2>nul

%ROMMAN% --silent -s %SCRIPT%

choice /m "print contents of new image?"

if errorlevel 1 (
%ROMMAN% --silent -l %REBUILT%
)

goto eof
:err
echo 
:eof
