# Prebuilt probe firmware

These binaries are the build validated on 2026-10-15 against an AT32F405
target. SHA-256 hashes are in `sha256.json`.

Flash addresses and options (ESP32-P4, 32 MB flash, DIO, 40 MHz):

```sh
esptool --chip esp32p4 --port PORT --baud 460800 write-flash \
  --flash-mode dio --flash-size 32MB --flash-freq 40m \
  0x2000  bootloader.bin \
  0x8000  partition-table.bin \
  0x10000 p4_swd_probe.bin
```

Rebuilding from this source tree with ESP-IDF v5.5.4 produces an app image of
about the same size. Build metadata and embedded diagnostic strings differ, so
compare behavior before replacing a known-good binary.
