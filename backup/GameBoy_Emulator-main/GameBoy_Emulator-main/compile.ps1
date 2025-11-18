# Find Visual Studio
$vsPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
if (-not $vsPath) {
    Write-Host "Visual Studio not found"
    exit 1
}

# Setup environment
$env:Path = "$vsPath\VC\Tools\MSVC\14.*/bin/Hostx64/x64;$env:Path"
$env:Include = "$vsPath\VC\Tools\MSVC\14.*/include;$env:Include"
$env:Lib = "$vsPath\VC\Tools\MSVC\14.*/lib/x64;$env:Lib"

cd C:\Users\mariu\source\repos\GameBoy

# Compile (use msbuild; fallback to devenv if available)
$msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
if (Test-Path $msbuild) {
    & $msbuild GameBoy.sln /t:Build /p:Configuration=Debug /p:Platform=x64 | Select-Object -Last 40
} elseif (Get-Command devenv -ErrorAction SilentlyContinue) {
    & devenv GameBoy.sln /build "Debug|x64" /project GameBoy.vcxproj 2>&1 | Select-Object -Last 40
} else {
    Write-Host "Neither msbuild nor devenv found. Build cannot continue."; exit 1
}
