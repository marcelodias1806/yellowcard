# Troubleshooting

Always capture the board variant, display controller, firmware tag, PlatformIO
version, and sanitized serial output before changing pinout or calibration.

## Device Not Detected

1. Use a known data-capable USB cable.
2. Try the CYD connector intended for serial communication.
3. Reconnect the board and compare `pio device list` output.
4. Check whether another serial monitor has the port open.
5. Confirm the board is powered and the USB bridge appears in the OS.

## CH340 Serial Driver

The validated board uses a CH340 USB serial bridge. Modern Linux kernels often
support it automatically. On Windows or macOS, install a trusted vendor/OS
driver only if the bridge does not appear. After driver installation, reconnect
the device and run:

```sh
pio device list
```

## WSL2 / usbipd

USB serial hardware connected to Windows is not automatically available in
WSL2. From an administrator PowerShell:

```powershell
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

Then in WSL:

```sh
lsusb
ls -l /dev/ttyUSB*
pio device list
```

Do not hardcode a BUSID; obtain it from `usbipd list`. Reattach after a reboot
or physical reconnect when required.

## PlatformIO Port Detection

List detected devices:

```sh
pio device list
```

Use an explicit port if auto-detection is ambiguous:

```sh
pio run -e esp32dev -t upload --upload-port <SERIAL_PORT>
pio device monitor --baud 115200 --port <SERIAL_PORT>
```

Linux users may need membership in the distribution's serial-access group.
Log out/in after changing group membership.

## Upload Errors

- Close serial monitors and other applications using the port.
- Confirm the `esp32dev` environment and correct port.
- Use a short, reliable USB cable and stable power.
- If the board does not enter the bootloader automatically, follow its BOOT/EN
  button sequence rather than repeatedly changing firmware settings.
- Never erase flash unless you understand that local data and the original
  vendor firmware may be lost.

## Wrong Display Colors or Blank Display

- Confirm the board uses ST7789, not ILI9341.
- Confirm the 2-USB style hardware revision.
- Check backlight GPIO 21 and active-HIGH behavior.
- The validated configuration uses `TFT_BGR`, 240×320, and rotation 0.
- Verify display pins in `platformio.ini` against the board schematic.

## Different CYD Controller

Other ESP32-2432S028 boards may require a different TFT_eSPI driver macro,
geometry, color order, pins, or rotation. Treat adaptation as a separate board
port. Do not change the validated target configuration and assume both variants
remain supported without hardware testing.

## Touch Does Not Respond

- Verify this board actually uses XPT2046.
- Confirm CLK 25, MOSI 32, MISO 39, CS 33, and IRQ 36.
- The touch controller uses a dedicated HSPI instance.
- Ensure no additional code calls `touched()` or `getPoint()` outside the LVGL
  input callback.
- Do not change calibration until raw touch and board orientation are verified.
- Resistive panels require pressure and may differ mechanically by unit.

## Wi-Fi Remains Offline

- Offline is expected when `include/wifi_secrets.h` is absent.
- Check that the local file matches `wifi_secrets.example.h` exactly.
- Configure no more than four networks.
- Confirm a configured 2.4 GHz SSID is available; classic ESP32 does not use
  5 GHz Wi-Fi.
- Review the serial state transitions without publishing credentials or private
  SSIDs.

## BLE Scan Fails

- Wait until a Wi-Fi scan or connection attempt finishes.
- Check serial output for Wi-Fi power-save or NimBLE initialization errors.
- Confirm NimBLE-Arduino 2.5.1 is selected by PlatformIO.
- Avoid repeatedly opening/leaving the screen faster than the state machine can
  stop the five-second scan.
- Compare free/minimum heap against the observed validation range.

## BLE/Wi-Fi Coexistence

The ESP32 shares radio resources between Wi-Fi and BLE. YellowCard requires
`WIFI_PS_MIN_MODEM` and serializes discovery scans with `radio_scan_lock`.
Some throughput or scan latency variation is normal. A BLE request may be
deferred while Wi-Fi scans or connects; it should not force a disconnect.

## TFT_eSPI `TOUCH_CS` Warning

This warning is expected:

```text
TOUCH_CS pin not defined, TFT_eSPI touch functions will not be available
```

TFT_eSPI is used for the ST7789 only. Touch is handled by
`XPT2046_Touchscreen` on a dedicated SPI bus, so defining TFT_eSPI touch support
is neither required nor desired.

## Reporting a Problem

Use the GitHub bug template and include sanitized logs. Remove passwords,
tokens, private SSIDs, private IP addresses, device identifiers, and any other
sensitive environment data before submission.
