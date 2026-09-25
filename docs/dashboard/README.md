# Personal X3 Todoist dashboard

This experimental firmware extends CrossPoint at `1d61100f90d2e7e32965c14301320d9989a7ab71`.
Select the `dashboard` build environment. Existing reader targets remain available.

Validation on 2026-09-26: `pio run -e dashboard` passed without compiler warnings;
all four host suites passed with address/undefined-behavior sanitizers; repository
formatting and `git diff --check` passed. The build reports 58,024 bytes of static
RAM and 5,548,715 bytes of application flash (84.7% of its partition). Static RAM
does not include runtime dashboard, Wi-Fi or TLS allocations. Hardware USB access,
flash backup, installation, battery wake and live Todoist tests remain pending.

Local artifacts and SHA-256 checksums are in `build/dashboard-release/` (ignored
by Git). The app-only and factory images are different: choose neither for an
existing installation until its backup and partition layout have been checked.

## Behavior

- Portrait overview with Today, Overdue and Upcoming (tomorrow through seven days ahead).
- Excludes undated tasks, completed/deleted tasks and tasks assigned to another person.
- Three text sizes, four tasks per list page, and paginated full task details.
- Logical Previous/Next buttons move selection; Confirm opens a section or task.
  Confirm on a detail opens a completion prompt, with Cancel selected initially.
  Back on the overview opens the dashboard menu.
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
compiler with AddressSanitizer/UndefinedBehaviorSanitizer and the ArduinoJson
dependency installed by PlatformIO. They cover dates, DST, sorting, cache
interruption/corruption, pagination, assignment filtering, authorization errors,
rate limits, uncertain completion retries and recurring-task conflicts.

`build/dashboard-preview/` contains SVG previews generated from the actual theme
code using approximate host font metrics. These check layout and pagination;
they do not reproduce the embedded font rasterizer or physical bezel exactly.

## Setup on the device

1. Insert a working SD card and connect to Wi-Fi using **Menu > Todoist setup**.
2. Open the local address shown on the X3 from a computer on the same trusted
   network. Enter the displayed pairing code and your personal Todoist API token.
   Do not paste the token into chat or commit it to this repository.
3. Setup closes after saving, leaving the screen, or five minutes. Five incorrect
   code attempts require reopening setup. The dashboard then refreshes.
4. Confirm **System settings > clock timezone** matches your location. The first
   unconfigured dashboard selects Bangkok if no timezone was previously set.
5. Use **Text size** to cycle through the three sizes.

The pairing page uses local HTTP and must be used on a trusted network. The token
and pending completion command are stored in device NVS without encryption.
Task text and temporary API responses are on the SD card under `/.crosspoint/`.
Changing the token clears the old account's task cache. Neither storage is a
secure vault; a full flash backup can contain credentials and should stay private.

## Memory and limits

The firmware retains a fixed 512-entry index, four visible task records and one
detail record. It reads full records from SD as needed. These buffers live on the
heap for the lifetime of the dashboard activity, avoiding a large task stack or
permanent global allocation. JSON allocations are capped at 64 KiB per parser;
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
   Check long titles, descriptions, empty sections and multi-page lists.
4. Sync a disposable Todoist task and complete it. Test a recurring task and a
   parent task with subtasks separately. Check the phone reflects the result.
5. Disconnect Wi-Fi: cached tasks must remain readable and completion must fail.
   Interrupt a sync and reboot: the last complete snapshot must survive.
6. Monitor the `DASH` serial log's free heap and minimum since boot. Require more
   than 50 KiB free through sync/completion, then repeat to check for leaks.
   Inspect stack high-water marks if a crash or low-stack warning occurs.
7. Verify a 15-minute automatic refresh both on USB and on battery; verify the
   power button wakes the device and measure sleeping battery drain. Timer wake
   currently keeps the X3 power latch asserted, so its electrical behavior must
   be confirmed rather than inferred from compilation.

The original TRMNL installation remains untouched until the hardware backup and
compatibility checks are complete.

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
.venv/Scripts/pio.exe run -e dashboard
.venv/Scripts/pio.exe device list
```

The Mac's `.venv`, build output and credentials are not part of the commit or Git
bundle. Recreate the environment on Windows. Host sanitizer tests additionally
need a compatible C++ toolchain; the documented host test results were on macOS.
Start the hardware session with serial identification and a verified full flash
backup, then inspect partition compatibility before flashing. The separate
experimental firmware ZIP contains the Mac-built images and checksums if needed.
