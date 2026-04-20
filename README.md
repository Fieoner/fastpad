# fastpad

USB HID gamepad firmware for CH32V307. 32 digital sensors mapped to 8 buttons, USB High-Speed at 8000Hz polling rate.

100kHz button scanning with eager press (2 readings, 20μs) and debounced release (4 readings, 40μs).

## Build & flash

You need a RISC-V GCC toolchain and `wchisp`:

```
brew install riscv64-elf-gcc
cargo install wchisp
```

Pull the WCH SDK vendor files (one time):

```
./setup.sh
```

Build:

```
make
```

Flash: hold BOOT0, press RST, release BOOT0, then:

```
make flash
```

Press RST to boot. Plug the USB HS port into your computer.

## Button mapping

See `buttons.md` for pin assignments. Each button is 4 sensors OR'd together. Edit `BUTTON_SENSOR_MASK` in `src/buttons.c` to change the mapping.

## License

GPLv3. Vendor files (pulled by `setup.sh`) are Apache 2.0 from WCH.
