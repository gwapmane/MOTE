@echo off
title MOTE installer
net session >nul 2>&1
if %errorlevel% neq 0 (
  echo Windows will ask for permission to install MOTE. Click Yes.
  powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)
set "SRC=%~dp0MOTE.vst3"
set "DEST=%CommonProgramFiles%\VST3"
if not exist "%SRC%" (
  echo Could not find MOTE.vst3 next to this file. Unzip the whole download first, then run this again.
  pause
  exit /b 1
)
if not exist "%DEST%" mkdir "%DEST%"
if exist "%DEST%\MOTE.vst3" rmdir /s /q "%DEST%\MOTE.vst3"
xcopy "%SRC%" "%DEST%\MOTE.vst3\" /E /I /Y /Q >nul
if errorlevel 1 (
  echo Install failed. Close FL Studio and try again.
  pause
  exit /b 1
)
echo.
echo MOTE is installed in:  %DEST%\MOTE.vst3
echo.
echo Now open FL Studio:
echo   Options ^> Manage plugins ^> Find installed plugins
echo   (tick "Verify plugins" if you see it, then click the scan button)
echo MOTE is an INSTRUMENT, so look for it in the plugin picker under Installed ^> Generators (it may also appear under "VST3").
echo.
pause
