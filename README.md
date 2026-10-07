# Jira Task Manager

A native Windows desktop client for personal Jira tasks, built with the Win32 API.
No third-party UI or runtime libraries are required.

Version: **0.9.30**.

Features include task search and sorting, assigned/reported scopes, status filters,
Jira workflow transitions, encrypted local notes and completion templates, Jira
comments with unread indicators, detachable panels, a workday timer, tray controls,
and Russian/English interfaces with light/dark themes. Version 0.9.30 adds manual
native updates, distinct own/colleague comment styling, and incoming/outgoing
scope icons with a blue cross-mode activity indicator.

## Updates

Use Settings → About → Check for updates. The app checks stable releases in
this repository only when requested; it does not add background update polling.
Download and installation are separate actions. Installation asks for confirmation
and restarts the app only after its draft/state-safety checks pass. The old EXE is
retained under a unique backup name; settings, encrypted notes, credentials and the
original workday timer start remain in their existing locations.

Versions up to 0.9.29 need a one-time manual bootstrap: close the app normally
after saving notes and clearing/sending comment drafts, back up its EXE and settings,
then place the release asset `Jira.Task.Manager.exe` in the existing application
folder as `Jira Task Manager.exe`. Keep existing shortcuts targeting that filename.
Publishing a release does not update an already-running 0.9.29 by itself.

The updater requires the release asset's SHA-256 digest, matching version and x64
PE metadata. HTTPS and the GitHub repository owner establish channel trust;
the digest verifies consistency with GitHub's asset, not an independent author
signature. No automatic elevation or bypass of Windows protections is used.

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
Release publication is manual. Comment image viewing and tray alerts for new
comments are not included in this version; existing task-arrival notifications
and comment unread indicators remain available.
