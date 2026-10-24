# Validation logs

Captured during the 2026-10-15 bring-up:

- `2026-10-15/openocd.log`: OpenOCD interface initialization.
- `2026-10-15/openocd-reconnect.json`: 10 consecutive OpenOCD sessions.
- `2026-10-15/idle-test.json`: SWJ query after 7 seconds idle.

These logs predate the packet-size fix (64 to 1024 bytes). See
`../docs/VALIDATION.md` for the full results and caveats.
