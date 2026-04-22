#!/usr/bin/env python3
"""IAP firmware update tool for CH32V307 fastpad.

Sends new firmware to the running app over USB HID report 0x0F.
The device erases its own flash and writes the new firmware from RAM,
then resets. Power loss during flash write requires physical BOOT0+RST.
"""

import sys
import time
import struct
import zlib
import usb.core

VID = 0x1209
PID = 0xB196
REPORT_ID = 0x0F
CHUNK_SIZE = 60  # bytes per DATA command (63 - 3 header bytes)

IAP_CMD_BEGIN  = 0x01
IAP_CMD_DATA   = 0x02
IAP_CMD_FINISH = 0x03

IAP_STATUS_IDLE  = 0x00
IAP_STATUS_READY = 0x01
IAP_STATUS_ERROR = 0x03
IAP_STATUS_DONE  = 0x04


def find_device():
    dev = usb.core.find(idVendor=VID, idProduct=PID)
    if dev is None:
        return None
    if dev.is_kernel_driver_active(0):
        dev.detach_kernel_driver(0)
    return dev


def set_report(dev, data):
    """Send SET_REPORT (Feature, report 0x0F)."""
    buf = bytes([REPORT_ID]) + data
    buf = buf.ljust(64, b'\x00')
    dev.ctrl_transfer(0x21, 0x09, 0x030F, 0, buf)


def get_status(dev):
    """GET_REPORT (Feature, report 0x0F). Returns status byte."""
    resp = dev.ctrl_transfer(0xA1, 0x01, 0x030F, 0, 64)
    return resp[1] if len(resp) > 1 else 0xFF


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <firmware.bin>")
        sys.exit(1)

    fw_path = sys.argv[1]
    with open(fw_path, "rb") as f:
        firmware = f.read()

    fw_crc = zlib.crc32(firmware) & 0xFFFFFFFF
    print(f"Firmware: {fw_path} ({len(firmware)} bytes, CRC32={fw_crc:#010x})")

    if len(firmware) > 24 * 1024:
        print(f"Error: firmware too large ({len(firmware)} > {24*1024} bytes)")
        sys.exit(1)

    dev = find_device()
    if dev is None:
        print("Device not found!")
        sys.exit(1)

    # BEGIN: send size + CRC
    begin_data = bytes([IAP_CMD_BEGIN])
    begin_data += struct.pack('<I', len(firmware))
    begin_data += struct.pack('<I', fw_crc)
    set_report(dev, begin_data)
    time.sleep(0.05)

    status = get_status(dev)
    if status != IAP_STATUS_READY:
        print(f"Error: device not ready after BEGIN (status={status:#x})")
        sys.exit(1)

    print("Device ready, sending firmware...")

    # DATA chunks
    total_chunks = (len(firmware) + CHUNK_SIZE - 1) // CHUNK_SIZE
    for i in range(total_chunks):
        offset = i * CHUNK_SIZE
        chunk = firmware[offset:offset + CHUNK_SIZE]

        data_cmd = bytes([IAP_CMD_DATA])
        data_cmd += struct.pack('<H', i)
        data_cmd += chunk
        set_report(dev, data_cmd)

        if (i + 1) % 20 == 0 or i == total_chunks - 1:
            pct = 100 * (i + 1) // total_chunks
            print(f"\r  Writing: {pct}% ({i+1}/{total_chunks})", end="", flush=True)

    print()

    # FINISH — device may disconnect immediately as it starts flashing
    set_report(dev, bytes([IAP_CMD_FINISH]))

    try:
        time.sleep(0.1)
        status = get_status(dev)
        if status == IAP_STATUS_ERROR:
            print("Error: CRC mismatch or incomplete transfer!")
            sys.exit(1)
        elif status == IAP_STATUS_DONE:
            print("CRC verified! Flashing...")
    except usb.core.USBError:
        print("Device is flashing (disconnected as expected)...")

    # Wait for device to come back
    print("Waiting for device to re-enumerate...", end="", flush=True)
    for _ in range(30):
        time.sleep(0.5)
        dev = find_device()
        if dev:
            print(" done!")
            print("Update complete!")
            return
        print(".", end="", flush=True)
    print("\nDevice did not re-enumerate (may need BOOT0+RST recovery)")


if __name__ == "__main__":
    main()
