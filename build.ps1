# ============================================================
#  libnexus - Build Script for Windows (PowerShell)
# ============================================================
[CmdletBinding()]
param(
    [switch]$DebugBuild,
    [switch]$Release,
    [string]$BuildDir = "build",
    [switch]$WithTests,
    [string]$VcpkgRoot = "",
    [string]$Generator = "",
    [int]$Jobs = 0,
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($Help) {
    Write-Host @"

 Usage: ./build.ps1 [OPTIONS]

 Options:
   -DebugBuild          Build in Debug mode
   -Release            Build in Release mode (default)
   -BuildDir <dir>     Output build directory (default: build)
   -WithTests          Enable unit tests (BUILD_TESTS=ON)
   -VcpkgRoot <path>   Path to vcpkg root (auto-detected if not set)
   -Generator <name>   CMake generator (e.g. "Ninja", "Visual Studio 17 2022")
   -Jobs <n>           Parallel build jobs
   -Help               Show this help message

"@
    exit 0
}

# --- Determine build type ---
$BuildType = if ($DebugBuild) { "Debug" } else { "Release" }

Write-Host ""
Write-Host "============================================================"
Write-Host " libnexus Build Script (Windows PowerShell)"
Write-Host " Configuration : $BuildType"
Write-Host " Build dir     : $BuildDir"
Write-Host " Tests         : $($WithTests.IsPresent)"
Write-Host "============================================================"
Write-Host ""

# --- Detect vcpkg ---
if (-not $VcpkgRoot) {
    if ($env:VCPKG_ROOT) {
        $VcpkgRoot = $env:VCPKG_ROOT
    } elseif (Test-Path "$env:USERPROFILE\vcpkg\vcpkg.exe") {
        $VcpkgRoot = "$env:USERPROFILE\vcpkg"
    } elseif (Test-Path "C:\vcpkg\vcpkg.exe") {
        $VcpkgRoot = "C:\vcpkg"
    } elseif (Test-Path "C:\dev\vcpkg\vcpkg.exe") {
        $VcpkgRoot = "C:\dev\vcpkg"
    }
}

$VcpkgToolchain = ""
if ($VcpkgRoot) {
    $VcpkgToolchain = "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
    Write-Host "[INFO] Using vcpkg at: $VcpkgRoot"
} else {
    Write-Host "[WARNING] vcpkg not found. Set -VcpkgRoot or VCPKG_ROOT environment variable."
    Write-Host "          Proceeding without vcpkg toolchain."
}

# --- Generator flag ---
$GeneratorFlag = @()
if ($Generator) {
    $GeneratorFlag = @("-G", $Generator)
}

# --- CMake options ---
$TestsFlag = if ($WithTests) { "ON" } else { "OFF" }

# --- Configure ---
Write-Host "[INFO] Running CMake configure..."
$cmakeArgs = @("-B", $BuildDir) + $GeneratorFlag
if ($VcpkgToolchain) { $cmakeArgs += $VcpkgToolchain }
$cmakeArgs += @(
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DBUILD_TESTS=$TestsFlag"
)
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[ERROR] CMake configure failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

# --- Build ---
Write-Host ""
Write-Host "[INFO] Building..."
$buildArgs = @("--build", $BuildDir, "--config", $BuildType)
if ($Jobs -gt 0) {
    $buildArgs += @("--", "/maxcpucount:$Jobs")
}
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[ERROR] Build failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

# --- Tests ---
if ($WithTests) {
    Write-Host ""
    Write-Host "[INFO] Running tests..."
    & ctest --test-dir $BuildDir -C $BuildType --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "[ERROR] Some tests failed." -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

Write-Host ""
Write-Host "[SUCCESS] Build completed successfully." -ForegroundColor Green
Write-Host "          Output in: $BuildDir\"
Write-Host ""
