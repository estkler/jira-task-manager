# Jira Task Manager

A native Windows desktop client for personal Jira tasks, built with the Win32 API.
No third-party UI or runtime libraries are required.

Version: **0.9.29**.

Features include task search and sorting, assigned/reported scopes, status filters,
Jira workflow transitions, encrypted local notes and completion templates, Jira
comments with unread indicators, detachable panels, a workday timer, tray controls,
and Russian/English interfaces with light/dark themes.

## Build and test

Use Windows and Zig **0.16.0**, unpacked to `.tools/zig-x86_64-windows-0.16.0/`
at the repository root. In PowerShell:

```powershell
& '.\Jira Task Manager\test.ps1'
& '.\Jira Task Manager\build.ps1'
```

Output: `Jira Task Manager/bin/Jira Task Manager.exe`.
See the application README for detailed behavior and storage information.

## Compatibility and privacy

The display name and executable are Jira Task Manager. Internal storage remains
under `%LOCALAPPDATA%/Task Manager` so existing settings, encrypted notes,
completion templates and the workday timer stay intact. Jira tokens remain in
Windows Credential Manager; they are not stored in this repository.

Build output, local settings, diagnostic logs, backups and shortcuts are excluded.
The update channel is not implemented yet; renaming this repository does not enable
application updates by itself.
