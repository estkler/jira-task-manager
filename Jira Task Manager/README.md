# Jira Task Manager

## Current version — 0.9.29

The display name, executable metadata and startup shortcut now use Jira Task Manager.
Internal data, credential and window-class names remain unchanged to preserve settings,
encrypted notes/templates, the timer and single-instance compatibility.

The completion dialog uses the same compact header and surfaces as notes/comments.
Five native-drawn stars preview ratings on hover; clicking commits the choice,
and a rating from one to five is required before completion. There is no zero-rating
button or asterisk beside the compact label. Arrow keys navigate available ratings.
The empty comment field shows “Комментарий к задаче: [task name]”, using the title
from the task list; typing hides this hint. The remembered-template workflow is unchanged.

The status filter is grouped with the STATUS header, centered as one unit without
changing column widths. For tasks without Jira comments, the context menu offers
Add comment (Написать Jira комментарий) instead of Jira comments; existing comment feeds keep their usual label.

Approved visual choices (2026-10-05): the application logo is a single white check
on a rounded blue tile, with no inner card or horizontal bars. Keep the same
silhouette at every icon size. The current icon-only assigned/reported scope
switch is the selected variant; do not redesign it as part of logo updates.

Jira Task Manager is a native Win32 desktop client for personal Jira tasks. It provides search,
sorting, per-tab status filters, Jira workflow transitions, task activation, comments,
DPAPI-encrypted local notes and completion-comment templates, a workday timer, tray controls
and light/dark themes.

Tasks with content show separate note and Jira-comment markers beside the title. Clicking a marker opens
a task-specific popover anchored to that marker; the table stays at its compact width. The
note popover edits a local note without an author field. The Jira popover displays a compact
chronological comment feed (oldest first, initially scrolled to the newest) with authors and inline dates, a native vertical scrollbar when needed, and a
field for posting a new comment. The existing footer controls remain accessible while a
popover is open and do not close it when theme, language or text size changes. The footer
mode switch and extra table column are no longer shown.
An unread Jira comment fills its marker; a newly arriving comment briefly pulses before settling
into that unread state. A read comment uses a quiet outline, while the note marker remains a
separate page symbol. Hovering either marker adds a soft tint; pressing it gives a darker fill,
and the active popover keeps its marker filled. Each popover has a compact title and
task key in a tinted header. Hovering a note marker previews its text. The task context menu can open
either popover even when its marker is absent.
Drag a popover by its header to detach it. Detached
notes and Jira comments stay open independently while other tasks are selected; several
windows can remain open at once. Drag a detached window back to its task marker to
attach it again. Detaching preserves the content's screen position and width; attaching
restores the pointer gutter without squeezing the text. Popovers can also be resized from their lower corner.
The shaped border stays active during resizing, so the pointer gutter never becomes a
colored rectangular underlay. Opening or resizing a popover does not shift or compress
the main window's footer controls or update label.
Each changed size completes painting of the popover and all its child controls before
the next drag event, instead of relying on idle-time WM_PAINT delivery during rapid resizing.

New Jira tasks show a small NEW marker until the task is clicked. The first successful list
establishes the baseline; later additions are tracked per account across restarts. When the
window is hidden or inactive, a tray notification announces new arrivals.
NEW, the comment marker and the note marker share one reserved cluster after the title;
long titles are shortened to leave room for all three. Clicking an icon next to NEW still
opens the intended popover when the task becomes seen and the badge disappears.
Right-clicking a task opens its local-note popover. Notes are encrypted on this computer and
saved as text changes, so a crash while editing does not discard already typed text.
If a disk write fails, the popover keeps the text visible for another attempt.
Right-clicking a task also gives the row the same brief feedback as a left click; a quieter
highlight remains while its compact, non-indented context menu is open.
The separate reporter-scope switch shows Jira issues reported by the current account,
including issues assigned to other people. The All, Current, and Done tabs filter either
assigned or reported issues; each scope remembers its selected tab and status filters.
The switch uses a task card with an incoming/outgoing arrow for the current scope. A small amber dot means
that the other scope has new tasks or unread Jira comments not present in this scope.
The status filter sits in the Status column header, visually separate from the scope switch.
Issues also assigned to the current account appear in both relevant scopes without a duplicate
row. The first reporter load sets a baseline for new-task markers. The compact scope icon has
a full role description on hover, keeping the toolbar on one line at minimum width.
Table columns reserve the vertical scrollbar gutter in both scopes, so changing the
number of visible rows does not shift their positions. The app icon uses a single white check
on a rounded blue tile, with antialiased versions for the title bar and desktop.
When no scrollbar is needed, the reserved space blends into the header and full-width row
backgrounds, rather than leaving a separate white cutout.
New Jira comments from others highlight the separate dialogue marker. Opening the Jira
popover records the comment as seen on this computer. Existing comments form
the first-load baseline; unread state is account-specific and survives a restart.
Tasks observed without comments retain an empty-history baseline, so their first later
comment from another person is also unread. Own comments are considered read and do not pulse.

The application uses Windows APIs only. It does not require .NET or third-party libraries.

## Data and diagnostics

- settings and encrypted local notes: `%LOCALAPPDATA%/Task Manager/settings.ini`
- Jira PAT: Windows Credential Manager target `Task Manager:Jira:PAT`
- active lifecycle log: `diagnostics/Task Manager.log`
- current-session marker: `diagnostics/Task Manager.running`
- preserved pre-rename diagnostics: `diagnostics/history/`

On first upgraded launch, settings and Jira credentials are migrated from the retired Native
identity. Old credential targets are removed only after the new credential has been written and
verified. Historical logs are not used as live application state.

## Startup

Autostart is implemented as the Windows Startup-folder shortcut `Jira Task Manager.lnk`. The shortcut
targets `Jira Task Manager.exe` with `--startup`; the single-instance guard prevents duplicate windows.
The Settings checkbox creates or removes this shortcut using the Windows Shell API.

## Build and verification

```powershell
& '.\Jira Task Manager\test.ps1'
& '.\Jira Task Manager\build.ps1'
```

The primary build output is `Jira Task Manager/bin/Jira Task Manager.exe`.

When upgrading manually from Task Manager, update the desktop and Windows Startup
shortcuts to target `Jira Task Manager.exe`, and rename the existing shortcuts to
`Jira Task Manager.lnk` rather than keeping duplicate startup entries. Do not delete
the existing `%LOCALAPPDATA%/Task Manager` data folder or its Windows credential.
