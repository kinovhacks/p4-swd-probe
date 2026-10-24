# AT32F405 flash examples

These scripts exercise the AT32F402/405 flash controller directly because
OpenOCD has no flash driver for this part.

- `flash_test.cfg` erases scratch sector `0x0800C000`, programs a 512-word
  pattern and checks it. It only writes to that one sector.
- `flash_restore.cfg` erases `0x0800C000` back to all `0xFF`.

Before running `flash_test.cfg`, confirm that `0x0800C000` is not used by your
application. The scripts are examples for the AT32F405RCT7 and should be
reviewed before use on another part or memory layout.

For full-image programming, use `tools/gen_at32f405_flash_cfg.py`; it generates
the same erase/program sequence for an arbitrary binary and adds a
`verify_image` step.
