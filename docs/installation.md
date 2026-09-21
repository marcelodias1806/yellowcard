# Installation

## Requirements

- Git.
- Visual Studio Code with the PlatformIO IDE extension, or PlatformIO Core.
- A data-capable USB cable.
- CH340 driver support where the operating system does not provide it.
- The validated ST7789/2-USB CYD variant, or an explicitly adapted build.

## Clone

After the repository is published, replace the placeholder with its real URL:

```sh
git clone <REPOSITORY_URL>
cd yellowcard
```

## Build

No private Wi-Fi configuration is required for a public build:

```sh
pio run -e esp32dev
```

PlatformIO downloads the pinned libraries on the first build. Successful size
output for the validated v0.4.0 release is approximately 29.4% static RAM and
38.8% of the `huge_app` application partition.

## Find the Serial Port

Connect the board and run:

```sh
pio device list
```

Linux commonly exposes the CH340 as `/dev/ttyUSB0`; the exact path is not
guaranteed. Windows commonly assigns a COM port. Unplug/reconnect the board and
compare the list if identification is unclear.

## Upload

Uploading changes device flash. Review the board and port first, then run:

```sh
pio run -e esp32dev -t upload
```

When automatic port detection is ambiguous:

```sh
pio run -e esp32dev -t upload --upload-port <SERIAL_PORT>
```

## Serial Monitor

```sh
pio device monitor --baud 115200
```

If the port must be explicit:

```sh
pio device monitor --baud 115200 --port <SERIAL_PORT>
```

Close the monitor before uploading if the operating system does not allow both
processes to open the port.

## Optional Wi-Fi Setup

The firmware works offline by default. To configure known local networks, read
[wifi-configuration.md](wifi-configuration.md). Never commit the resulting
`include/wifi_secrets.h`.

## WSL2 USB Passthrough

From an administrator PowerShell on Windows:

```powershell
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

Then verify from WSL:

```sh
lsusb
ls -l /dev/ttyUSB*
pio device list
```

The BUSID changes by machine and port; do not copy a hardcoded example.
Reattachment may be required after reconnecting the device or restarting WSL.

See [troubleshooting.md](troubleshooting.md) for upload, driver, display,
touch, Wi-Fi, and BLE diagnostics.
