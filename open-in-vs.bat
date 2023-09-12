@echo off
REM
REM Open Amalgam in VS IDE based off first script parameter:
REM
REM		1) Default (no args) : Visual Studio solution (CMake generated, "amd64-windows-vs" preset)
REM		2) vs_cmake          : Visual Studio directory (load from directory with CMake file)
REM		3) vscode            : VSCode directory (load from directory with CMake file)
REM		4) vs_static         : Visual Studio solution (local static non-CMake generated: Amalgam.sln)
REM

echo Setting up build tools...
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /F "tokens=*" %%g in ('%VSWHERE% -latest -property installationPath') do (set VS_INSTALL_PATH=%%g)
echo VS/BuildTools install path: %VS_INSTALL_PATH%
call "%VS_INSTALL_PATH%\VC\Auxiliary\Build\vcvars64.bat"

if "%1"=="" (

	if not exist "out/build/amd64-windows-vs" (

		echo CMake configure+generate Visual Studio solution...
		cmake -DUSE_OBJECT_LIBS=OFF --preset amd64-windows-vs
		if %ERRORLEVEL% GEQ 1 exit /B 1

		echo Fixing up generated Visual Studio projects...
		PowerShell -NoProfile -ExecutionPolicy Bypass -Command "& 'build/powershell/Fixup-Generated-VisualStudio-Projects.ps1'"
		if %ERRORLEVEL% GEQ 1 exit /B 1

	) else (
		echo CMake build dir already exists, not re-running CMake configure+generate
	)

	echo Opening generated Visual Studio solution...
	cmake --open out/build/amd64-windows-vs
	if %ERRORLEVEL% GEQ 1 exit /B 1

) else if "%1"=="vs_cmake" (

	echo Opening Visual Studio...
	devenv .
	if %ERRORLEVEL% GEQ 1 exit /B 1

) else if "%1"=="vscode" (

	echo Opening VSCode...
	code .
	if %ERRORLEVEL% GEQ 1 exit /B 1

) else if "%1"=="vs_static" (

	echo Opening Visual Studio w/ non-CMake Amalgam sln...
	devenv Amalgam.sln
	if %ERRORLEVEL% GEQ 1 exit /B 1

) else (

	echo Unknown arg for opening project: %1
	exit /b 1

)

exit /b 0