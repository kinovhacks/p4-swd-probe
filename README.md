# p4-swd-probe

An experimental CMSIS-DAP SWD probe running on an ESP32-P4. It turns an
ESP32-P4 board and its USB-UART bridge into an OpenOCD-compatible debugger for
a 3.3 V SWD target, without a J-Link or a dedicated USB probe.

The project is a P4 port of the ARM CMSIS-DAP command layer plus a small host
bridge that carries DAP packets over the board's serial port:

```text
OpenOCD (CMSIS-DAP TCP backend)
        |  127.0.0.1:4441
        v
host/probe.py  (serial <-> TCP bridge)
        |  UART0, 921600 8N1
        v
ESP32-P4 firmware  (CMSIS-DAP subset)
        |  GPIO20 SWCLK, GPIO21 SWDIO, GPIO22 nRESET
        v
target MCU
```

## Why this exists

I bricked my keyboard (an AT32F405-based board) while flashing firmware. The
fix needed a teardown and a J-Link, and I didn't have a J-Link. I did have an
ESP32-P4 dev board, so I worked with AI to turn it into an SWD probe that
OpenOCD can drive, and used it to restore the keyboard's firmware myself.

This repo is that tool, cleaned up so you can use it too. It's part of
[Kinov Hacks](https://kinovhacks.com): hackable hardware, extended with AI.
Follow along on [X](https://x.com/kinovhacks) and
[YouTube](https://youtube.com/@kinovhacks).

## Status

This is a lab tool, not a product and not a certified CMSIS-DAP implementation.

- SWD only. JTAG, SWO, UART passthrough and Atomic Commands are not implemented.
- The firmware reports a 1024-byte packet size and passes OpenOCD transfers up
  to that limit. It is an experimental subset of CMSIS-DAP 2.1.2.
- Not a J-Link and not compatible with J-Link software.
- No target voltage sensing, no level shifting and no target power. Use 3.3 V
  logic only; never connect 5 V signals.
- The tested board still enumerates as a USB serial port. Native USB
  CMSIS-DAP is not implemented.
- The host bridge binds to `127.0.0.1` only, but it has no authentication.
  Any local process that can reach the port can use the probe.

## Tested hardware

| Item | Value |
|---|---|
| Probe board | ESP32-P4 v1.3, 32 MB flash (Waveshare ESP32-P4-WIFI6 class) |
| ESP-IDF | v5.5.4, target `esp32p4`, chip revisions 1.0-1.99 |
| Probe UART | UART0, GPIO37 TX / GPIO38 RX, 921600 8N1 |
| Target SWD | GPIO20 SWCLK, GPIO21 SWDIO, GPIO22 nRESET (open drain) |
| Target tested | Artery AT32F405RCT7 (Cortex-M4) |
| Host OpenOCD | 0.12.0+dev-02750-g751e5af67 with CMSIS-DAP TCP backend |

Verify the pin headers and the UART wiring on your own board before connecting
a target. The UART pins, SWD pins and baud rate are compile-time constants in
`main/DAP_config.h`. The host bridge assumes 921600 baud (`BAUD` in
`host/probe.py`); change both sides together.

## Build the probe firmware

```sh
. /path/to/esp-idf/export.sh    # ESP-IDF v5.5.4
idf.py build
```

The build produces `build/bootloader/bootloader.bin`,
`build/partition_table/partition-table.bin` and `build/p4_swd_probe.bin`.
The build configuration is pinned in `sdkconfig` and `sdkconfig.defaults`.

Prebuilt binaries are in `firmware/` with SHA-256 hashes in
`firmware/sha256.json`. Flash them at the addresses below (DIO, 32 MB, 40 MHz):

```sh
esptool --chip esp32p4 --port PORT --baud 460800 write-flash \
  --flash-mode dio --flash-size 32MB --flash-freq 40m \
  0x2000  firmware/bootloader.bin \
  0x8000  firmware/partition-table.bin \
  0x10000 firmware/p4_swd_probe.bin
```

## Host setup

Requirements: Python 3 and `pyserial`, plus an OpenOCD build that includes the
CMSIS-DAP TCP backend (`src/jtag/drivers/cmsis_dap_tcp.c`). Stable OpenOCD
releases may not include that backend; use a development build from source.

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r host/requirements.txt
```

Start the bridge (replace `PORT` with the P4 board's serial port; it may
enumerate as a WCH/CH34x device, for example `/dev/cu.usbmodem*` or
`/dev/cu.wchusbserial*` on macOS, or `COMx` on Windows):

```sh
python3 host/probe.py --port PORT serve
```

The bridge listens on `127.0.0.1:4441`. In another terminal:

```sh
openocd -f openocd/p4-swd-probe.cfg -f openocd/at32f405.cfg \
  -c 'init; halt; reg pc; resume; shutdown'
```

`openocd/at32f405.cfg` contains an AT32F405 example target. For another
Cortex-M target, replace that file or write your own target config.

## Self-test

With no target connected, the probe can exercise its own protocol and the GPIO
input buffers:

```sh
python3 host/probe.py --port PORT test
```

This checks DAP identification, packet limits, malformed-packet rejection, the
pin self-readback and 1000 round trips. It prints a JSON report. Hardware
details (chip revision, flash size, GPIO list) are reported but not asserted,
because they depend on the board.

## Programming an AT32F405

OpenOCD has no flash driver for the AT32F405 (DBGMCU IDCODE `0x70053242` is not
in its device table). `tools/gen_at32f405_flash_cfg.py` generates a Tcl script
that uses the AT32F402/405 flash controller registers directly. The generated
script erases sectors from high to low, programs the image, calls
`verify_image`, and resumes the target.

Back up the region first:

```sh
openocd -f openocd/p4-swd-probe.cfg -f openocd/at32f405.cfg \
  -c 'init; halt' \
  -c 'dump_image backup.bin 0x08004000 0x1c000' \
  -c 'resume; shutdown'
```

Generate and run the programming script:

```sh
python3 tools/gen_at32f405_flash_cfg.py app.bin \
  --base 0x08004000 -o /tmp/at32f405_flash.cfg
openocd -f openocd/p4-swd-probe.cfg -f openocd/at32f405.cfg \
  -f /tmp/at32f405_flash.cfg
```

The generated script contains the image words. Treat it as private if the
image is private. It also records the image's absolute path for
`verify_image`; regenerate it if the image moves, or pass `--no-verify`.

Measured throughput on the validated setup (32 KB image, 500 kHz SWD):

| Operation | Before optimization | After optimization |
|---|---|---|
| Full 32 KB write | 6 min 04 s | 12.56 s |
| 32 KB read-back verify | 6.4 s | 1.35 s |
| One DAP round trip | ~10 ms | 1.13 ms |

The fast write path keeps the flash programming bit set for a whole sector and
does not poll the busy bit after every word. Back-to-back writes were verified
to be safe on the tested part, but always keep the read-back verification.

## Reset-window attach

The tested application firmware disables SWD after boot. To attach at reset,
override the reset configuration before `init`:

```sh
openocd -f openocd/p4-swd-probe.cfg -f openocd/at32f405.cfg \
  -c 'reset_config srst_only srst_nogate connect_assert_srst' \
  -c 'init; reset halt; reg pc; shutdown'
```

The probe drives nRESET as an open-drain output and leaves all three SWD pins
floating during startup and after about 5 seconds without host traffic.

## Repository layout

```text
main/          ESP32-P4 firmware (P4 port and UART transport)
vendor/        Pinned ARM CMSIS-DAP sources
host/          Python serial <-> TCP bridge, self-test and wire logger
openocd/       OpenOCD interface and example target configurations
tools/         AT32F405 flash-script generator
examples/      Register-level flash test/restore scripts
firmware/      Prebuilt probe binaries and SHA-256 hashes
validation/    Captured logs from the original bring-up
docs/          Validation record and upstream provenance
```

## Limitations

- SWD transfers are bit-banged; the highest reliable clock has not been
  measured with an oscilloscope. 500 kHz is validated on the test setup.
- No VTref detection, target power or protection. Check target voltage first.
- No JTAG, SWO or UART passthrough.
- The bridge is a single-client TCP server. Do not run two host programs
  against it at once.
- If the P4 is reset or the bridge stops, the target may need to be reattached.
- The probe does not recover a target with read-out protection enabled.

## License and provenance

Apache-2.0. See `LICENSE`, `NOTICE` and `docs/UPSTREAM.md`.

The CMSIS-DAP command layer is derived from ARM CMSIS-DAP (Apache-2.0).
OpenOCD is a separate GPLv2 runtime dependency and is not included.
