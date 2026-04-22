# Software Bootloader Entry — Status

## Goal

Enter the WCH ISP bootloader (VID 4348:55e0) from running firmware via USB command, eliminating the need to physically press BOOT0+RST.

## Current Approach

Based on [ch32v003-bootloader-docs](https://github.com/basilhussain/ch32v003-bootloader-docs), the CH32V003 enters its ROM bootloader by:

1. Unlocking `FLASH->MODEKEYR` with KEY1/KEY2
2. Setting bit 14 (`0x4000`) in `FLASH->STATR`
3. Clearing reset flags via `RCC->RSTSCKR |= RCC_RMVF`
4. Software reset via `NVIC_SystemReset()`

We're attempting the same on the CH32V307, which has the same `FLASH->MODEKEYR` and `FLASH->STATR` registers. **Untested** — the CH32V003 docs note a `START_MODE` option byte must be set to 1 first (via SWIO/WCH-LinkE). It's unknown whether CH32V307 requires a similar prerequisite.

## What Works

- `make bootloader` sends a USB GET_REPORT for feature report 0x0F
- Firmware handles it in `Handle_Feature_Get()`, sets `bootloader_request = 1`
- Main loop sees the flag, waits 50ms (for USB transfer to complete), then calls `Enter_Bootloader()`
- `Enter_Bootloader()` does the FLASH MODEKEYR + STATR + reset sequence

## What Doesn't Work Yet

### 1. Linux hidraw rejects all HID report writes/reads for custom reports

Every attempt to use the Python `hid` library (which wraps hidraw) fails with `Protocol error`:

- `d.write(bytes([0x03, 0x00]))` — Output report → Protocol error
- `d.send_feature_report(bytes([0x0F, 0x00]))` — Feature report SET → Protocol error
- `d.get_feature_report(0x0F, 2)` — Feature report GET → Protocol error

This suggests the Linux kernel's HID descriptor parser isn't matching our custom reports correctly, or the hidraw ioctls have stricter validation than expected.

**Workaround:** Use `pyusb` (`usb.core`) to send a raw USB control transfer, bypassing hidraw entirely:

```python
import usb.core
dev = usb.core.find(idVendor=0x1209, idProduct=0xB196)
if dev.is_kernel_driver_active(0):
    dev.detach_kernel_driver(0)
dev.ctrl_transfer(0xA1, 0x01, 0x030F, 0, 2)
```

This also failed with I/O error, but that was because the firmware was resetting before the USB transfer completed (fixed by adding 50ms delay). **Needs retesting after the delay fix.**

### 2. ROM bootloader entry is unverified on CH32V307

The FLASH STATR bit 14 trick is documented for CH32V003. It's plausible on CH32V307 (same register layout) but:

- May require a `START_MODE` option byte to be programmed first
- May not exist on this chip family at all
- Previous attempt to jump directly to ROM at `0x1FFFF000` crashed the device

If the STATR trick doesn't work, fallback options are:
- **IAP (In-Application Programming):** Firmware receives new binary over USB, copies flash-write routine to RAM, erases/writes flash from RAM
- **Hardware mod:** Wire a GPIO to the BOOT0 pin

## Files

- `src/main.c` — `Enter_Bootloader()` (FLASH STATR approach), main loop delay + flag check
- `src/usb_device.c` — `Handle_Feature_Get()` triggers bootloader on report 0x0F, `Handle_Feature_Set()` handles 0x03/0x0F
- `src/usb_desc.c` — Report 0x0F declared as 1-byte Feature report
- `src/adp_reports.h` — `REPORT_ID_BOOTLOADER 0x0F`
- `Makefile` — `make bootloader` target
- `99-fastpad.rules` — udev rules for device access (VID 1209:B196 + WCH ISP 4348:55E0)

## Next Step

Flash the latest build (with 50ms delay fix) and test `make bootloader` with the pyusb command. If the device shows up as 4348:55E0 in `lsusb`, the STATR trick works. If not, investigate the `START_MODE` option byte or implement IAP.
