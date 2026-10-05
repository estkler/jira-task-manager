param([string]$OutputName = 'Jira Task Manager.exe')
$ErrorActionPreference = 'Stop'
if ([IO.Path]::GetFileName($OutputName) -ne $OutputName -or -not $OutputName.EndsWith('.exe')) {
    throw 'OutputName must be an executable file name.'
}

$projectDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspaceDirectory = Split-Path -Parent $projectDirectory
$compilerDirectory = Join-Path $workspaceDirectory '.tools\zig-x86_64-windows-0.16.0'
$compiler = Join-Path $compilerDirectory 'zig.exe'
$outputDirectory = Join-Path $projectDirectory 'bin'
$globalCache = Join-Path $workspaceDirectory '.zig-cache-global'
$localCache = Join-Path $workspaceDirectory '.zig-cache-local'

if (-not (Test-Path -LiteralPath $compiler)) {
    throw 'Zig 0.16.0 is not available in the workspace tool directory.'
}

New-Item -ItemType Directory -Path $outputDirectory,$globalCache,$localCache -Force | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR = $globalCache
$env:ZIG_LOCAL_CACHE_DIR = $localCache

Push-Location $projectDirectory
try {
    & $compiler rc /nologo /fo 'bin\app.res' 'app.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }

    & $compiler cc -target x86_64-windows-gnu -Os -s -municode 'native.c' 'native_jira.c' 'native_notes.c' 'native_new_tasks.c' 'native_lifecycle.c' 'native_identity.c' 'bin\app.res' `
        -o (Join-Path 'bin' $OutputName) `
        '-Wl,--subsystem,windows' `
        -lcomctl32 -ldwmapi -lgdi32 -lshell32 -lshlwapi -luxtheme -lwinhttp -ladvapi32 -lcrypt32 -lwtsapi32 -lole32 -luuid
    if ($LASTEXITCODE -ne 0) { throw 'Native compilation failed.' }
}
finally {
    Pop-Location
}

$binary = Get-Item (Join-Path $outputDirectory $OutputName)
$version = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($binary.FullName)
[pscustomobject]@{
    Path = $binary.FullName
    Bytes = $binary.Length
    KiB = [math]::Round($binary.Length / 1KB, 1)
    ProductVersion = $version.ProductVersion
}
