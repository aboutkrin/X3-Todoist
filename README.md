# X3 Todoist

A Todoist dashboard for the **Xteink X3** e-reader, built on
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).
It brings a focused view of your projects and tasks to an e-ink screen.

![An Xteink e-reader running CrossPoint Reader](docs/images/cover.jpg)

## What it does

- Choose up to 16 Todoist projects from a setup page hosted on the device.
- See due and overdue tasks in **Today**, and browse tasks in selected projects,
  including future and undated tasks.
- Read project names and task text in Thai, English, and monochrome emoji.
- Open task details and complete tasks with confirmation and recovery for
  uncertain network responses.
- Keep the last saved task snapshot on the SD card for offline reading.
- Choose among three text sizes, refresh over Wi-Fi, and wake from the sleeping
  display without losing your place.

The dashboard uses Todoist's API over HTTPS. Sync is started from the device;
it does not run automatically at startup.

## Set up the dashboard

1. Insert a microSD card and connect the X3 to Wi-Fi.
2. Open **Menu > Todoist setup**. On a device already configured for Todoist,
   choose the projects to show. On a new device, enter a Todoist API token and
   reopen setup after saving it.
3. On a computer connected to the same trusted network, open the local address
   shown on the X3 and enter the pairing code displayed on the device.
4. Select your projects and choose **Save and refresh**.
5. Confirm the device's clock and timezone in **System settings**. Use **Text
   size** to cycle through the dashboard's three text sizes.

Use the setup page only on a trusted local network. The API token is stored in
device NVS without encryption. Task snapshots and temporary API responses are
stored on the SD card under `/.crosspoint/`. Keep SD-card contents, flash backups,
and serial logs private. See the [device guide](docs/dashboard/README.md) for
setup behavior, data limits, and hardware validation details.

## Install and device safety

This is experimental, device-specific firmware. Before installing it, back up
the device's full flash and verify the existing partition layout. Do not assume
that the X3's factory or third-party firmware uses the same layout as CrossPoint.
The development guide documents the tested build and flashing workflow, current
validation limits, and the distinction between app-only and factory images.

## Build from source

The default PlatformIO environment is `dashboard`.

```sh
git clone --recurse-submodules https://github.com/aboutkrin/X3-Todoist.git
cd X3-Todoist
python -m venv .venv

# Windows PowerShell
.venv\Scripts\Activate.ps1

# Install the pinned PlatformIO version and YAML dependency
python -m pip install platformio==6.1.19 pyyaml
pio run -e dashboard
```

On macOS or Linux, activate the environment with `source .venv/bin/activate`.
The project also needs a C++20 compiler for its host-side dashboard checks.
Windows compiler setup, host verification, and additional build notes are in
the [development and device guide](docs/dashboard/README.md).

## Development and validation

The dashboard extends CrossPoint Reader while preserving its existing reader
build environments. For implementation notes, behavior details, memory limits,
and the hardware acceptance checklist, see
[`docs/dashboard/README.md`](docs/dashboard/README.md).

Run the dashboard host checks with:

```sh
python test/dashboard/run.py
```

The host checks cover dashboard logic and parsing; they do not replace testing
on an X3. Verify display layout, repeated Wi-Fi sync, task completion, SD-card
behavior, sleep/wake, and heap use on the actual device before relying on it.

## Credits and licenses

CrossPoint Reader and the FreeInk SDK provide the reader, hardware, and rendering
foundation. This project retains the original [MIT license](LICENSE). The
dashboard's Noto Emoji and Noto Sans Thai assets include their own license
notices; see the [emoji source notes](src/dashboard/emoji/README.md) and
[Thai font notes](src/dashboard/thai/README.md).
