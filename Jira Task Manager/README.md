# Jira Tasklet

Automatic Jira refresh uses a lightweight, paginated membership index and fetches
details only for new or changed issues. Requests share one serialized transport;
rate-limit responses pause subsequent requests, and writes are never blindly retried.

Verified account metadata is reused for the same connection. Cross-job reuse requires
the same server and Credential Manager revision; changed credentials force verification.
Interface-only settings and comment-display changes do not reload Jira. A manual
refresh postpones the next automatic refresh by the selected interval.

The latest complete task snapshot is stored in a Windows DPAPI-encrypted
`settings.ini.jira-cache.dat` sidecar, with a seven-day lifetime and 128 MiB limit.
It contains server task data, never tokens, local notes or media. After restart,
the app verifies the account and fetches a fresh membership/revision index before
reusing unchanged details. Different server/account/credential revision/comment
filter, malformed cache or unavailable storage falls back to a normal load.
Sign-out removes this derived cache. It is not an offline task view.

For a server returning 100 index rows per page, a 54-task first load uses five
GETs (account + index + three detail batches); an unchanged verified restart
uses two. At 1000 tasks the respective counts are 51 and 11. Ordinary unchanged
refreshes use only the index pages; changed details are fetched in batches of up
to 25. These are request counts, not a guarantee of server processing cost.

Settings → Jira has “Pause updates when the computer is locked”, enabled by
default. Windows lock/disconnect and sleep pause automatic refresh; unlock/resume
coalesces into one delayed refresh. Manual actions remain available. There are no
working-hour or weekday fields. Changes apply with Save, and Cancel discards drafts.
Session notification registration and state queries recover after early-logon
service delays. Local notes, bookmarks and dimmed tasks remain local.

The Jira account card shows the connected user without token fragments or local
token labels. Replace token verifies the new credential before saving it; a
canceled or failed replacement preserves the existing connection and local data.

Due dates appear before Status, using the standard Jira `duedate` field in
existing detail requests. Today is amber, overdue dates are red with an explicit
label, and completed issues retain a neutral date. Empty values stay blank; the
column disappears when no valid dates exist. At narrow widths, dates use a compact
numeric format with `!` for overdue; hover retains the full date. Date-only values
are never shifted through UTC, and day rollover updates labels locally.

Right-click the workday timer to switch directly between digital and text formats;
the choice persists without changing the workday start. Left-click opens the start-time
and work-time analysis menu. Task-row hover includes the free space after the last column;
status, issue-key and note/comment action targets remain precise.

The interface and Windows display metadata use Jira Tasklet. Existing owned Desktop,
Programs and Startup shortcuts migrate to Jira Tasklet.lnk while retaining launch parameters.
The installed EXE, internal resource identity and update asset names remain compatible with Jira Task Manager;
no data or credential migration is needed.

## Current version — 0.9.32

Manual native updates are available in Settings → About. Checking never runs at
startup or on a background timer. A newer stable GitHub release offers a download,
then a separately confirmed install/restart. Installation is blocked by active
Jira/account requests, completion dialogs, comment drafts (including detached
windows), note-save failures, active edits, or unsaved settings. State-save or
helper-readiness failure leaves the application running. The timer's original
start value, storage identity, credentials and shortcuts are not changed.

For the first upgrade from 0.9.29, save your work and close the application normally,
back up the old EXE and `%LOCALAPPDATA%/Task Manager/settings.ini`, then manually
replace only the EXE in its existing folder. The GitHub asset is
`Jira.Task.Manager.exe`; the installed filename remains `Jira Task Manager.exe`.
Do not overwrite a running EXE or remove the backups. Later confirmed updates
retain a uniquely named old-EXE backup and restore it if replacement/restart fails
where the filesystem still permits recovery.

Only stable releases from `estkler/jira-task-manager` are accepted, with the exact
asset name, bounded size, SHA-256 digest, x64 PE and matching version. A missing or
incorrect digest blocks installation. SHA-256 is not an independent author
signature: channel trust depends on HTTPS and control of the GitHub repository.
No external updater runtime, service, administrator elevation or Windows-security
bypass is introduced. The update helper uses the same EXE in a separate native mode.

Version 0.9.31 adds bookmarks, modeless completion feedback, comment attachment
viewing, avatars/cache, comment/status tray alerts and compact preference pages
to the published 0.9.30 baseline, without another Jira polling schedule.

Settings → Notifications has three independent switches for task/comment/status
Windows alerts and related in-app activity indicators. All default on. Status
changes detected outside this app notify even while its window is active; the
checkbox is labelled "Notify about status changes by a manager". Comma-separated author email exclusions default to
`no-mail@kama.gs`; an explicitly empty field disables exclusions. Matching is
case-insensitive and exact, requires Jira's `emailAddress`, and never guesses by
display name. Excluded comments are hidden without hiding their issues. Saving
local preferences does not verify credentials when the account is unchanged.
The normal settings window stays 430×360 DIP; the compact page scrolls only if
its contents no longer fit. Comment alerts share the existing Jira refresh,
persist consumed comment IDs, ignore self-authored comments by stable identity,
include completed tasks, and combine simultaneous task/comment arrivals into
one quiet Windows notification. Changing exclusions clears cached excluded
previews immediately, including offline; revealing older comments does not
generate history alerts across restarts.

Settings → General has a separate Show task totals in tabs checkbox. Hiding totals
keeps the tab activity +counts visible, subject to the independent task/comment
notification switches. These preference checkboxes apply immediately, including
notification behavior; Save persists the choice, while Cancel restores the previous
values without marking tasks/comments read. Notifications suppressed while a
switch is off are not replayed on Cancel. Timer/theme saves do not commit previews.

Settings → General also offers Show the All tab (off by default; existing saved
choices are preserved). Turning it off
hides All in both task scopes and gives its toolbar space to search. If All
was selected, Current becomes active without clearing the query; Done is not
changed. The checkbox previews immediately, Save persists it, and Cancel restores
the previous visibility. This unreleased option does not change Jira data.
Show the Done tab is independent and on by default. Hiding an active Done tab
falls back to Current without clearing search; hidden tab slots return to search.
Both visibility preferences preview live, respect Save/Cancel, and apply in both
task scopes. Current remains available even if All and Done are both hidden.

Settings have four tabs in visual order: Jira, General, Notifications, About.
General contains startup, task totals, optional All/Done tabs, and the tray-menu
task limit. The latter is a full-width native slider with integer values 1–30,
an immediate numeric readout, and the existing stored preference (default 20).
The refresh slider has seven subtle five-minute ticks aligned to its selectable positions, with numeric labels at15/30/45. The tray slider likewise marks each integer1–30, retaining its three numeric labels. Native position changes invalidate the whole custom control, clearing the old thumb, halo and selected track during dragging and keyboard navigation. The complete slider is composed in memory and presented in one copy, avoiding cleared or half-drawn tick frames; stock synchronous position redraw is suppressed. Low-memory allocation failure keeps a complete direct-drawing fallback. Both sliders select the nearest allowed value directly where the track is clicked,
instead of moving by one page. Pointer dragging snaps to 5-minute refresh steps or
individual tray tasks. The native keyboard/value provider remains intact. Their
slim tracks, round thumbs, focus cues and disabled states follow the active theme;
dark mode has no light control rectangle. Hiding, disabling or canceling a drag
releases pointer capture without committing the preference.
Jira retains connection/account actions and its full-width 15–45-minute refresh
slider. The shared Defaults footer button stages the factory Settings/Advanced values without changing the Jira connection, URL/token drafts, local data, timer, theme or language. Save applies; Cancel discards, including checkbox edits after reset. Defaults are refresh30/tray20, startup off, totals on, All off/Done on, all three alerts on, the standard excluded author and avatar cache7days. Busy account/Jira/update operations block resetting. Advanced now opens from General and remains a separate owned window
with the existing shared Save/Cancel and avatar-cache draft. Notification
preferences and all About/update/diagnostics/contact actions remain available.
Validation/status messages are shared across all pages; busy account checks
freeze the new controls too. Neither sliders nor tab changes save drafts.

Outside bookmark view, saved tasks show a small muted outline ribbon beside the
issue key. Every row keeps its number. A fixed 14-DIP allowance in the key column
prevents the marker from colliding with the title or shifting columns when
membership changes. The marker does not change task status or acknowledge
activity. Native accessibility retains the number and adds Bookmarked.
Bookmark view keeps its normal row numbers; selection mode retains its existing
blue filled/outline choices.

Bookmarks use the fixed icon at the right of search. An empty set opens selection:
click or drag through rows to add/remove task bookmarks, keeping search editable.
Enter or the icon/caption saves; Esc cancels. With a saved set, the icon switches
between all tasks and bookmarks. Right-click it to edit or clear the set; task
context menus also offer bookmarks alongside Jira comments and local notes.
The left caption and blue header identify selection without moving the icon or
renaming columns. Clearing shows a short confirmation. Existing tabs, scopes and
status filters still apply; bookmarking never acknowledges task/comment activity.
The footer shows only the bookmark total when all are visible, and a shown/total
message when search or other filters hide some bookmarks.
Membership is stored atomically as stable issue keys in adjacent
`bookmarks-<identity-hash>.dat` files, isolated by the loaded snapshot's server and
stable Jira user identity. A restart preserves membership but opens the full list.
Save failures keep a retryable draft. No new library or background Jira request is
required.

Comment media: Jira wiki image references and ADF attachment IDs
(or unambiguous media alt filenames) produce image thumbnails under the comment
text; click opens a resizable native viewer. GIF uses the first frame. Windows
WIC raster codecs decode off the UI thread; an unavailable codec or corrupt
file leaves a clear placeholder. Avatar pictures load into the existing circle,
falling back to initials. Author names open their Jira profile in the browser,
with UTF-8 percent-encoded stable usernames or account IDs.

Images/avatars load only when opening or reloading the comment feed, not during
ordinary task refresh. Downloads require HTTPS, the configured host/port/context,
known attachment/avatar routes, no path traversal and no redirects; external
images/avatars keep a placeholder/initial. Repeated avatar URLs are requested
once, including failures. A feed allows 16 attachment requests and 12 MiB of
image bytes (including partial failures), plus at most 24 avatar requests of
128 KiB each. Each image is at most 6 MiB encoded, 12 megapixels and 8192 pixels
per dimension. Retained encoded assets are capped globally at 24 MiB and decoded
bitmaps at 16 MiB; a single WIC decode bounds codec scratch memory. The enlarged
bitmap fits within 1920×1440. Rapid viewer activations coalesce behind one worker;
obsolete results are discarded, and the viewer retains its own asset reference
after the source feed closes. Comment attachments have no disk cache. Avatars use
a bounded disk cache isolated by Jira server and stable account identity; its
retention is controlled in General → Advanced. No external libraries,
services or additional polling timers are added.

Known rare recovery limitation: if Windows refuses the viewer's completion
message (for example, an exhausted message queue), worker ownership and its
slot recover, but the open viewer may keep its loading text. Closing/reopening
the viewer retries; this case is not yet covered by the completion fixtures.

The display name and executable metadata are Jira Tasklet. The technical executable
remains Jira Task Manager.exe for compatibility with existing installations and updates.
About uses compact sections, one version label, one update action and quiet Report a problem / Diagnostics links.
Internal data, credential and window-class names remain unchanged to preserve settings,
encrypted notes/templates, the timer and single-instance compatibility.

The completion dialog uses the same compact header and surfaces as notes/comments.
Five native-drawn stars preview ratings on hover; clicking commits the choice,
and a rating from one to five is required before completion. There is no zero-rating
button or asterisk beside the compact label. Arrow keys navigate available ratings.
The empty comment field shows “Комментарий к задаче: [task name]”, using the title
from the task list; typing hides this hint. The remembered-template workflow is unchanged.
Completion forms are modeless and own their task/connection binding and a copy of
the selected transition. Settings, scopes, tabs, search and bookmarks remain
available while drafting. Live language/theme changes retain the rating/comment;
each form handles its own monitor DPI. Only a submitting form is protected, with
the existing single Jira-job guard applying to network operations. Another form's
submission while busy explains waiting and retains its draft; it is not silently
queued or retried. Request failures keep the draft visible.

Selected stars stay bright gold and briefly compress on press. Hovering the gaps
between stars does not restore a different committed value; there is no redundant
selected-count label. Pending work retains the real Jira status. Successful POST
plus reread of the exact destination gives a clean green row sweep and centered
check/actual status, then an 850 ms Current-row hold/fade and a 6 sec footer notice.
Only display retention bypasses the Current tab/status filter; search, bookmarks,
scope and real tab counts remain authoritative. No simulated status or POST retry
is introduced. Returning managed Review/Upload project tasks to work is explained
as a manager-controlled action rather than a generic transition error.

Starting work by double-click or an explicit status transition smoothly lifts the
visible row into its new position over 350 ms after Jira confirms it is active.
The client changes only the selected issue. Automatically pausing other active
issues is owned by the team's Jira workflow, not duplicated by the client;
explicitly pausing the selected active issue still requests On Hold. The reread
shows actual server statuses, including if a server rule has not finished yet.
Start/pause preflight reads the selected issue, not the full feeds. Readback
fetches status and ownership only for the selected and previously active issues;
cached titles, comments and notes stay untouched. While Jira's rule is pending,
only unresolved keys are reread (at most four follow-ups in a 15-second scheduling
window, with the remaining budget passed into HTTP timeouts). Expired or superseded
results are ignored. Failed reads retain the last observed values; no synthetic
Hold or repeated POST is introduced. Ordinary scheduled/manual full refresh is
unchanged.
Other visible rows interpolate to their authoritative positions, then one 450 ms
amber sweep plays. The previous
active row fades to its normal background; the new row keeps its usual yellow
highlight. Failed requests, pauses, and ordinary refreshes do not play a success
effect. The effect follows task identity rather than row position and respects
Windows' animation preference. Hidden, scrolled-out, changing or oversized views
retain ordinary feedback. Filter/sort/scroll, column resize, theme/DPI/layout changes
and cancellation release the temporary pixel layer. A dismissing mouse click is
consumed through release so it cannot act on a different issue beneath the motion.

Notification preferences are compact, and Advanced opens in a separate owned
window. Email exclusions show a scrollbar only when needed. Native checkbox
captions follow light/dark themes, including live theme changes. The Jira refresh
slider offers 15–45 minutes in 5-minute steps, with a live value and 15/30/45 marks;
the default is 30 minutes. Save commits the selected value and Cancel keeps the
previous interval. Legacy values are clamped to the range and rounded to the nearest step.

The status filter is grouped with the STATUS header, centered as one unit without
changing column widths. For tasks without Jira comments, the context menu offers
Add comment (Написать Jira комментарий) instead of Jira comments; existing comment feeds keep their usual label.

Approved visual choices (2026-10-05): the application logo is a single white check
on a rounded blue tile, with no inner card or horizontal bars. Keep the same
silhouette at every icon size. The current icon-only assigned/reported scope
switch is the selected variant; do not redesign it as part of logo updates.

Jira Tasklet is a native Win32 desktop client for personal Jira tasks. It provides search,
sorting, per-tab status filters, Jira workflow transitions, task activation, comments,
DPAPI-encrypted local notes and completion-comment templates, a workday timer, tray controls
and light/dark themes.

The workday timer continues counting after nine hours. Its elapsed digits then become
bold red; the ordinary-color caption reads “Пора домой” / “Time to go home”, followed
by the transparent U+1F917 Sample artwork from Unicode's emoji chart. The PNG is
embedded locally, with its provenance and upstream notices in `Assets`; no download
or additional runtime library is needed. The existing daily reset is unchanged.

Tasks with content show separate note and Jira-comment markers beside the title. Clicking a marker opens
a task-specific popover anchored to that marker; the table stays at its compact width. The
note popover edits a local note without an author field. The Jira popover displays a compact
chronological comment feed (oldest first, initially scrolled to the newest) with authors and inline dates, a native vertical scrollbar when needed, and a
field for posting a new comment. The existing footer controls remain accessible while a
popover is open and do not close it when theme, language or text size changes. The footer
mode switch and extra table column are no longer shown.
An unread Jira comment uses an accent outline with a light blue fill and gentle repeating motion.
A read comment uses a quiet outline, while the note marker remains a
separate page symbol. Hovering either marker adds a soft tint; pressing it gives a darker fill,
and the active popover keeps its marker filled. The comments header shows the task
name rather than a generic label, with its issue key and close control independently
reserved on the right. Long names are ellipsized and available in full on hover;
the title updates after a task rename and stays available if a detached window's
task disappears from the refreshed list. Local notes keep their Notes header.
Hovering a note marker previews its text. The task context menu can open
either popover even when its marker is absent. It can also copy the full task title
(without the issue key) or its plain Jira link. Copying does not mark task or comment activity as read.
Drag a popover by its header to detach it. Detached
notes and Jira comments stay open independently while other tasks are selected; several
windows can remain open at once. Drag a detached window back to its task marker to
attach it again. Detaching preserves the content's screen position and width; attaching
restores the pointer gutter without squeezing the text. Popovers can also be resized from their lower corner.
The shaped border stays active during resizing, so the pointer gutter never becomes a
colored rectangular underlay. Opening or resizing a popover does not shift or compress
the main window's footer controls or update label.
Local notes show a vertical scrollbar only when their wrapped text exceeds the
available editing height. Full-width measurement avoids retaining an unnecessary
bar at a wrapping boundary, without resetting undo, selection or the reading position.
The complete rounded frame is drawn from the same region used to shape the window,
so its lower/right sides and corners remain visible in docked and detached panels.
Each changed size completes painting of the popover and all its child controls before
the next drag event, instead of relying on idle-time WM_PAINT delivery during rapid resizing.

New Jira tasks show a small NEW marker until the task is clicked. The first successful list
establishes the baseline; later additions are tracked per account across restarts. When the
window is hidden or inactive, a tray notification announces new arrivals.
NEW, the comment marker and the note marker share one reserved cluster after the title;
long titles are shortened to leave room for all three. Clicking an icon next to NEW still
opens the intended popover when the task becomes seen and the badge disappears.
Right-clicking a task opens its context menu, including the local-note action. Notes are encrypted on this computer and
saved as text changes, so a crash while editing does not discard already typed text.
If a disk write fails, the popover keeps the text visible for another attempt.
Right-clicking a task also gives the row the same brief feedback as a left click; a quieter
highlight remains while its compact, non-indented context menu is open.
The outgoing scope shows Jira issues reported by the current account,
including issues assigned to other people. The All, Current, and Done tabs filter either
assigned or reported issues. Switching scopes preserves the selected tab; status filters
remain independent for each scope/tab.
Two permanent mode buttons precede the search field: inbox for assigned tasks,
paper plane for reported tasks. Both buttons share one neutral rounded frame; the selected segment
has a quiet panel-colored background and blue glyph, without the status tabs' underline,
and the window caption names the current mode. A quiet in-button counter shows
tasks with new tasks or unread comments in that button's own scope; one task with
both event types counts once. Shared issues can mark both buttons. The counter
caps at 99; tooltips expose the exact total, new-task count and unread-comment task count.
Clicking an already selected mode does nothing. No extra notification row is reserved,
and search, mode buttons and tabs retain their geometry across language and activity changes.
Search begins at the left margin. The inbox, paper plane and counter use a common 14-DIP
visual size in separate, centered slots, with breathing room above and below.
Unread Jira comments retain the normal full-size cloud with a blue outline and light blue fill,
without a corner dot or the solid fill reserved for an open/pressed panel. Visible unread comments run
two gentle movements during the first 960ms of each 6000ms cycle until read, including
persisted unread comments after restart. Hidden/minimized windows, offscreen/filtered rows,
and comments already open in a panel do not animate. Windows' client-area animation preference
is respected. The existing local second tick checks visibility; the 30ms painter timer runs
only while unread comment icons are visible and invalidates the bounded 4-DIP motion envelope.
The cloud keeps its original size and hit target. There are no extra Jira requests, new libraries,
or row geometry changes. When animation is disabled, the blue unread state remains static.
Hover, pressed and open fills include both the cloud body and its tail.
Hovering does not mark comments read. After icon activation, successful loading marks them
read; failed loading preserves the unread marker.
The status filter sits in the Status column header, visually separate from the scope switch.
Issues also assigned to the current account appear in both relevant scopes without a duplicate
row. The first reporter load sets a baseline for new-task markers. The compact scope icon has
a full role description on hover, keeping the toolbar on one line at minimum width.
Table columns reserve the vertical scrollbar gutter in both scopes, so changing the
number of visible rows does not shift their positions. The app icon uses a single white check
on a rounded blue tile, with antialiased versions for the title bar and desktop.
When no scrollbar is needed, the reserved space blends into the header and full-width row
backgrounds, rather than leaving a separate white cutout.
New Jira comments from others highlight the separate dialogue marker. Successfully loading
comments in the activated Jira popover records them as seen on this computer. Existing comments form
the first-load baseline; unread state is account-specific and survives a restart.
Tasks observed without comments retain an empty-history baseline, so their first later
comment from another person is also unread. Own comments are considered read and do not pulse.
The comment feed uses the same neutral surface and avatar styling for every
message; a muted localized You/Вы marker beside the author identifies your own
messages without making them look unread. Ownership
uses Jira account identifiers, not matching display names. Chronology and body
alignment remain unchanged.

The application uses Windows APIs only. It does not require .NET or third-party libraries.

Double-click All, Current or Done to scroll to the next task with new-task or unread-comment
activity in that scope and tab. Repeating the gesture cycles through the visible filtered
rows. A short row highlight locates the target without marking its activity read.

Note and Jira-comment windows have a Pin on top button beside their Close button. Pinning
detaches the window and keeps it above other apps when the main window is hidden or minimized.
Unpinning changes only topmost status: the panel stays independent and does not restore the
main list or move. Dragging back to a visible task marker explicitly docks it again.
A real Exit from the tray still closes the
application and its windows. Header buttons support keyboard navigation and localized names.
Header glyphs are thin and muted, with a blue pin when active but no persistent colored tile.
The issue key is a keyboard-accessible link opening Jira in the browser even with the main
window hidden. Comment thumbnails and captions share the text's left edge and a bounded
aspect-ratio card, rather than a full-width centered image slot. Trailing blank rows left
by extracted final wiki image markup are removed without rewriting ordinary paragraphs.
Send sits inside the composer frame, in its own right-hand slot; text never overlaps it.

Hiding system authors no longer turns an older visible comment into unread activity. Numeric
Jira comment IDs retain a read watermark which never moves backward after filtering, deletion,
opening older history or observing an older own comment. This change concerns new comments;
editing the body of an existing comment with the same ID is not a new-comment notification.

Avatars support raster images and bounded, local-only SVG through Windows WIC/Direct2D/XmlLite.
Jira's simple class-based SVG colors are converted to presentation attributes; active content,
external resources, imports and unsupported CSS are rejected. No third-party renderer ships.
Encoded avatars are cached under `%LOCALAPPDATA%/Task Manager/avatar-cache`, scoped to server
and stable account identity. A valid stale avatar stays visible if refresh fails. The cache is
bounded to 256 files and 32 MiB and excludes tokens. Settings → General → Advanced
contains the refresh interval, default 7 days (1–365).

Advanced settings now open in a separate owned window beside Settings (with a left/onscreen
fallback), rather than expanding a settings page. Closing Advanced retains its draft;
Save in Settings persists it and Cancel discards it. Its font/layout follows monitor DPI and
its theme follows the application. Email exclusions show a scrollbar only when wrapped text
overflows the field. Notifications no longer contain the redundant explanatory caption.

Unavailable return-to-work attempts from Review/Upload project/Done use contextual RU/EN
information pointing to the manager, without “statuses unchanged” copy. Availability still
comes from Jira preflight, not a hardcoded transition ban. Connection/reload failures remain
warnings; unavailable pauses name On Hold, not In Progress.

## Data and diagnostics

Status-change tray notifications have an independent default-on setting,
“Уведомлять об изменении статуса менеджером”. They also appear while Task Manager
is active, at the next successful Jira refresh. Multiple arrivals share one balloon.
The first snapshot after launch/account change is a baseline, not historical activity.
Own transitions through this app are excluded by issue key and expected destination;
failed reloads retain that journal until a successful refresh. The current snapshot
API does not identify the change author or their role: other changes, including
your own edits in the Jira browser, are treated as external status changes.

- settings and encrypted local notes: `%LOCALAPPDATA%/Task Manager/settings.ini`
- Jira PAT: Windows Credential Manager target `Task Manager:Jira:PAT`
- active lifecycle log: `diagnostics/Task Manager.log`
- current-session marker: `diagnostics/Task Manager.running`
- preserved pre-rename diagnostics: `diagnostics/history/`

On first upgraded launch, settings and Jira credentials are migrated from the retired Native
identity. Old credential targets are removed only after the new credential has been written and
verified. Historical logs are not used as live application state.

## Startup

Autostart is implemented as the Windows Startup-folder shortcut `Jira Tasklet.lnk`. The shortcut
targets `Jira Task Manager.exe` with `--startup`; the single-instance guard prevents duplicate windows.
The Settings checkbox creates or removes this shortcut using the Windows Shell API.

## Build and verification

Automatic Jira refresh uses 15–45 minutes in 5-minute steps; a missing setting defaults
to 30. Legacy values are clamped to the nearest boundary and rounded to the nearest
step. Tray task counts and avatar cache lifetimes retain their independent ranges.

Bookmark editing uses a pencil before changes and a blue check when there are unsaved
changes. The action stays fixed beside search; narrow windows hide its text instead of
covering the query. Enter or the action saves; Esc cancels.

```powershell
& '.\Jira Task Manager\test.ps1'
& '.\Jira Task Manager\build.ps1'
```

The primary build output is `Jira Task Manager/bin/Jira Task Manager.exe`.

When upgrading manually from Task Manager, update the desktop and Windows Startup
shortcuts to target `Jira Task Manager.exe`. Current Jira Task Manager shortcuts for this exact
installation migrate to `Jira Tasklet.lnk`, preserving arguments and Windows startup approval.
Foreign shortcuts and conflicting names are left untouched. Do not delete
the existing `%LOCALAPPDATA%/Task Manager` data folder or its Windows credential.
