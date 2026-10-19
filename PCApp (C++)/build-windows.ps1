[CmdletBinding()]
param(
    [string]$QtDir,
    [string]$VcpkgDir = (Join-Path $PSScriptRoot '.tools\vcpkg'),
    [switch]$SkipTests,
    [switch]$Launch,
    [switch]$PauseOnError
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
trap {
    Write-Host "`nBuild stopped: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'The remote-control app has not been built successfully. Fix the reported prerequisite or build error and run the launcher again.'
    if ($PauseOnError) { Read-Host 'Press Enter to close' | Out-Null }
    exit 1
}

function Find-JengaQtKit([string]$ProjectDirectory, [string]$Requested) {
    # No path is guessed as valid: both Qt deployment and CMake files must exist.
    $candidates = New-Object 'System.Collections.Generic.List[string]'
    if ($Requested) { $candidates.Add($Requested) }
    if ($env:QTDIR) { $candidates.Add($env:QTDIR) }
    foreach ($name in @('msvc2022_64','msvc2022 64','Qt\6.8.3\msvc2022_64')) {
        $candidates.Add((Join-Path $ProjectDirectory $name))
    }
    $candidates.Add((Join-Path $env:USERPROFILE 'Qt\6.8.3\msvc2022_64'))
    $candidates.Add('C:\Qt\6.8.3\msvc2022_64')
    foreach ($root in @($ProjectDirectory,(Join-Path $env:USERPROFILE 'Qt'),'C:\Qt')) {
        if (Test-Path -LiteralPath $root -PathType Container) {
            # The kit may have been copied beside the project under a different name.
            foreach ($child in (Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
                $candidates.Add($child.FullName)
                $candidates.Add((Join-Path $child.FullName 'msvc2022_64'))
            }
        }
    }
    foreach ($candidate in $candidates) {
        if ((Test-Path -LiteralPath (Join-Path $candidate 'bin\windeployqt.exe') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $candidate 'lib\cmake\Qt6\Qt6Config.cmake') -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'Qt MSVC 2022 x64 kit was not found. Put the msvc2022_64 folder beside this script, or leave it under your user Qt folder.'
}
$QtDir = Find-JengaQtKit $PSScriptRoot $QtDir
Write-Host "Qt detected: $QtDir" -ForegroundColor Cyan
function Run-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed (exit $LASTEXITCODE)." }
}
$QtDir = (Resolve-Path $QtDir).Path
if (!(Test-Path "$QtDir\bin\windeployqt.exe")) { throw 'QtDir must point to the 64-bit MSVC Qt kit, e.g. C:\Qt\6.8.3\msvc2022_64.' }

# Import MSVC tools into this process; no permanent PATH or system settings are changed.
if (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (!(Test-Path $vswhere)) { throw 'Install Visual Studio 2022 with Desktop development with C++, or its free Build Tools equivalent.' }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'The MSVC x64 C++ compiler is not installed.' }
    $dev = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
    $command = 'call "' + $dev + '" -no_logo -arch=x64 >nul && set'
    $environmentLines = & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialise the Visual Studio compiler environment.' }
    foreach ($line in $environmentLines) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) { [Environment]::SetEnvironmentVariable($line.Substring(0,$separator),$line.Substring($separator+1),'Process') }
    }
}
if (!(Get-Command cmake.exe -ErrorAction SilentlyContinue)) { throw 'CMake 3.24+ is required. Install the C++ CMake tools in Visual Studio or install CMake separately.' }
if (!(Get-Command git.exe -ErrorAction SilentlyContinue)) { throw 'Install Git for Windows, then rerun this script.' }
$baseline = '664933b8d385dfc01f4e0cdff4c7c74e71f6a740'
if (!(Test-Path "$VcpkgDir\bootstrap-vcpkg.bat")) {
    if (Test-Path $VcpkgDir) { throw 'The vcpkg destination exists but is not a valid checkout. Choose another VcpkgDir.' }
    New-Item -ItemType Directory -Force $VcpkgDir | Out-Null
    Run-Checked 'git.exe' @('-C',$VcpkgDir,'init')
    Run-Checked 'git.exe' @('-C',$VcpkgDir,'remote','add','origin','https://github.com/microsoft/vcpkg.git')
    Run-Checked 'git.exe' @('-C',$VcpkgDir,'fetch','--depth','1','origin',$baseline)
    Run-Checked 'git.exe' @('-C',$VcpkgDir,'checkout','--detach','FETCH_HEAD')
}
$VcpkgDir = (Resolve-Path $VcpkgDir).Path
if (!(Test-Path "$VcpkgDir\vcpkg.exe")) { Run-Checked "$VcpkgDir\bootstrap-vcpkg.bat" @('-disableMetrics') }
Run-Checked "$VcpkgDir\vcpkg.exe" @('install','--triplet','x64-windows',"--x-manifest-root=$PSScriptRoot","--x-install-root=$PSScriptRoot\vcpkg_installed",'--disable-metrics')
$env:PATH = "$QtDir\bin;$PSScriptRoot\vcpkg_installed\x64-windows\bin;$env:PATH"
Run-Checked 'cmake.exe' @('-S','.','-B','build-windows','-G','Visual Studio 17 2022','-A','x64',"-DCMAKE_PREFIX_PATH=$QtDir","-DCMAKE_TOOLCHAIN_FILE=$VcpkgDir\scripts\buildsystems\vcpkg.cmake",'-DVCPKG_TARGET_TRIPLET=x64-windows',"-DVCPKG_INSTALLED_DIR=$PSScriptRoot\vcpkg_installed",'-DBUILD_TESTING=ON')
Run-Checked 'cmake.exe' @('--build','build-windows','--config','Release','--parallel')
if (!$SkipTests) { Run-Checked 'ctest.exe' @('--test-dir','build-windows','-C','Release','--output-on-failure') }
$destination = Join-Path $PSScriptRoot 'dist\JengaRemote'
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item 'build-windows\Release\JengaRemote.exe' $destination -Force
Run-Checked "$QtDir\bin\windeployqt.exe" @('--release','--no-translations','--compiler-runtime','--dir',$destination,"$destination\JengaRemote.exe")
Get-ChildItem 'vcpkg_installed\x64-windows\bin\*.dll' | Copy-Item -Destination $destination -Force
Copy-Item 'README.md','SECURITY.md','THIRD_PARTY.md' $destination -Force
if (Test-Path 'vcpkg_installed\x64-windows\share\openssl\copyright') { Copy-Item 'vcpkg_installed\x64-windows\share\openssl\copyright' "$destination\OPENSSL-LICENSE.txt" -Force }
$qtLicense = Get-ChildItem "$QtDir\..\..\Licenses" -ErrorAction SilentlyContinue
if ($qtLicense) { Copy-Item "$QtDir\..\..\Licenses" "$destination\Qt-Licenses" -Recurse -Force }
Compress-Archive -Path "$destination\*" -DestinationPath 'dist\JengaRemote-Windows-x64.zip' -Force
Write-Host "`nReady: $destination\JengaRemote.exe" -ForegroundColor Green
Write-Host 'Keep the whole folder together. Java is not required.'
if ($Launch) { Start-Process -FilePath "$destination\JengaRemote.exe" -WorkingDirectory $destination }
