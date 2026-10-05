#ifndef TASK_MANAGER_NATIVE_IDENTITY_H
#define TASK_MANAGER_NATIVE_IDENTITY_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>

#define TASK_MANAGER_TITLE L"Jira Task Manager"
#define TASK_MANAGER_EXE_NAME L"Jira Task Manager.exe"
#define TASK_MANAGER_DATA_DIRECTORY L"Task Manager"
#define TASK_MANAGER_STARTUP_LINK L"Jira Task Manager.lnk"
#define TASK_MANAGER_CREDENTIAL L"Task Manager:Jira:PAT"

BOOL TaskManagerMigrateSettingsAtRoot(const wchar_t *local_root,wchar_t *settings_path,size_t capacity);
BOOL TaskManagerDiagnosticsDirectory(const wchar_t *executable_path,wchar_t *directory,size_t capacity);
BOOL TaskManagerSetStartupAt(const wchar_t *startup_folder,const wchar_t *executable_path,BOOL enabled);
BOOL TaskManagerStartupEnabledAt(const wchar_t *startup_folder,const wchar_t *executable_path);
BOOL TaskManagerSetStartup(BOOL enabled);
BOOL TaskManagerStartupEnabled(void);
BOOL TaskManagerMigrateCredentialNames(const wchar_t *new_target,const wchar_t *old_native_target,const wchar_t *old_legacy_target);
BOOL TaskManagerMigrateCredential(void);

#endif
