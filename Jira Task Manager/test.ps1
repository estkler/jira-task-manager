$ErrorActionPreference = 'Stop'
$projectDirectory = $PSScriptRoot
$workspaceDirectory = Split-Path -Parent $projectDirectory
$compiler = Join-Path $workspaceDirectory '.tools\zig-x86_64-windows-0.16.0\zig.exe'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $workspaceDirectory '.zig-cache-global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $workspaceDirectory '.zig-cache-local'
New-Item -ItemType Directory -Path (Join-Path $projectDirectory 'bin'),$env:ZIG_GLOBAL_CACHE_DIR,$env:ZIG_LOCAL_CACHE_DIR -Force | Out-Null
Push-Location $projectDirectory
try {
    foreach ($testName in @('update_model_test','native_json_test','time_input_test','workday_clock_test','identity_migration_test','lifecycle_log_test','notes_test','new_tasks_test','jira_model_test','jira_operations_test','status_color_test','density_test','flash_test','settings_test')) {
        $sources = @("$testName.c")
        if ($testName -eq 'update_model_test') { $sources += 'native_update_model.c' }
        elseif ($testName -eq 'identity_migration_test') { $sources += 'native_identity.c' }
        elseif ($testName -eq 'lifecycle_log_test') { $sources += 'native_lifecycle.c','native_identity.c' }
        elseif ($testName -eq 'notes_test') { $sources += 'native_notes.c' }
        elseif ($testName -eq 'new_tasks_test') { $sources += 'native_new_tasks.c' }
        elseif ($testName -notin @('native_json_test','time_input_test','workday_clock_test')) {
            $sources += 'native_jira.c','native_notes.c','native_new_tasks.c','native_identity.c'
        }
        if ($testName -in @('status_color_test','density_test','flash_test','settings_test')) {
            $sources += 'native_lifecycle.c'
        }
        & $compiler cc -DUNICODE -D_UNICODE @sources -o "bin/$testName.exe" -lcomctl32 -ldwmapi -lgdi32 -lshell32 -lshlwapi -luxtheme -lwinhttp -ladvapi32 -lcrypt32 -lwtsapi32 -lole32 -luuid
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $testName" }
        & ".\bin\$testName.exe"
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $testName" }
    }
} finally { Pop-Location }
