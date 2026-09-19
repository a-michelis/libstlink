# Chip descriptions

One file per chip, named after the device it describes. Each says what a chip is and where
its memories are, so that adding support for a chip whose flash already behaves like one of
the implemented families is a matter of adding a file.

These files come from the C project and keep its values; the layout differs.

## Format

Plain text, one field per line: a name, whitespace, a value.

```
name             STM32F1xx_MD
chip_id          0x410
flash.page_size  0x400       # 1 KB
```

- `#` starts a comment, either on its own line or after a value.
- Blank lines are ignored and group related fields.
- A dot in a name groups fields; it carries no meaning beyond reading order.
- Numbers are hexadecimal with a `0x` prefix, or decimal without one.
- Order does not matter. Unknown names are an error rather than ignored, so a typo is
  reported instead of silently leaving a field at zero.

There is no JSON, TOML or YAML here on purpose: the data is flat, the parser is short, and
the library has no third party dependencies.

## Fields

### Identity

| field | required | meaning |
| --- | --- | --- |
| `name` | yes | The device, as ST names it. `STM32L45x_L46x` covers a group that is identical for our purposes. |
| `reference` | yes | The reference manual documenting it, for example `RM0394`. |
| `chip_id` | yes | What the chip reports from its identity register. The key: no two files may share one. |
| `family` | yes | Which flash mechanism it uses. See below. |

### Memories

Each region is a `base` and a `size`. A `size` of `0x0` means the chip does not have that
region, and its `base` is then ignored.

| field | required | meaning |
| --- | --- | --- |
| `flash.base` | yes | Where main flash starts. `0x08000000` on every chip so far, stated rather than assumed. |
| `flash.size_reg` | yes | Address of the register holding the flash size. The size is read from the chip, never taken from this file, because one part number ships in several sizes. |
| `flash.page_size` | yes | Erase granularity. Families with irregular sectors, such as F2 and F4, compute it instead and this is the smallest. |
| `sram.base` | yes | Where SRAM starts. `0x20000000` on every chip so far. |
| `sram.size` | yes | How much there is. |
| `bootrom.base` | yes | The system bootloader. |
| `bootrom.size` | yes | |
| `option.base` | yes | Option bytes, which configure the chip itself. `0x0` when they are not reachable this way. |
| `option.size` | yes | |
| `otp.base` | no | One time programmable area. Omit both when the chip has none. |
| `otp.size` | no | |

### Flags

`flags` takes zero or more names separated by whitespace. Omit the line when there are none.

| flag | meaning |
| --- | --- |
| `swo` | Has a trace output pin, so `st-trace` can work. |
| `dualbank` | Flash is in two banks that erase and program independently. |

## Families

`family` names the flash mechanism, not the chip series: several series share one, and the
value decides which implementation drives the flash controller.

| family | used by |
| --- | --- |
| `C0` | C0 series |
| `C5` | C5 series |
| `F0_F1_F3` | F0, F1 low, medium and high density, F3 |
| `F1_XL` | F1 extra large density, which has a second bank |
| `F2_F4` | F2, F4 |
| `F7` | F7 |
| `G0` | G0 |
| `G4` | G4 |
| `H5` | H5 |
| `H7` | H7, dual bank with two register sets |
| `L0_L1` | L0, L1, which use a different controller entirely |
| `L4` | L4 |
| `L5_U5` | L5, U5 |
| `WB_WL` | WB, WL |
| `WB0` | WB0 |

Adding a chip to a family that is already implemented needs only a file. A chip whose flash
does not behave like any of these needs a new family and an implementation to go with it.

## Adding a chip

1. Copy the closest existing file and rename it after the new device.
2. Set `chip_id` from the chip's identity register, and check no other file uses it.
3. Fill in the memories from the reference manual.
4. Choose the `family` whose flash controller the manual describes. If none matches, the
   chip needs code as well as a file.
