# X3 Todoist development and device guide

Repository: [aboutkrin/X3-Todoist](https://github.com/aboutkrin/X3-Todoist), branch `main`.

Current validation: Thai/emoji rendering and project selection pass eight host
suites. The Back-position firmware was built, flashed and hash-verified; the
subsequent **Sleep now** label/display clarification is built but not yet flashed.
The latest Back-position startup capture again reported inability to open the
temporary SD response file (sync result 8). The successful Thai-build sync below
is an earlier result, not evidence that the storage issue is permanently resolved.

This experimental firmware extends CrossPoint at `1d61100f90d2e7e32965c14301320d9989a7ab71`.
Select the `dashboard` build environment. Existing reader targets remain available.

Project-picker validation on 2026-09-26: six host suites pass on Windows using
Zig's C++20 compiler (without sanitizers). They cover project settings recovery,
multi-project sync, undated/future tasks, selection/cache isolation, web pairing,
escaped project names, invalid selections and setup expiry, alongside existing
completion and layout checks. Earlier firmware was backed up, flashed and verified
with live Todoist refresh on the X3. The project-picker update still requires its
own hardware acceptance; a passing host test does not establish runtime heap use.

The project-picker firmware build passed without compiler warnings: 58,160 bytes
static RAM and 5,580,017 bytes application flash (85.1% of the partition).
Matching firmware, ELF and checksums are saved in
`build/dashboard-release/project-picker/`. Runtime Wi-Fi/TLS allocations are
additional to the static RAM figure.

The emoji update also passes the firmware build and seven Windows host suites.
Static RAM remains 58,160 bytes; application flash is 5,912,659 bytes (90.2%).
`build/dashboard-release/emoji/` holds the matching binary, ELF and checksums.
The linked symbol table places all 331,690 bytes of emoji lookup/artwork data in
read-only flash. Drawing uses fixed buffers and no additional heap allocation.
The emoji image was uploaded to the X3 on COM3 and its flash hash verified. Startup
did not report a crash, but the first automatic refresh could not open
`/.crosspoint/dashboard-response.json` for writing (sync result 8, storage error).
That attempt reported 93,740 bytes free heap and a 61,864-byte minimum since boot;
it failed before the HTTPS request, so these are not successful TLS-sync figures.
Live emoji appearance and another refresh still need confirmation.
The card was subsequently inspected in a Windows reader: FAT32 had a cross-linked
allocation involving `System Volume Information/WPSettings.dat`, and the temporary
Todoist response was incomplete. All 12 readable user/cache files were backed up
with verified checksums before CHKDSK repair. A subsequent scan found no filesystem
problems, and a 1 MiB write/read checksum test passed. The backed-up temporary
response was removed for regeneration; selected projects and task snapshots were
preserved. Refresh on the X3 after reinserting the repaired card remains to be checked.

Thai text support builds successfully and passes eight Windows host suites, including
all 12,829 Thai cluster entries, sara-am ordering, tone/vowel positioning, mixed text,
selected-row inversion and cluster-preserving pagination. Static RAM remains 58,160
bytes; application flash is 6,152,867 bytes (93.9%). The additional 238,857 bytes of
Thai data reside in read-only flash. Artifacts are in `build/dashboard-release/thai/`.
This image was flashed and hash-verified on the X3 with the repaired card inserted.
Startup sync completed successfully (`Sync result 0`): 99,400 bytes free afterward
and a 9,116-byte minimum since boot. The earlier SD response-write failure did not
recur in this capture. On-device visual confirmation of Thai marks is still needed;
the host previews and pagination/positioning tests pass.

Local artifacts and SHA-256 checksums are in `build/dashboard-release/` (ignored
by Git). The app-only and factory images are different: choose neither for an
existing installation until its backup and partition layout have been checked.

## Behavior

- Select 1–16 Todoist projects through the X3's local web setup.
- Portrait **My projects** overview, alphabetically ordered, four projects per page,
  with task counts and a first-task preview. Deleted projects show as unavailable.
- Includes undated and future tasks in selected projects. Excludes completed/deleted
  tasks and tasks assigned to another person. Lists sort by due date, then priority;
  undated tasks follow dated tasks.
- Three text sizes, four tasks per list page, and paginated full task details.
- Thai project names, task titles and details, including vowels/tone marks and
  mixed Thai/English/emoji text. Menus remain in English. Thai wraps at glyph
  cluster boundaries. See [Thai font sources](../../src/dashboard/thai/README.md).
- Monochrome emoji in project names, task titles and details. Joined sequences,
  flags and skin-tone variants stay together across line/page wrapping and invert
  with selected rows. The pinned Noto atlas has artwork for 5,060 Unicode emoji
  sequences/presentation variants; 165 newer unsupported sequences use a visible
  crossed-box placeholder. Skin tones can share monochrome artwork. See
  [emoji sources and regeneration](../../src/dashboard/emoji/README.md).
- Logical Previous/Next buttons move selection; Confirm opens a project or task.
  Confirm on a detail opens a completion prompt, with Cancel selected initially.
  Back on the overview opens **Menu**.
- Back from a project list restores that project's highlight and overview page;
  Back from task details restores the task's highlight and list page. Cancelling
  the completion prompt also retains this return position. Navigation positions
  stay in memory only and are clamped if the list size changes.
- **Sleep now** paints the project overview with a **SLEEPING** status and waits
  for the display refresh before entering deep sleep. The retained image is normal
  for e-paper; use the power button to wake. This action does not open settings.
- Direct HTTPS requests to Todoist API v1, with certificate verification.
- Refresh every 15 minutes. After 60 seconds idle, keep the overview visible on
  e-paper and request a timed deep-sleep wake. Timer wake on battery still needs
  validation on the actual X3 hardware revision.
- Retain the last complete cache on network, parsing or SD errors. An offline
  task completion is refused. If a completion response is lost, persist and
  resend the same command UUID; block further completions until reconciled.
- For recurring tasks, compare the current due date with the cached occurrence
  before sending the completion. This catches an already-advanced occurrence,
  but cannot make a concurrent phone update and completion atomic.

## Build and host verification

On the tested macOS setup, use Python 3.13 and PlatformIO **6.1.19**. PlatformIO
6.2.0 selected a conflicting SCons version with this ESP32 platform; 6.1.18 is
below its minimum supported version. The platform is already pinned in
`platformio.ini`.

The ESP32 SDK builder may create a second PlatformIO installation at
`~/.platformio/penv`. If that nested build reports the same SCons error, pin it
as well using its own `uv`:

```sh
~/.platformio/penv/bin/uv pip install --python ~/.platformio/penv/bin/python platformio==6.1.19
```

```sh
python3.13 -m venv .venv
.venv/bin/python -m pip install platformio==6.1.19 pyyaml clang-format==21.1.8
.venv/bin/pio run -e dashboard
.venv/bin/python test/dashboard/run.py
PATH="$PWD/.venv/bin:$PATH" ./bin/clang-format-fix -g
```

The first build also compiles the customized ESP32 SDK. Host tests need a C++20
compiler and the ArduinoJson
dependency installed by PlatformIO. They cover dates, DST, sorting, cache
interruption/corruption, pagination, assignment filtering, authorization errors,
rate limits, uncertain completion retries and recurring-task conflicts.
Linux/macOS runs enable AddressSanitizer/UndefinedBehaviorSanitizer. On Windows,
set `CXX` to `.venv/Lib/site-packages/ziglang/zig.exe` after installing `ziglang`
in the virtual environment, then run `.venv/Scripts/python.exe test/dashboard/run.py`.

`build/dashboard-preview/` contains SVG previews generated from the actual theme
code using approximate host font metrics. These check layout and pagination;
they do not reproduce the embedded font rasterizer or physical bezel exactly.

## Setup on the device

1. Insert a working SD card and connect to Wi-Fi using **Menu > Todoist setup**.
2. Open the local address shown on the X3 from a computer on the same trusted
   network. Enter the displayed pairing code. A configured X3 keeps its existing
   API token; a new device asks for the token, then requires reopening setup.
3. Tick the projects to display and press **Save and refresh**. The X3 saves the
   selection on SD, closes setup and refreshes those projects. Reopen setup to
   change the selection. If project retrieval fails, the page shows the last saved
   catalogue with a notice. Pending task completions must reconcile before saving.
   Setup closes after saving, leaving the screen, or five minutes. Five incorrect
   code attempts require reopening setup.
4. Confirm **System settings > clock timezone** matches your location. The first
   unconfigured dashboard selects Bangkok if no timezone was previously set.
5. Use **Text size** to cycle through the three sizes.

The pairing page uses local HTTP and must be used on a trusted network. The token
and pending completion command are stored in device NVS without encryption.
Task text and temporary API responses are on the SD card under `/.crosspoint/`.
Changing the token clears the old account's task cache. Neither storage is a
secure vault; a full flash backup can contain credentials and should stay private.

## Memory and limits

The task index is sized to the cache, capped at 512 entries and released before
networking. Four visible task records, one detail record and a bounded 16-project
selection live with the heap-allocated dashboard activity. The project catalogue
is streamed through SD; it is not retained as an unbounded heap list. A temporary
selection draft and verification buffer are allocated only while loading/saving
settings, keeping these roughly 3 KiB records off the task stack. Setup retrieves
the catalogue before allocating its web server, avoiding overlapping server and
TLS allocations. JSON allocations are capped at 64 KiB per parser;
TLS allocations are additional and must be measured on hardware. Only one
framebuffer is used.

The cache accepts up to 512 relevant tasks. Titles and descriptions each accept
up to 1,023 UTF-8 bytes, project names 127 bytes, and IDs 63 bytes. Oversized
relevant records or feeds reject the new snapshot and preserve the previous one.
Project/task fetches use 20-record pages; responses are capped at 256 KiB on SD.

## Hardware acceptance before daily use

No hardware backup, flash or live Todoist acceptance test is implied by a passing
host test or firmware build.

1. Identify the connected serial device, chip, flash capacity and current
   partition table. Read the entire existing flash, verify its byte count and
   checksum, and retain it privately before overwriting firmware. Do not assume
   that the installed TRMNL partition layout matches CrossPoint.
2. Confirm the firmware image and partition layout fit that hardware. Use the
   repository's documented flashing flow only after the backup is verified.
3. Check the actual 528 × 792 panel, bezel margins, buttons and all text sizes.
   Check long titles, descriptions, empty projects and multi-page lists. Save a
   different project selection, reboot and verify only those projects appear.
   Include emoji such as 📚, ❤️, 👩‍💻, 👍🏽 and 🇹🇭 in project/task names; check
   normal and selected rows at all three text sizes, then refresh repeatedly and
   inspect heap logs. Host previews use the actual packed emoji bitmaps, but
   approximate the surrounding text font metrics.
4. Sync a disposable Todoist task and complete it. Test a recurring task and a
   parent task with subtasks separately. Check the phone reflects the result.
5. Disconnect Wi-Fi: cached tasks must remain readable and completion must fail.
   Interrupt a sync and reboot: the last complete snapshot must survive.
6. Monitor the `DASH` serial log's free heap and minimum since boot. Require
   a successful repeated sync without decreasing retained heap or allocation
   failures; record the minimum heap and largest allocatable block.
   Inspect stack high-water marks if a crash or low-stack warning occurs.
7. Verify a 15-minute automatic refresh both on USB and on battery; verify the
   power button wakes the device and measure sleeping battery drain. Timer wake
   currently keeps the X3 power latch asserted, so its electrical behavior must
   be confirmed rather than inferred from compilation.

The original TRMNL installation has a verified full-flash backup retained locally.

## Continue on Windows

Use this branch: `feature/x3-todoist-dashboard`. If transferring the incremental
Git bundle from the Mac, first clone upstream so its base commit is available.
Run these commands in Git Bash, adjusting the bundle path:

```sh
git clone --recurse-submodules https://github.com/crosspoint-reader/crosspoint-reader.git X3
cd X3
git fetch /path/to/x3-todoist-dashboard.bundle feature/x3-todoist-dashboard:feature/x3-todoist-dashboard
git switch feature/x3-todoist-dashboard
git submodule update --init --recursive
py -3.13 -m venv .venv
.venv/Scripts/python.exe -m pip install platformio==6.1.19 pyyaml
export PYTHONUTF8=1 PYTHONIOENCODING=utf-8
.venv/Scripts/pio.exe run -e dashboard
.venv/Scripts/pio.exe device list
```

The Mac's `.venv`, build output and credentials are not part of the commit or Git
bundle. Recreate the environment on Windows. Host sanitizer tests additionally
need a compatible C++ toolchain; the documented host test results were on macOS.
Start the hardware session with serial identification and a verified full flash
backup, then inspect partition compatibility before flashing. The separate
experimental firmware ZIP contains the Mac-built images and checksums if needed.

In PowerShell, set `$env:PYTHONUTF8 = '1'` and
`$env:PYTHONIOENCODING = 'utf-8'` before building. Without these settings,
PlatformIO's output thread can fail with `UnicodeEncodeError` when the translation
generator prints language names through the Windows legacy encoding.

For a stable backup, use esptool's `--after no-reset-stub` option with
`read-flash`, then `--before no-reset --after no-reset-stub` with `verify-flash`.
This keeps the installed application from changing NVS between reading and
verification. Save the full detected flash capacity and a SHA-256 checksum
outside the repository and cloud-synced folders. On the tested 16 MiB X3,
verification passed this way; rebooting TRMNL between operations changed an NVS
sector and caused a digest mismatch. These read/verify operations do not install
firmware; a reset resumes the existing application.

Windows session on 2026-09-26: identified the connected ESP32-C3 revision v0.4
with 16 MiB flash, secure boot disabled and flash encryption disabled. Saved and
verified a full TRMNL backup outside the repository. Its app partitions are only
`0x1d0000` bytes each, so an app-only dashboard upload cannot use that layout.
Python 3.13 and PlatformIO 6.1.19 are installed locally, including the nested
PlatformIO environment. The Windows dashboard build passed without compiler
warnings: 58,024 bytes of static RAM and a 5,563,232-byte application image,
within the `0x640000`-byte app partition. Runtime heap use remains unverified.

The installed SCons 4.8.1 Windows launcher needed a local workaround: use
`subprocess.call` in `exec_spawn`, and launch `riscv32-esp-elf-*` executables
directly in `spawn` rather than through `cmd.exe`. The original installed
`SCons/Platform/win32.py` is retained as `win32.py.crosspoint-backup` beside it
under `~/.platformio/packages/tool-scons/scons-local-4.8.1/`. Reinstalling the
SCons package removes this workaround. Earlier project-level overrides were
unsuccessful and removed. Directly launching the exact stalled compiler command
succeeded; the patched full build then completed. The isolated `os.spawnve`
failure resembles [CPython issue 137934](https://github.com/python/cpython/issues/137934),
but the precise cause of the suspended shell processes has not been established.

Flashed and hash-verified the bootloader at `0x0`, partition table at `0x8000`,
`boot_app0.bin` at `0xe000`, and dashboard application at `0x10000` using esptool
5.4.0 directly on COM3 (DIO, 80 MHz, 16 MiB). PlatformIO's `nobuild` upload failed
at argument parsing before writing, so it was not used for installation. The
original full TRMNL backup remains available privately. Post-flash serial capture
and the screenshot command returned no bytes; physical power-on/display checks,
SD access, live Todoist sync and battery wake acceptance remain pending.

The user subsequently confirmed that the dashboard boots. The first Wi-Fi
connection reached the NTP-success settings save, then aborted. The recovered
flash coredump showed `std::bad_alloc` while copying 65 `SettingInfo` entries
(8,580 bytes) in `getSettingsList`, called by `CrossPointSettings::toJson` from
`WifiSelectionActivity::checkConnectionStatus`. Persistence now reads the shared
board-filtered metadata by const reference; only UI callers copy it and build
dynamic font/dictionary options. No new dashboard heap buffer is introduced.
The matching original ELF and dump are retained in ignored
`build/dashboard-crash/`.

The fix passed `pio run -e dashboard`, the repository formatting wrapper and
`git diff --check`, with no compiler warnings. The rebuilt application is
5,580,832 bytes; PlatformIO reports 58,160 bytes of static RAM. It was written
at `0x10000` and hash-verified without replacing the partition table or NVS.
Retrying Wi-Fi on hardware and completing Todoist setup remain the next checks.

The next device photo, taken after API-key submission, showed
`HalFile::close()` asserting `impl != nullptr` at `HalStorage.cpp:260`.
`Store::startWrite()` and `finishWrite()` both closed the default-constructed
output handle unconditionally. Cleanup now checks whether the file is open;
starting another write uses the same guarded cleanup. This adds no allocation.
The dashboard build and formatting checks passed. The application and matching
ELF are retained in ignored `build/dashboard-release/windows-cache-close-fix/`.
After USB reconnection, the recovered coredump confirmed the call chain
`DashboardActivity::refresh()` -> `Store::finishWrite()` -> `HalFile::close()`.
The application (5,580,848 bytes) was flashed at `0x10000` on COM3 and
hash-verified, then the device was reset. NVS and the partition table were
preserved. Live sync success remains unverified.
Hardware verification should cover the first successful task sync, a second
refresh, and exiting or failing a sync before any cache write starts.

The user reported a non-crashing "Refresh failed" after that update. Inspecting
the previous coredump's `DashboardActivity::refresh` frame with its matching ELF
showed `SyncResult::Transport`. HTTP diagnostics now log the endpoint category
(excluding IDs and query parameters), status, completion, received byte count,
storage failure flag, errno and TLS error codes. They do not log authorization
headers or response bodies. The diagnostic dashboard build passed and was
flashed/hash-verified on COM3; artifacts are retained in ignored
`build/dashboard-release/windows-https-diagnostics/`. Serial capture works while
the device is awake, but dashboard idle sleep disconnects USB after 60 seconds.
Live serial capture identified `mbedtls_ssl_setup` error `-0x7F00`
(`MBEDTLS_ERR_SSL_ALLOC_FAILED`). The cache index is now allocated to the
validated record count instead of reserving all 512 entries at startup, and is
released before sync. On unsuccessful sync the old SD cache is reloaded after
Wi-Fi shutdown; successful commits rebuild the index. No TLS code uses the
index. An index-release/reload regression case was added to the storage host
test, but the host suite has not been run on this Windows machine.

The first memory fix built and was flashed/hash-verified. Sync-start free heap
rose from 70,400 to 113,404 bytes in the captured runs. TLS then reached RSA
certificate verification but failed with `0x4290` (`RSA_PUBLIC_FAILED` plus
`MPI_ALLOC_FAILED`); minimum free heap was 268 bytes. The dashboard SDK profile
now enables asymmetric TLS records: 16,384 incoming bytes and 4,096 outgoing
bytes, reducing buffer allocation by 12,288 bytes per connection. Certificate
verification remains enabled. The SDK and firmware rebuild passed, and the
generated configuration was checked for these values. Artifacts are retained
in ignored `build/dashboard-release/windows-tls-buffer-fix/`. It was flashed
and hash-verified. Live sync progressed through account and project retrieval,
then the tasks request returned `ESP_ERR_HTTP_EAGAIN` before any response data
arrived. Minimum free heap was 3,516 bytes, so memory margin remains tight.
The request timeout has been increased from 4.5 to 10 seconds; the application
build passed. That update is retained in ignored
`build/dashboard-release/windows-timeout-fix/`. It was flashed/hash-verified;
the captured first sync completed successfully (`Sync result 0` at 8,840 ms).
Sync-start heap was 113,256 bytes and the recorded minimum was 1,968 bytes.
Subsequent refreshes timed out on tasks and projects; the user still reported
"Refresh failed". TLS memory margin remains tight, so one successful sync
does not establish reliability. The dashboard now also calls
`FontCacheManager::releaseSdFontCaches()` under its render lock before sync,
matching the existing web-server activity's memory-release approach. That
application build passed. The first upload lost USB mid-write; the retry
completed and hash-verified. Artifacts are retained in
`build/dashboard-release/windows-font-cache-fix/`. The captured boot sync
succeeded at 12,090 ms, with 125,760 bytes free at sync start and a minimum of
11,976 bytes since boot. A second captured boot sync also succeeded at
16,844 ms, with a minimum of 8,664 bytes since boot. Both captures start with
boot-relative timestamps. The user then confirmed that tasks appear and a
further manual refresh succeeds. Long-duration and battery-wake testing remain
outside this verification.
