param(
    [string]$CacheRoot,
    [string]$CMakePath,
    [string]$NinjaPath,
    [string]$OpenSSLRoot,
    [int]$Jobs = 8
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $CacheRoot) {
    $CacheRoot = Join-Path $repoRoot '.build\win64-deps'
}
$CacheRoot = [System.IO.Path]::GetFullPath($CacheRoot)
if (-not $OpenSSLRoot) {
    $OpenSSLRoot = Join-Path $repoRoot '..\..\toolchain\openssl\x86_64-win64'
}
$OpenSSLRoot = [System.IO.Path]::GetFullPath($OpenSSLRoot)
foreach ($required in @('include\openssl\opensslv.h', 'lib\libcrypto.lib', 'lib\libssl.lib')) {
    if (-not (Test-Path -LiteralPath (Join-Path $OpenSSLRoot $required) -PathType Leaf)) {
        throw "Missing prebuilt OpenSSL 3 file: $OpenSSLRoot\$required. Install the developer package using network/openssl/README.md."
    }
}

function Find-Tool {
    param([string]$Name, [string]$Path)
    if ($Path) {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
            throw "$Name not found: $Path"
        }
        return (Resolve-Path -LiteralPath $Path).Path
    }
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "$Name is required. Put it on PATH or supply its path to this script."
    }
    return $command.Source
}

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments, [string]$Directory)
    Push-Location -LiteralPath $Directory
    try {
        & $Program @Arguments | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "$Program failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }
}

function Get-PinnedSource {
    param([string]$Name, [string]$Url, [string]$Revision)
    $source = Join-Path $CacheRoot $Name
    if (-not (Test-Path -LiteralPath $source)) {
        New-Item -ItemType Directory -Path $source | Out-Null
        Invoke-Checked $git @('init', $source) $CacheRoot
        Invoke-Checked $git @('-C', $source, 'remote', 'add', 'origin', $Url) $CacheRoot
        Invoke-Checked $git @('-C', $source, 'fetch', '--depth=1', 'origin', $Revision) $CacheRoot
        Invoke-Checked $git @('-C', $source, 'checkout', '--detach', 'FETCH_HEAD') $CacheRoot
    }
    if (-not (Test-Path -LiteralPath (Join-Path $source '.git'))) {
        throw "Not a source checkout: $source"
    }
    $actual = (& $git -C $source rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $actual -ne $Revision) {
        throw "$Name must be at $Revision; found $actual in $source"
    }
    return $source
}

function Copy-RequiredFile {
    param([string]$Source, [string]$Destination)
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Missing build output: $Source"
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

if ($Jobs -lt 1) { throw 'Jobs must be at least 1.' }
$git = (Get-Command git -ErrorAction Stop).Source
$cmake = Find-Tool 'cmake.exe' $CMakePath
$ninja = Find-Tool 'ninja.exe' $NinjaPath
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio vswhere.exe is required.' }
$vsRoot = (& $vswhere -latest -products '*' -property installationPath | Select-Object -First 1)
if (-not $vsRoot) { throw 'Visual Studio with the v142 C++ toolset is required.' }
$vc = Get-ChildItem -LiteralPath (Join-Path $vsRoot 'VC\Tools\MSVC') -Directory |
    Where-Object Name -Like '14.29.*' | Sort-Object Name -Descending | Select-Object -First 1
if (-not $vc) { throw 'The Visual C++ v142 (14.29) toolset is required.' }
$vcBin = Join-Path $vc.FullName 'bin\Hostx64\x64'
$cl = Join-Path $vcBin 'cl.exe'
$libTool = Join-Path $vcBin 'lib.exe'
$msbuild = Join-Path $vsRoot 'MSBuild\Current\Bin\amd64\MSBuild.exe'
$sdkRoot = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots' -Name KitsRoot10).KitsRoot10
$sdkVersion = '10.0.19041.0'
$sdkBin = Join-Path $sdkRoot "bin\$sdkVersion\x64"
foreach ($required in @($cl, $libTool, $msbuild, (Join-Path $sdkBin 'rc.exe'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing build tool: $required" }
}
$env:PATH = "$vcBin;$sdkBin;$(Split-Path $ninja);$env:PATH"
$env:INCLUDE = "$(Join-Path $vc.FullName 'include');$(Join-Path $sdkRoot "Include\$sdkVersion\ucrt");$(Join-Path $sdkRoot "Include\$sdkVersion\um");$(Join-Path $sdkRoot "Include\$sdkVersion\shared")"
$env:LIB = "$(Join-Path $vc.FullName 'lib\x64');$(Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64");$(Join-Path $sdkRoot "Lib\$sdkVersion\um\x64")"
New-Item -ItemType Directory -Path $CacheRoot -Force | Out-Null

$shaderc = Get-PinnedSource 'shaderc' 'https://github.com/google/shaderc.git' '24275a11d81a6b33ef345878f8a4ef929c95a116'
$shaderThirdParty = Join-Path $shaderc 'third_party'
New-Item -ItemType Directory -Path $shaderThirdParty -Force | Out-Null
$null = Get-PinnedSource 'shaderc\third_party\glslang' 'https://github.com/KhronosGroup/glslang.git' 'e56beaee736863ce48455955158f1839e6e4c1a1'
$null = Get-PinnedSource 'shaderc\third_party\spirv-headers' 'https://github.com/KhronosGroup/SPIRV-Headers.git' 'a3fdfe81465d57efc97cfd28ac6c8190fb31a6c8'
$null = Get-PinnedSource 'shaderc\third_party\spirv-tools' 'https://github.com/KhronosGroup/SPIRV-Tools.git' 'ef3290bbea35935ba8fd623970511ed9f045bbd7'
$spirvCross = Get-PinnedSource 'spirv-cross' 'https://github.com/KhronosGroup/SPIRV-Cross.git' 'cf1e9e0643eba07535f0015e68fd95bfe3cf1e73'
$libevent = Get-PinnedSource 'libevent' 'https://github.com/libevent/libevent.git' '79ddfb460847999b807cba76d04e73891f29c6ee'
$mdns = Get-PinnedSource 'mdnsresponder' 'https://github.com/apple-oss-distributions/mDNSResponder.git' 'f783506af3836b39b83fc14115bc2728a49db4b2'

$shaderBuild = Join-Path $CacheRoot 'shaderc-build'
Invoke-Checked $cmake @('-S', $shaderc, '-B', $shaderBuild, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
    "-DCMAKE_C_COMPILER=$cl", "-DCMAKE_CXX_COMPILER=$cl", "-DCMAKE_MAKE_PROGRAM=$ninja",
    '-DCMAKE_C_FLAGS_RELEASE=/MD /O2 /Ob2 /DNDEBUG',
    '-DCMAKE_CXX_FLAGS_RELEASE=/MD /O2 /Ob2 /DNDEBUG',
    '-DLLVM_USE_CRT_RELEASE=MD',
    '-DSHADERC_SKIP_TESTS=ON', '-DSHADERC_SKIP_EXAMPLES=ON', '-DSHADERC_SKIP_INSTALL=ON',
    '-DSHADERC_ENABLE_SHARED_CRT=ON',
    '-DSHADERC_ENABLE_WERROR_COMPILE=OFF', '-DENABLE_GLSLANG_BINARIES=OFF',
    '-DSPIRV_SKIP_EXECUTABLES=ON', '-DSPIRV_SKIP_TESTS=ON') $CacheRoot
Invoke-Checked $cmake @('--build', $shaderBuild, '--target', 'libshaderc/shaderc_combined.lib', '--parallel', "$Jobs") $CacheRoot

$spirvBuild = Join-Path $CacheRoot 'spirv-cross-build'
Invoke-Checked $cmake @('-S', $spirvCross, '-B', $spirvBuild, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
    "-DCMAKE_C_COMPILER=$cl", "-DCMAKE_CXX_COMPILER=$cl", "-DCMAKE_MAKE_PROGRAM=$ninja",
    '-DSPIRV_CROSS_CLI=OFF', '-DSPIRV_CROSS_STATIC=ON', '-DSPIRV_CROSS_ENABLE_GLSL=ON',
    '-DSPIRV_CROSS_ENABLE_HLSL=OFF', '-DSPIRV_CROSS_ENABLE_MSL=OFF',
    '-DSPIRV_CROSS_ENABLE_CPP=OFF', '-DSPIRV_CROSS_ENABLE_C_API=OFF',
    '-DSPIRV_CROSS_ENABLE_REFLECT=OFF', '-DSPIRV_CROSS_ENABLE_TESTS=OFF',
    '-DSPIRV_CROSS_ENABLE_UTIL=OFF') $CacheRoot
Invoke-Checked $cmake @('--build', $spirvBuild, '--target', 'spirv-cross-core', 'spirv-cross-glsl', '--parallel', "$Jobs") $CacheRoot

$eventBuild = Join-Path $CacheRoot 'libevent-build'
Invoke-Checked $cmake @('-S', $libevent, '-B', $eventBuild, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
    "-DCMAKE_C_COMPILER=$cl", "-DCMAKE_MAKE_PROGRAM=$ninja",
    '-DEVENT__LIBRARY_TYPE=STATIC', '-DEVENT__MSVC_STATIC_RUNTIME=ON',
    '-DEVENT__DISABLE_TESTS=ON', '-DEVENT__DISABLE_BENCHMARK=ON',
    '-DEVENT__DISABLE_SAMPLES=ON', "-DOPENSSL_ROOT_DIR=$OpenSSLRoot",
    "-DOPENSSL_INCLUDE_DIR=$(Join-Path $OpenSSLRoot 'include')",
    "-DOPENSSL_CRYPTO_LIBRARY=$(Join-Path $OpenSSLRoot 'lib\libcrypto.lib')",
    "-DOPENSSL_SSL_LIBRARY=$(Join-Path $OpenSSLRoot 'lib\libssl.lib')") $CacheRoot
Invoke-Checked $cmake @('--build', $eventBuild, '--parallel', "$Jobs") $CacheRoot

$dnsBuild = Join-Path $CacheRoot 'dnssd-build'
New-Item -ItemType Directory -Path $dnsBuild -Force | Out-Null
$dnsObject = Join-Path $dnsBuild 'DLLStub.obj'
$dnsLibrary = Join-Path $dnsBuild 'dnssd.lib'
Invoke-Checked $cl @('/nologo', '/c', '/O2', '/MT', '/EHsc', '/DWIN32', '/DNDEBUG',
    '/D_LIB', '/DWIN32_LEAN_AND_MEAN', '/D_CRT_SECURE_NO_DEPRECATE',
    "/I$(Join-Path $mdns 'mDNSShared')", "/I$(Join-Path $mdns 'mDNSWindows')",
    "/Fo$dnsObject", (Join-Path $mdns 'mDNSWindows\DLLStub\DLLStub.cpp')) $dnsBuild
Invoke-Checked $libTool @('/NOLOGO', "/OUT:$dnsLibrary", $dnsObject) $dnsBuild

$vulkanLib = Join-Path $repoRoot 'external\vulkan\Lib64'
$liveRoot = Join-Path $repoRoot 'external\live-libs64'
$liveInclude = Join-Path $liveRoot 'include'
$liveLib = Join-Path $liveRoot 'lib'
New-Item -ItemType Directory -Path $vulkanLib, $liveLib,
    (Join-Path $liveInclude 'openssl'), (Join-Path $liveInclude 'event2') -Force | Out-Null
Copy-RequiredFile (Join-Path $shaderBuild 'libshaderc\shaderc_combined.lib') (Join-Path $vulkanLib 'shaderc_combined.lib')
foreach ($name in @('spirv-cross-core.lib', 'spirv-cross-glsl.lib')) {
    Copy-RequiredFile (Join-Path $spirvBuild $name) (Join-Path $vulkanLib $name)
}
foreach ($name in @('libcrypto.lib', 'libssl.lib')) {
    Copy-RequiredFile (Join-Path $OpenSSLRoot "lib\$name") (Join-Path $liveLib $name)
}
foreach ($name in @('event_core.lib', 'event_extra.lib', 'event_openssl.lib')) {
    Copy-RequiredFile (Join-Path $eventBuild "lib\$name") (Join-Path $liveLib $name)
}
Copy-RequiredFile $dnsLibrary (Join-Path $liveLib 'dnssd.lib')
Copy-Item (Join-Path $OpenSSLRoot 'include\openssl\*') (Join-Path $liveInclude 'openssl') -Recurse -Force
Copy-Item (Join-Path $libevent 'include\event2\*') (Join-Path $liveInclude 'event2') -Recurse -Force
Copy-RequiredFile (Join-Path $eventBuild 'include\event2\event-config.h') (Join-Path $liveInclude 'event2\event-config.h')
Copy-RequiredFile (Join-Path $mdns 'mDNSShared\dns_sd.h') (Join-Path $liveInclude 'dns_sd.h')
Write-Output "Built and staged x64 dependencies in $vulkanLib and $liveRoot"
