call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86 

:compile
bam server_release

echo.
set /p again=Compile again? press Enter

if "%again%" == "" goto compile