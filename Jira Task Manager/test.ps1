$ErrorActionPreference = 'Stop'
$projectDirectory = $PSScriptRoot
$workspaceDirectory = Split-Path -Parent $projectDirectory
$compiler = Join-Path $workspaceDirectory '.tools\zig-x86_64-windows-0.16.0\zig.exe'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $workspaceDirectory '.zig-cache-global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $workspaceDirectory '.zig-cache-local'
New-Item -ItemType Directory -Path (Join-Path $projectDirectory 'bin'),$env:ZIG_GLOBAL_CACHE_DIR,$env:ZIG_LOCAL_CACHE_DIR -Force | Out-Null
Push-Location $projectDirectory
try {
    & $compiler rc /nologo /fo 'bin/update_test_fixture.res' 'update_test_fixture.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Update fixture resource compilation failed.' }
    & $compiler cc -municode 'update_test_fixture.c' 'native_update_model.c' 'native_update_files.c' 'native_update_apply.c' 'bin/update_test_fixture.res' -o 'bin/update_test_fixture.exe' '-Wl,--subsystem,windows' -lwinhttp -lshlwapi -lshell32 -ladvapi32 -lbcrypt
    if ($LASTEXITCODE -ne 0) { throw 'Update fixture compilation failed.' }
    & $compiler rc /nologo /DFIXTURE_PATCH=29 /fo 'bin/update_test_old.res' 'update_test_fixture.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Old fixture resource compilation failed.' }
    & $compiler cc -municode 'update_test_fixture.c' 'native_update_model.c' 'native_update_files.c' 'native_update_apply.c' 'bin/update_test_old.res' -o 'bin/update_test_old.exe' '-Wl,--subsystem,windows' -lwinhttp -lshlwapi -lshell32 -ladvapi32 -lbcrypt
    if ($LASTEXITCODE -ne 0) { throw 'Old fixture compilation failed.' }
    foreach ($testName in @('update_model_test','update_http_test','update_files_test','update_apply_test','native_json_test','time_input_test','workday_clock_test','identity_migration_test','lifecycle_log_test','notes_test','new_tasks_test','jira_model_test','jira_operations_test','status_color_test','density_test','flash_test','settings_test')) {
        $sources = @("$testName.c")
        if ($testName -eq 'update_model_test') { $sources += 'native_update_model.c' }
        elseif ($testName -eq 'update_http_test') { $sources += 'native_update_model.c','native_update_http.c' }
        elseif ($testName -eq 'update_files_test') { $sources += 'native_update_model.c','native_update_files.c' }
        elseif ($testName -eq 'update_apply_test') { $sources += 'native_update_model.c','native_update_files.c','native_update_apply.c' }
        elseif ($testName -eq 'identity_migration_test') { $sources += 'native_identity.c' }
        elseif ($testName -eq 'lifecycle_log_test') { $sources += 'native_lifecycle.c','native_identity.c' }
        elseif ($testName -eq 'notes_test') { $sources += 'native_notes.c' }
        elseif ($testName -eq 'new_tasks_test') { $sources += 'native_new_tasks.c' }
        elseif ($testName -notin @('native_json_test','time_input_test','workday_clock_test')) {
            $sources += 'native_jira.c','native_notes.c','native_new_tasks.c','native_identity.c'
        }
        if ($testName -in @('status_color_test','density_test','flash_test','settings_test')) {
            $sources += 'native_lifecycle.c','native_update_model.c','native_update_files.c','native_update_apply.c'
        }
        $unicodeEntry = @()
        if ($testName -in @('update_files_test','update_apply_test')) { $unicodeEntry += '-municode' }
        & $compiler cc -DUNICODE -D_UNICODE @unicodeEntry @sources -o "bin/$testName.exe" -lcomctl32 -ldwmapi -lgdi32 -lshell32 -lshlwapi -luxtheme -lwinhttp -ladvapi32 -lcrypt32 -lbcrypt -lwtsapi32 -lole32 -luuid
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $testName" }
        if ($testName -eq 'update_files_test') {
            $fixture = (Resolve-Path 'bin/update_test_fixture.exe').Path
            $fixtureHash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
            & ".\bin\$testName.exe" $fixture $fixtureHash
        } elseif ($testName -eq 'update_apply_test') {
            $oldFixture = (Resolve-Path 'bin/update_test_old.exe').Path
            $newFixture = (Resolve-Path 'bin/update_test_fixture.exe').Path
            & ".\bin\$testName.exe" $oldFixture $newFixture (Get-FileHash -LiteralPath $oldFixture).Hash (Get-FileHash -LiteralPath $newFixture).Hash (Get-Item -LiteralPath $oldFixture).Length (Get-Item -LiteralPath $newFixture).Length
        } else { & ".\bin\$testName.exe" }
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $testName" }
    }
} finally { Pop-Location }
