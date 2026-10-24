# Upstream provenance

## ARM CMSIS-DAP

- Project: <https://github.com/ARM-software/CMSIS-DAP>
- Commit: `12636590eec66fae2d1bba4518749426ad5a4595`
- License: Apache-2.0 (`vendor/LICENSE`)

`vendor/DAP.c` and `vendor/SW_DP.c` are vendored unchanged. `vendor/DAP.h` is
modified for this port:

- ESP32-P4 compiler definitions replace the ARM-specific ones.
- The ARM assembly delay loop is replaced with a RISC-V delay loop.
- Port-specific comments mark the modified sections.

The P4 transport (`main/main.c`, `main/DAP_config.h`) wraps the upstream
command handler and implements only the SWD transport.

## OpenOCD CMSIS-DAP TCP framing

- Reference: <https://github.com/openocd-org/openocd/blob/master/src/jtag/drivers/cmsis_dap_tcp.c>

`host/probe.py` is an independent implementation of the published TCP frame
layout. No OpenOCD source code is copied or bundled. OpenOCD is GPLv2 and is
used as a separate program at runtime.

## Board wiring

The pin assignment follows the ESP32-P4-WIFI6 reference design:

- <https://files.waveshare.com/wiki/ESP32-P4-WIFI6/ESP32-P4-WIFI6-datasheet.pdf>

The tested board is a v1.3 ESP32-P4 with 32 MB flash. Header positions vary
between boards, so verify the physical pinout before connecting a target.

## Host dependency

`host/probe.py` requires pyserial (BSD-3-Clause), which is not bundled.
