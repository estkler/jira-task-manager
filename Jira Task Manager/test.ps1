$ErrorActionPreference = 'Stop'
$projectDirectory = $PSScriptRoot
$workspaceDirectory = Split-Path -Parent $projectDirectory
$compiler = Join-Path $workspaceDirectory '.tools\zig-x86_64-windows-0.16.0\zig.exe'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $workspaceDirectory '.zig-cache-global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $workspaceDirectory '.zig-cache-local'
New-Item -ItemType Directory -Path (Join-Path $projectDirectory 'bin'),$env:ZIG_GLOBAL_CACHE_DIR,$env:ZIG_LOCAL_CACHE_DIR -Force | Out-Null
Push-Location $projectDirectory
try {
    & $compiler cc -Os -fno-exceptions -fno-rtti -c 'native_svg.cpp' -o 'bin/native_svg.o'
    if ($LASTEXITCODE -ne 0) { throw 'OS-native SVG compilation failed.' }
    & $compiler rc /nologo /fo 'bin/update_test_fixture.res' 'update_test_fixture.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Update fixture resource compilation failed.' }
    & $compiler cc -municode 'update_test_fixture.c' 'native_update_model.c' 'native_update_files.c' 'native_update_apply.c' 'bin/update_test_fixture.res' -o 'bin/update_test_fixture.exe' '-Wl,--subsystem,windows' -lwinhttp -lshlwapi -lshell32 -ladvapi32 -lbcrypt
    if ($LASTEXITCODE -ne 0) { throw 'Update fixture compilation failed.' }
    & $compiler rc /nologo /DFIXTURE_PATCH=29 /fo 'bin/update_test_old.res' 'update_test_fixture.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Old fixture resource compilation failed.' }
    & $compiler cc -municode 'update_test_fixture.c' 'native_update_model.c' 'native_update_files.c' 'native_update_apply.c' 'bin/update_test_old.res' -o 'bin/update_test_old.exe' '-Wl,--subsystem,windows' -lwinhttp -lshlwapi -lshell32 -ladvapi32 -lbcrypt
    if ($LASTEXITCODE -ne 0) { throw 'Old fixture compilation failed.' }
    & $compiler rc /nologo /fo 'bin/activity_header_test.res' 'app.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Activity header fixture resource compilation failed.' }
    foreach ($testName in @('update_model_test','update_http_test','update_files_test','update_apply_test','native_json_test','time_input_test','workday_clock_test','identity_migration_test','lifecycle_log_test','notes_test','new_tasks_test','jira_model_test','jira_operations_test','status_color_test','density_test','flash_test','settings_test','refresh_interval_test','comment_style_test','comment_indicator_test','comment_attention_test','scope_icon_test','layout_test','activity_header_test','notification_settings_test','comment_alert_test','status_alert_test','status_write_test','comment_exclusion_test','comment_images_test','comment_media_ui_test','update_navigation_test','avatar_svg_test','avatar_cache_test','avatar_settings_test','notification_layout_test','switch_unavailable_test','tab_count_preferences_test','popover_refinement_test','bookmarks_model_test','bookmarks_ui_test','completion_ui_test')) {
        $sources = @("$testName.c",'native_bookmarks.c')
        if ($testName -eq 'completion_ui_test') { $sources += 'native_lifecycle.c','native_update_model.c','native_update_http.c','native_update_files.c','native_update_apply.c','bin/activity_header_test.res' }
        if ($testName -eq 'update_model_test') { $sources += 'native_update_model.c' }
        elseif ($testName -eq 'update_http_test') { $sources += 'native_update_model.c' }
        elseif ($testName -eq 'update_files_test') { $sources += 'native_update_model.c','native_update_files.c' }
        elseif ($testName -eq 'update_apply_test') { $sources += 'native_update_model.c','native_update_files.c','native_update_apply.c' }
        elseif ($testName -eq 'identity_migration_test') { $sources += 'native_identity.c' }
        elseif ($testName -eq 'lifecycle_log_test') { $sources += 'native_lifecycle.c','native_identity.c' }
        elseif ($testName -eq 'notes_test') { $sources += 'native_notes.c' }
        elseif ($testName -eq 'new_tasks_test') { $sources += 'native_new_tasks.c' }
        elseif ($testName -eq 'bookmarks_model_test') { }
        elseif ($testName -notin @('native_json_test','time_input_test','workday_clock_test')) {
            $sources += 'native_jira.c','native_images.c','native_avatar_cache.c','bin/native_svg.o','native_notes.c','native_new_tasks.c','native_identity.c'
        }
        if ($testName -in @('status_color_test','density_test','flash_test','settings_test','refresh_interval_test','comment_style_test','comment_indicator_test','comment_attention_test','scope_icon_test','layout_test','activity_header_test','notification_settings_test','comment_alert_test','status_alert_test','comment_media_ui_test','update_navigation_test','avatar_settings_test','notification_layout_test','switch_unavailable_test','tab_count_preferences_test','popover_refinement_test','bookmarks_ui_test')) {
            $sources += 'native_lifecycle.c','native_update_model.c','native_update_http.c','native_update_files.c','native_update_apply.c'
        }
        $unicodeEntry = @()
        if ($testName -in @('settings_test','comment_attention_test','layout_test')) { $sources += 'bin/update_test_old.res' }
        if ($testName -in @('activity_header_test','refresh_interval_test','notification_settings_test','comment_alert_test','status_alert_test','comment_media_ui_test','update_navigation_test','avatar_settings_test','notification_layout_test','switch_unavailable_test','tab_count_preferences_test','popover_refinement_test','bookmarks_ui_test')) { $sources += 'bin/activity_header_test.res' }
        if ($testName -in @('update_files_test','update_apply_test')) { $unicodeEntry += '-municode' }
        & $compiler cc -DUNICODE -D_UNICODE @unicodeEntry @sources -o "bin/$testName.exe" -lcomctl32 -ldwmapi -lgdi32 -lshell32 -lshlwapi -luxtheme -lwinhttp -ladvapi32 -lcrypt32 -lbcrypt -lwtsapi32 -lole32 -luuid -ld2d1 -ld3d11 -ldxgi -lxmllite
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
