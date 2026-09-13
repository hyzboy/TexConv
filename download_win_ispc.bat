@echo off
rem ===========================================================================
rem  Download and unpack ISPC (Intel SPMD Program Compiler, Windows x64) into
rem      ISPCTextureCompressor\ISPC\win\
rem
rem  Why: the custom build steps of ISPCTextureCompressor invoke
rem      ..\ISPC\win\ispc.exe        (see ispc_texcomp\ispc_texcomp.vcxproj)
rem  to compile kernel*.ispc into kernel*.obj, and the upstream repository
rem  ships neither the compiler nor the generated kernels. Without them
rem  TexConv's Intel encoder stays disabled - see the auto-detect block in
rem  TexConv\CMakeLists.txt (it looks for x64\{Debug,Release}\kernel.obj).
rem
rem  The archive keeps ispc.exe next to ispc.dll / ispcrt*.dll, so the whole
rem  bin\ folder is installed - ispc.exe does not run without those DLLs.
rem
rem  Download strategy (flaky links are the normal case here):
rem    1. BITS  - Start-BitsTransfer, the Windows transfer service. It retries
rem               and resumes on its own inside a job, which is far more
rem               reliable than a bare curl over a lossy connection.
rem    2. curl  - fallback if BITS is unavailable/blocked; resumes with -C -,
rem               retries, and gives up on a stalled link (speed-limit/Time).
rem    3. After every attempt the archive is verified with tar -t (0.1 s).
rem       A truncated or corrupt file is discarded and fetched again instead of
rem       blowing up later during unpacking.
rem
rem  Usage:
rem      download_win_ispc.bat                : default trunk build (x64)
rem      download_win_ispc.bat <url>          : use another archive/mirror
rem      download_win_ispc.bat --keep         : keep the downloaded zip
rem      set ISPC_DEST_DIR=D:\somewhere ^&^& download_win_ispc.bat
rem                                           : install somewhere else
rem
rem  Notes:
rem    * This file is ASCII-only on purpose: cmd.exe would garble comments
rem      written in non-ANSI encodings.
rem    * An already downloaded zip (in the target dir or next to this script)
rem      is reused after a quick integrity check - put one there by hand if you
rem      prefer a download manager for the ~164 MB archive.
rem ===========================================================================

setlocal EnableExtensions

set "DEFAULT_URL=https://github.com/ispc/ispc/releases/download/trunk-artifacts/ispc-trunk-windows.zip"
set "MAX_RETRY=5"

set "URL=%DEFAULT_URL%"
set "KEEP=0"

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="--keep" goto opt_keep
if /i "%~1"=="-h" goto help
if /i "%~1"=="--help" goto help
set "URL=%~1"
shift
goto parse

:opt_keep
set "KEEP=1"
shift
goto parse

:parsed
set "SCRIPT_DIR=%~dp0"

if defined ISPC_DEST_DIR (
    set "DEST=%ISPC_DEST_DIR%"
) else (
    set "DEST=%SCRIPT_DIR%ISPCTextureCompressor\ISPC\win"
)

for %%I in ("%URL%") do set "ZIP_NAME=%%~nxI"
set "ZIP=%DEST%\%ZIP_NAME%"
set "PART=%ZIP%.part"
set "BSDTAR=%SystemRoot%\System32\tar.exe"

echo URL       : %URL%
echo Target dir: %DEST%

if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 goto err_mkdir

rem ---------------------------------------------------------------------------
rem  Already have a usable archive? (target dir first, then next to this script)
rem ---------------------------------------------------------------------------
if exist "%ZIP%" (
    call :archive_ok "%ZIP%"
    if not errorlevel 1 (
        echo Reusing existing archive: "%ZIP%"
        goto have_zip
    )
    echo Existing archive is incomplete - fetching again.
    del /q "%ZIP%" 2>nul
)

if exist "%SCRIPT_DIR%%ZIP_NAME%" (
    call :archive_ok "%SCRIPT_DIR%%ZIP_NAME%"
    if not errorlevel 1 (
        echo Reusing archive next to this script: "%SCRIPT_DIR%%ZIP_NAME%"
        set "ZIP=%SCRIPT_DIR%%ZIP_NAME%"
        set "PART=%ZIP%.part"
        goto have_zip
    )
)

rem ---------------------------------------------------------------------------
rem  Fetch it
rem ---------------------------------------------------------------------------
set /a ATTEMPT=0
set "RESUME=-C -"

:download
set /a ATTEMPT+=1
echo Fetching archive (attempt %ATTEMPT%/%MAX_RETRY%) ...

rem 1) BITS first: it manages retries and resumes inside its own job. It cannot
rem    continue a foreign partial file, so start it from a clean slate.
if exist "%PART%" del /q "%PART%" 2>nul
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-BitsTransfer -Source '%URL%' -Destination '%PART%' -ErrorAction Stop" >nul 2>&1
if not errorlevel 1 goto check_archive

echo BITS transfer failed - falling back to curl ...

rem 2) curl fallback, keeping the partial file so -C - can resume it
curl.exe -fL %RESUME% --connect-timeout 20 --retry 3 --retry-delay 2 --speed-limit 1024 --speed-time 30 -o "%PART%" "%URL%"
if %ERRORLEVEL%==0 goto check_archive
if %ERRORLEVEL%==33 goto no_resume
echo curl exited with %ERRORLEVEL%.
goto retry

:no_resume
rem CURLE_RANGE_ERROR: server cannot resume. Never keep a partial file around
rem as if it were complete - restart this attempt from scratch.
echo Server does not support resume - restarting from scratch ...
del /q "%PART%" 2>nul
set "RESUME="
goto retry

:check_archive
rem Verify before trusting it: a truncated zip must be caught here, not later
if not exist "%PART%" goto retry
call :archive_ok "%PART%"
if not errorlevel 1 goto got_archive
echo Archive is incomplete or corrupt - discarding it and retrying ...
del /q "%PART%" 2>nul

:retry
if %ATTEMPT% GEQ %MAX_RETRY% goto err_download
call :sleep 3
goto download

:got_archive
move /y "%PART%" "%ZIP%" >nul
echo Downloaded: "%ZIP%"

:have_zip
if not exist "%ZIP%" goto err_nofile
call :archive_ok "%ZIP%"
if errorlevel 1 goto err_badzip

rem ---------------------------------------------------------------------------
rem  Unpack
rem ---------------------------------------------------------------------------
set "UNPACK=%TEMP%\ispc_unpack_%RANDOM%%RANDOM%"
mkdir "%UNPACK%" 2>nul

echo Unpacking ...
"%BSDTAR%" -xf "%ZIP%" -C "%UNPACK%" 2>nul
if not errorlevel 1 goto unpacked

echo Built-in tar failed, trying PowerShell Expand-Archive ...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath '%ZIP%' -DestinationPath '%UNPACK%' -Force"
if errorlevel 1 goto err_unpack

:unpacked
set "FOUND="
for /f "delims=" %%F in ('dir /b /s "%UNPACK%\ispc.exe" 2^>nul') do (
    if not defined FOUND set "FOUND=%%F"
)
if not defined FOUND goto err_noispc

for %%F in ("%FOUND%") do set "SRC_BIN=%%~dpF"
echo Installing from "%SRC_BIN%" ...

copy /y "%SRC_BIN%*" "%DEST%" >nul
if errorlevel 1 goto err_copy

echo.
echo Installed: "%DEST%\ispc.exe"
dir /b "%DEST%"

rem ---------------------------------------------------------------------------
rem  Verify
rem ---------------------------------------------------------------------------
for /f "delims=" %%V in ('"%DEST%\ispc.exe" --version 2^>nul') do (
    echo Version   : %%V
    goto version_ok
)
rmdir /s /q "%UNPACK%" 2>nul
echo ERROR: ispc.exe is in place but did not run.
echo        Check that ispc.dll and ispcrt*.dll are next to it.
exit /b 1

:version_ok
rem ---------------------------------------------------------------------------
rem  Cleanup
rem ---------------------------------------------------------------------------
rmdir /s /q "%UNPACK%" 2>nul

if "%KEEP%"=="1" (
    echo Kept archive: "%ZIP%"
) else (
    if not "%ZIP%"=="%SCRIPT_DIR%%ZIP_NAME%" del /q "%ZIP%" 2>nul
)

echo.
echo Next step - generate the kernels (ispc_texcomp.vcxproj does this through
echo ISPC\win\ispc.exe), then re-run cmake configure for TexConv:
echo     msbuild "ISPCTextureCompressor\ispc_texcomp\ispc_texcomp.vcxproj" /p:Configuration=Release /p:Platform=x64
echo TexConv\CMakeLists.txt then finds x64\Release\kernel.obj and enables the
echo Intel ISPC encoder automatically.
exit /b 0

rem ---------------------------------------------------------------------------
rem  Helpers
rem ---------------------------------------------------------------------------

rem :archive_ok <file>  -> errorlevel 0 when tar can list it (0.1 s for 164 MB).
rem Drops a truncated/corrupt archive instead of failing much later.
:archive_ok
if not exist "%~1" exit /b 1
"%BSDTAR%" -tf "%~1" >nul 2>&1
if errorlevel 1 exit /b 1
exit /b 0

rem :sleep <seconds>  (timeout needs a console; ping always works)
:sleep
timeout /t %~1 /nobreak >nul 2>&1
if errorlevel 1 ping -n %~1 127.0.0.1 >nul 2>&1
exit /b 0

rem ---------------------------------------------------------------------------
rem  Errors
rem ---------------------------------------------------------------------------
:help
echo Usage: %~nx0 [url] [--keep]
echo   url      archive to download (default: ISPC trunk, Windows x64)
echo   --keep   keep the downloaded zip
echo Env: ISPC_DEST_DIR  install directory (default: ISPCTextureCompressor\ISPC\win)
exit /b 0

:err_mkdir
echo ERROR: cannot create "%DEST%"
exit /b 1

:err_nofile
echo ERROR: archive not found: "%ZIP%"
exit /b 1

:err_badzip
echo ERROR: "%ZIP%" is not a readable zip - download it again or delete it.
exit /b 1

:err_download
echo ERROR: download failed after %MAX_RETRY% attempts.
echo        Partial file kept: "%PART%"
echo        Run this file again to resume, or fetch the archive with a
echo        download manager and drop it next to this script (it is reused).
exit /b 1

:err_unpack
rmdir /s /q "%UNPACK%" 2>nul
echo ERROR: cannot unpack "%ZIP%" (tried built-in tar and Expand-Archive)
exit /b 1

:err_noispc
rmdir /s /q "%UNPACK%" 2>nul
echo ERROR: no ispc.exe inside "%ZIP%"
exit /b 1

:err_copy
rmdir /s /q "%UNPACK%" 2>nul
echo ERROR: cannot copy from "%SRC_BIN%" to "%DEST%"
exit /b 1
