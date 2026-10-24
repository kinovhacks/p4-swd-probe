# Validation record

Validation was performed on 2026-10-15 with an ESP32-P4 v1.3 board (32 MB
flash) and an Artery AT32F405RCT7 target. OpenOCD was
`0.12.0+dev-02750-g751e5af67` using its CMSIS-DAP TCP backend, with
`host/probe.py` in between.

The target application runs from `0x08004000`. The app region is
`0x08004000..0x0801FFFF`.

## Fixes required for OpenOCD compatibility

The initial port could connect, halt, step and read single words, but bulk
transfers failed with `0xff` responses. Three issues were fixed:

1. `main/DAP_config.h`: `DAP_PACKET_SIZE` raised from 64 to 1024. OpenOCD's
   CMSIS-DAP TCP backend forwards bulk requests against a 1024-byte budget and
   sends `DAP_Transfer` requests for 21-255 transfers, whose responses can
   reach 1023 bytes.
2. `main/main.c`: the response header's length high byte was hard-coded to
   zero. Responses longer than 255 bytes were truncated. The packet validator
   was widened to accept Transfer counts up to 255, response lengths up to
   1024, and TransferBlock reads up to 255 / writes up to 254. Request and
   UART driver buffers were increased accordingly.
3. `host/probe.py`: the bridge's two length limits were raised from 64 to
   1024.

## Probe and target checks

| Check | Result |
|---|---|
| OpenOCD init / examine | DPIDR `0x2ba01477`, Cortex-M4 r0p1 |
| halt / read PC / step / resume | PC advanced by 2 after a Thumb step; `S_HALT=0` after resume |
| Target memory read | CPUID, flash size, series and vector table matched known values |
| App-region read-back | 32 KB matched the source image byte for byte |
| Sector erase | `0x0800C000` read back as all `0xFF`, no programming or ECC error |
| Sector write | 512/512 words written and read back correctly |
| Restore | Test sector erased back to all `0xFF`; target resumed |
| Reconnect | 10/10 OpenOCD sessions completed |
| Idle keep-alive | SWJ pin query still answered after 7 s idle |
| Malformed packets | 6 malformed length/count variants rejected |
| Serial stress | 1000 identification queries, no errors |

## Full-image write

An earlier register-level script wrote a 32 KB application image (16 sectors,
8192 words) in 6 min 04 s. Two transient single-word failures occurred
(about 0.02%) and were recovered by the retry logic. The read-back matched the
image, and the target booted normally after a system reset.

After optimization, the same write took 12.56 s with zero errors:

- Removed a per-packet `vTaskDelay(1)` in the firmware.
- Raised the host link from 115200 to 921600 baud.
- Raised SWD from 100 kHz to 500 kHz.
- Kept the flash programming bit set for a whole sector instead of polling
  after every word. A 64-word back-to-back test passed before adopting this.

| Metric | Before | After |
|---|---|---|
| 32 KB write | 6 min 04 s | 12.56 s |
| 32 KB read-back verify | 6.4 s | 1.35 s |
| DAP round trip | ~10 ms | 1.13 ms |
| Dump throughput | 4.8 KiB/s | 23.7 KiB/s |

## Reproduction

The AT32F405 flash scripts used for these tests are in
`examples/at32f405/` (pattern write and restore). Full-image programming uses
`tools/gen_at32f405_flash_cfg.py`, which embeds the image at generation time.

## Caveats

- SWD waveforms and duty cycle were not measured with an oscilloscope. 500 kHz
  is the highest clock validated on this setup.
- The AT32 flash controller register sequence is based on the AT32F402/405
  reference manual and the vendor SDK. It is not exercised by OpenOCD's
  built-in flash driver.
- Transient single-word write failures were observed at a low rate. Always
  keep the read-back verification.
- The logs in `validation/2026-10-15/` are early bring-up captures taken before
  the packet-size fix (64 to 1024 bytes). They are kept for the OpenOCD and
  reconnect evidence, not as a report from the final firmware.
- The prebuilt binaries in `firmware/` are the validated 2026-10-15 build. A
  fresh ESP-IDF v5.5.4 build from this source tree produces a similar app image;
  build metadata and embedded diagnostic strings differ, so compare behavior
  before replacing a known-good binary.
