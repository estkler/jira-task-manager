# Native Updates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Добавить работающую ручную проверку и подтверждаемую установку обновлений Jira Task Manager без потери пользовательских данных.

**Architecture:** Независимые C-модули разбирают релиз, выполняют ограниченный HTTPS-запрос, проверяют EXE и заменяют его в отдельном режиме того же приложения. Фоновый работник передаёт результаты основному окну; окно настроек только отображает состояние. Установка начинается после повторной проверки всех черновиков и штатного завершения приложения.

**Tech Stack:** C, Win32, WinHTTP, Windows Cryptography API, существующий native_json.h, Zig 0.16.0; никаких сторонних библиотек.

**Spec:** [2026-10-05-native-updates-design.md](../specs/2026-10-05-native-updates-design.md) — согласована пользователем 2026-10-05.

## Global Constraints

- Канал: публичный `estkler/jira-task-manager`, только стабильные GitHub Releases.
- Asset-name: `Jira.Task.Manager.exe`; установленный файл: `Jira Task Manager.exe`.
- Проверка запускается вручную; в первом выпуске нет фоновых автопроверок.
- Нужны корректные размер и `digest` вида `sha256:` плюс 64 hex-символа. Если digest не предоставлен, автоматическая установка запрещена.
- Размер EXE ограничен 16 MiB, ответа JSON — 1 MiB.
- Только HTTPS с проверкой сертификатов; разрешены `api.github.com`, `github.com`, `release-assets.githubusercontent.com` и `objects.githubusercontent.com`.
- Обновляется только EXE приложения в его текущей папке. Не переименовываются пользовательское хранилище, ключ Windows Credential Manager и данные таймера.
- Ничего не устанавливается автоматически без подтверждения; отсутствуют автоматическое повышение прав и обход предупреждений Windows.
- Установка запрещена при Jira job, несохранённой заметке, непустом черновике Jira комментария или открытом окне завершения задачи.
- Работать в Git-копии `.publish/jira-task-manager`; исходники находятся в `Jira Task Manager/`. Не публиковать bin, диагностику, настройки, токены и ярлыки.
- Для первой реализации версия 0.9.30 во всех ресурсах и APP_VERSION; не менять логотип, список задач или переключатель режимов.

## Review Focus

1. Неизвестное поле JSON, дублирующее критическое поле или усечённая строка не должны превращать плохой релиз в допустимый (Task 1).
2. Redirect с userinfo, другим портом, суффиксом разрешённого хоста или HTTP не должен обходить проверку адреса (Task 2).
3. Файл, изменённый после скачивания, или временная папка через reparse point не должны приводить к запуску непроверенных байтов (Tasks 3–4).
4. PID, повторно использованный другим процессом, поздний результат работника и повторный клик не должны закрывать чужое окно или запускать вторую установку (Tasks 4–5).
5. Отсоединённая панель, ошибка автосохранения заметки или несохранённые изменения настроек должны остановить установку до выхода, сохранив введённое (Task 5).

---

## Files and boundaries

- Create `Jira Task Manager/native_update.h`: общие типы версии, релиза, результата, отмены и опубликованные интерфейсы ниже; без Jira-зависимостей.
- Create `native_update_model.c`: строгие версии, метаданные релиза, политика URL.
- Create `native_update_http.c`: ограниченный HTTPS transport и скачивание; независим от HWND.
- Create `native_update_files.c`: приватный staging-каталог, hash/PE/version-проверка и точечная очистка собственных файлов.
- Create `native_update_apply.c`: проверка параметров помощника, ожидание родителя, замена/rollback/restart.
- Create `native_update_ui.h`: интеграция работника и состояния с native.c/native_settings.h, по существующей модели включаемого UI-header.
- Modify `native.c`, `native_settings.h`, `build.ps1`, `test.ps1`, `app.rc`, `app.manifest`, README; минимальная интеграция без общего рефакторинга.
- Create `update_model_test.c`, `update_http_test.c`, `update_files_test.c`, `update_apply_test.c`, `update_test_fixture.c`; extend `settings_test.c`.

Все пути в задачах ниже относительны к Git-корню, если не указан префикс, — к `Jira Task Manager/`.

### Task 1: Release model and version comparison

**Files:** Create `native_update.h`, `native_update_model.c`, `update_model_test.c`; modify `test.ps1`.

**Interfaces:**
- `UpdateVersion { DWORD major, minor, patch; }`.
- `UpdateRelease { UpdateVersion version; ULONGLONG asset_id; DWORD size; BYTE sha256[32]; }`; не содержит произвольный URL или release notes.
- `UpdateStatus`: `UPDATE_OK`, `UPDATE_NONE`, `UPDATE_CANCELLED`, `UPDATE_INVALID`, `UPDATE_NETWORK`, `UPDATE_IO`, `UPDATE_BUSY`.
- `BOOL UpdateParseVersion(const wchar_t *text, UpdateVersion *out)`; `int UpdateCompareVersion(UpdateVersion left, UpdateVersion right)`.
- `UpdateStatus UpdateParseRelease(const char *json, size_t length, UpdateVersion current, UpdateRelease *out)`.
- `BOOL UpdateUrlAllowed(const wchar_t *url)`; строгие HTTPS, порт 443/default, без userinfo; точное case-insensitive совпадение хоста.

- [ ] **Step 1: Write failing model tests.** Assert `v0.9.30` equals `0.9.30`, `0.10.0 > 0.9.99`; reject suffix, missing parts, DWORD overflow and negative parts. Release fixtures accept one exact-name asset with positive numeric ID, size 1..16777216 and 32-byte digest; reject duplicates of critical fields, duplicate assets, malformed flags/types, draft/prerelease, missing digest, escaped NUL and JSON >1048576 bytes. Equal/older valid releases return UPDATE_NONE.

  Name the cases `test_version_order`, `test_release_valid`, `test_release_rejected`, `test_url_policy`; representative exact assertions:
  ```c
  UpdateVersion current, next;
  assert(UpdateParseVersion(L"0.9.99", &current));
  assert(UpdateParseVersion(L"v0.10.0", &next));
  assert(UpdateCompareVersion(next, current) > 0);
  assert(!UpdateParseVersion(L"0.9.30-beta", &next));
  assert(!UpdateUrlAllowed(L"https://github.com.evil/asset"));
  assert(!UpdateUrlAllowed(L"https://user@github.com/asset"));
  ```
- [ ] **Step 2: Run the new suite** via `& '.\Jira Task Manager\test.ps1'`; expected compile failure for the missing model functions. Wire this suite into test.ps1 with its exact model source list, without unrelated Jira sources.
- [ ] **Step 3: Implement the model** using native_json.h. Parse asset ID as checked unsigned 64-bit decimal, not nj_int. Check duplicate critical keys locally rather than changing the parser used by Jira. Validate the entire selected asset even when its version is not newer.
- [ ] **Step 4: Run the full suite**; expected existing 13 suites plus update_model_test pass.
- [ ] **Step 5: Commit** only model, header, tests and test.ps1: `feat: validate native update release metadata`.

### Task 2: Bounded HTTPS transport

**Files:** Create `native_update_http.c`, `update_http_test.c`; extend header and test.ps1.

**Interfaces:**
- `UpdateCancel { volatile LONG cancelled; }`; all workers inspect with InterlockedCompareExchange.
- `UpdateHttpSink(const BYTE *bytes, DWORD length, void *context) -> BOOL` callback.
- `UpdateHttpGet(const wchar_t *url, const wchar_t *accept, DWORD limit, UpdateCancel *cancel, UpdateHttpSink sink, void *context, DWORD *http_status) -> UpdateStatus`.
- `UpdateCheck(UpdateVersion current, UpdateCancel *cancel, UpdateRelease *out) -> UpdateStatus`; 404 maps to UPDATE_NONE, 403/rate limit remains an explicit error.
- `UpdateDownload(const UpdateRelease *release, HANDLE destination, UpdateCancel *cancel) -> UpdateStatus`.
- Test-only injected transport supplies status, headers and byte chunks; production calls WinHTTP. Both execute the same URL/redirect/size policy.

- [ ] **Step 1: Write failing transport tests.** Assert no Authorization/Jira header, exact latest endpoint and API asset-ID endpoint; `Accept: application/json` for check, `application/octet-stream` for download. Test 404, 403, 500, cancellation, truncated body, oversized declared/chunked response, zero progress and sink failure. Reject HTTP, userinfo, port 444, `github.com.evil`, sixth redirect, redirect loop and non-allowlisted hosts before connection; accept the four permitted hosts.

  Name cases `test_check_no_release`, `test_transport_policy`, `test_download_size`, `test_cancel`; with the injected transport configured for the named case:
  ```c
  UpdateRelease release = {0};
  UpdateCancel cancel = {0};
  assert(UpdateCheck((UpdateVersion){0,9,29}, &cancel, &release) == UPDATE_NONE); /* HTTP 404 */
  InterlockedExchange(&cancel.cancelled, 1);
  assert(UpdateCheck((UpdateVersion){0,9,29}, &cancel, &release) == UPDATE_CANCELLED);
  assert(!UpdateUrlAllowed(L"https://github.com:444/asset"));
  assert(!UpdateUrlAllowed(L"http://github.com/asset"));
  ```
- [ ] **Step 2: Run full test.ps1**; expected failure for missing transport implementation.
- [ ] **Step 3: Implement transport** with automatic redirects disabled. Allow at most five hops, checked relative Location resolution, 10-second resolve/connect and 15-second send/receive timeouts plus a 120-second overall deadline. No credential reuse from Jira; stream at most the limit to the sink. Require status 200 for final body and downloaded byte count exactly release.size. Cancellation stops at bounded waits and before each new request.
- [ ] **Step 4: Run full test.ps1**; expected all suites pass without real network or Jira writes.
- [ ] **Step 5: Commit:** `feat: check and download updates over bounded HTTPS`.

### Task 3: Verify and retain only trusted staged files

**Files:** Create `native_update_files.c`, `update_files_test.c`, `update_test_fixture.c`; modify build.ps1/test.ps1 and header.

**Interfaces:**
- `UpdateStage { wchar_t directory[MAX_PATH], executable[MAX_PATH]; }`.
- `UpdateStageCreate(UpdateStage *out) -> UpdateStatus`; private unique directory beneath resolved Windows temp directory, current-user/system ACL, no directory/file reparse points.
- `UpdateVerifyFile(HANDLE file, const UpdateRelease *release) -> UpdateStatus`; regular seekable file, exact size/hash, AMD64 PE and matching VS_FIXEDFILEINFO version major.minor.patch with fourth component 0.
- `UpdateStageCleanup(UpdateStage *stage) -> void`; closes/removes only known owned files and removes empty owned directory; never recursive deletion.
- Download into a newly created, exclusively opened file. Verify that handle before closing; reopen read-only without FILE_SHARE_WRITE/DELETE and keep the verified file locked until the helper has taken over.

- [ ] **Step 1: Write failing file tests.** Compile a harmless versioned x64 fixture EXE; assert valid expected SHA-256 succeeds. Reject changed bytes, mismatched size/version, non-PE, x86, truncated headers/resources, missing version resource and reparse targets. Assert mutation while verified handle is retained fails and cleanup leaves a neighbouring sentinel file untouched.

  Name cases `test_verified_fixture`, `test_modified_fixture`, `test_staging_isolation`; `file` and `release` are the test-owned fixture handle and its expected metadata:
  ```c
  assert(UpdateVerifyFile(file, &release) == UPDATE_OK);
  release.sha256[0] ^= 1;
  assert(UpdateVerifyFile(file, &release) == UPDATE_INVALID);
  release.sha256[0] ^= 1;
  release.size++;
  assert(UpdateVerifyFile(file, &release) == UPDATE_INVALID);
  ```
- [ ] **Step 2: Run full test.ps1**; expected file suite fails. Fixture versions/resources are test-owned generated build outputs under ignored bin, not product releases.
- [ ] **Step 3: Implement file verification** using Windows hashing and version-resource APIs; add system `-lversion` where required. Do not load/execute arbitrary downloaded PE merely to read its version; bounded resource parsing if handle-based Windows APIs cannot safely provide it. Fail closed on paths longer than the supported buffers.
- [ ] **Step 4: Run full test.ps1 and build.ps1**; expected all suites and production build pass.
- [ ] **Step 5: Commit:** `feat: validate update executable before execution`.

### Task 4: Separate native apply mode with rollback

**Files:** Create `native_update_apply.c`, `update_apply_test.c`; extend fixture/header; modify native.c wWinMain and test/build scripts.

**Interfaces:**
- `UpdateApplyRequest { DWORD parent_pid; ULONGLONG parent_created; wchar_t target[MAX_PATH]; UpdateRelease release; }`.
- `UpdateLaunchHelper(const UpdateStage *stage, const UpdateApplyRequest *request, HANDLE *ready_event) -> UpdateStatus`.
- `UpdateRunHelper(int argc, wchar_t **argv) -> int`, entered for `--apply-update` before singleton, lifecycle/Jira setup or settings migration.
- Apply parameters include parent PID + creation FILETIME, canonical target, expected version/size/hash and unpredictable ready-event name. Use CommandLineToArgvW and CreateProcessW with explicit executable path and correct argument quoting, never shell interpretation.
- Main waits asynchronously for validated helper readiness; it may then request normal exit. Helper waits at most 60 seconds for the exact opened parent process, whose image must equal target and whose creation time must match.

- [ ] **Step 1: Write failing isolated integration tests.** Fake parent/fixture EXEs in a dedicated temporary directory with spaces and Cyrillic names. Assert successful replacement keeps old bytes in a uniquely named backup and starts new fixture. Assert invalid target, same source/target, reparse point, reused PID identity, missing parent, missing source, locked target, timeout and replacement failure leave old EXE usable. Inject rename failure after backup to assert restore; refuse to overwrite an existing backup. Helper must not create app settings, acquire ordinary singleton or issue Jira calls.

  Name cases `test_apply_success`, `test_apply_identity`, `test_apply_rollback`, `test_apply_timeout`; after successful fixture replacement, opening target/backup handles must satisfy:
  ```c
  assert(UpdateVerifyFile(target_file, &new_release) == UPDATE_OK);
  assert(UpdateVerifyFile(backup_file, &old_release) == UPDATE_OK);
  ```
  For injected replacement failure the target assertion instead uses `old_release`; the fixture reports launch via a sentinel in its own temporary directory, never real application state.
- [ ] **Step 2: Run test.ps1**; expected apply suite failure; tests never reference the real installation path.
- [ ] **Step 3: Implement helper** by revalidating its own staging location and bytes under a non-write-sharing handle, and the old target version being older. Retain identity/handles through launch. Copy verified bytes to a unique same-directory candidate, verify candidate, preserve old target under a unique backup, then atomically rename candidate into target. Restore backup on replace/restart failure where possible; leave recoverable files and report failure if rollback itself fails. Main remains running when helper validation/readiness fails.
- [ ] **Step 4: Run full tests and build**; expected all pass; enforce exact-target cleanup and preserve backups on success. Keep update result log non-sensitive. Write the existing planned-update lifecycle marker only after helper readiness; use shutdown reason `update` for a normal app exit.
- [ ] **Step 5: Commit:** `feat: apply native updates with backup and rollback`.

### Task 5: Working About controls and safe UI lifetime

**Files:** Create `native_update_ui.h`; modify native.c, native_settings.h, settings_test.c, test.ps1/build.ps1.

**Interfaces:**
- UI state enum: IDLE, CHECKING, CURRENT, AVAILABLE, DOWNLOADING, READY, APPLYING, ERROR; only the main UI thread mutates it.
- `UpdateUiBeginCheck(HWND settings)`, `UpdateUiBeginDownload(HWND settings)`, `UpdateUiCancel(void)`; void main-thread functions.
- `BOOL UpdateUiHandleMessage(UINT message, WPARAM wp, LPARAM lp)`; main-window-owned WM_APP+41 result and WM_APP+42 helper-ready messages.
- `BOOL UpdateInstallAllowed(wchar_t *reason, size_t capacity)` in native.c checks g_jira_busy, active completion-window count, g_attached_popover and every g_detached_popovers node, legacy g_note_edit if present, and modified settings fields.
- Worker results carry request-generation IDs, not settings HWND. Main owns/frees results even after settings closes; cancelled results cannot alter a newer request. Failed PostMessage frees its payload. Joining is bounded and never blocks UI for a network timeout.

- [ ] **Step 1: Write failing settings tests.** On About assert control 242 says «Проверить обновления»/“Check for updates”; 235 no longer contains the old Native-channel text. Exercise fake transitions: current, new, no releases, network/digest failure, download progress/error/retry, double click, close/reopen and stale result after generation change. Assert no request is started by opening settings or startup.
- [ ] **Step 2: Add failing install-gate tests.** Every attached/detached comment draft including whitespace-only text blocks exit; note_save_failed blocks; an open completion dialog and Jira/account worker block. Changed URL/token/preferences, active timer edit and unsaved legacy note block until saved/cancelled. A clean app can install. Assert a newly appearing draft between download and helper readiness cancels installation, leaves main alive and does not send a Jira comment.

  Name cases `test_update_about_controls`, `test_update_stale_result`, `test_install_blocks_drafts`, `test_install_clean`; initialize each fixture independently:
  ```c
  wchar_t reason[256];
  assert(UpdateInstallAllowed(reason, _countof(reason))); /* clean fixture */
  g_jira_busy = TRUE;
  assert(!UpdateInstallAllowed(reason, _countof(reason)));
  g_jira_busy = FALSE;
  g_attached_popover.note_save_failed = TRUE; /* fixture with live note window */
  assert(!UpdateInstallAllowed(reason, _countof(reason)));
  ```
- [ ] **Step 3: Run full test.ps1**; expected settings/UI assertions fail.
- [ ] **Step 4: Implement UI/controller.** Layout About inside its existing 430×380 DIP window: update status and check/download action, then distinct diagnostics action, then feedback; reuse theme/fonts/owner-drawn states. Idle text «Обновления через GitHub Releases» / “Updates via GitHub Releases”; no-release text «Пока нет опубликованных версий» / “No published releases yet”. Current/new/error messages are real results, never placeholders presented as completion. “Download update” only downloads; READY offers «Установить и перезапустить» / “Install and restart” and asks explicit confirmation naming both versions. Do not hide drafts as part of confirmation. Freeze new editing/Jira work only across validated helper handoff and normal exit; restore it on failure.
- [ ] **Step 5: Wire existing shutdown/state persistence.** SaveSettings preserves original g_workday_started; do not replace credential/storage identity, change startup links or call signout. Integrate completion count around real create/destroy paths. If About closes, cancel its work safely; account validation and application update cannot run concurrently. Drain/free update results during application destruction without using destroyed controls.
- [ ] **Step 6: Run full test.ps1 and build.ps1**; expected all suites pass. Add isolated preview fixture with no account/settings/Jira access; verify RU/EN, light/dark, 96/144/192 DPI and every state fits without clipped controls. Do not use Escape, OBS or system Task Manager for QA.
- [ ] **Step 7: Commit:** `feat: expose safe native updates in About settings`.

### Task 6: Verified release and rollout

**Files:** Modify native.c APP_VERSION, app.rc, app.manifest, root/source README.

**Interfaces:**
- Publication is manual: validate metadata/source/tag and show exact repo/asset/version/hash before publication. Use authenticated GitHub CLI when available; otherwise user-authorized Chrome UI. No new publication subsystem, token extraction or new credentials.
- The release is stable `v0.9.30`, asset `Jira.Task.Manager.exe`, published from the reviewed committed source. Existing releases/assets are preserved.

- [ ] **Step 1: Bump version to 0.9.30** consistently and document manual bootstrap for 0.9.29, expected update behaviour, preservation and SHA-256 trust limits. Build script must still support default installed filename and explicit release output name.
- [ ] **Step 2: Verify full test.ps1, build.ps1 -OutputName 'Jira.Task.Manager.exe', and git diff --check**; confirm all tests pass, metadata 0.9.30, x64 architecture and no tracked private/generated files. Hash the exact release artifact. Obtain a fresh whole-change code review and fix important findings before publishing.
- [ ] **Step 3: Commit and push** source/docs; publish the validated stable release using normal authorized workflow. Read back latest API, asset size/digest and tag/commit; mismatches stop rollout.
- [ ] **Step 4: Prepare local preview and show About** from an isolated profile. Explain that publishing alone cannot add an updater to the running 0.9.29; bootstrap requires replacing that EXE once. Request confirmation before restarting the real application, inspect unsaved work, back up EXE/settings/shortcuts and retain the old version. Do not stop a process by generic name.
- [ ] **Step 5: After confirmed bootstrap, verify** title/version, actual update check returning current, timer start unchanged, shortcuts valid and no settings/notes loss. If user has not confirmed, leave the running app untouched and hand off the built release.
- [ ] **Step 6: Report limitations honestly.** Fixture-based end-to-end replacement proves installation mechanics; real 0.9.30→newer installation is not claimed tested until a valid newer release exists and user explicitly confirms that test. Do not publish a fake newer version just to make the button react.

## Execution choice

Recommend **Native**: one implementer executes these dependent tasks in this session, then one independent reviewer examines the whole change. Subagent-driven per-task execution is available only if the user chooses it. User review of this plan and execution choice are required before product-code changes.
