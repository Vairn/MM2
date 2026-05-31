# Open Questions and Next Traces

## High-Value Targets

1. Combat pipeline around `LAB_53C8` and `LAB_28F6`.
2. Precise semantics of the loader wrappers near `0x254CC`/`0x25528` (open/read/decode mapping).
3. Concrete layout of the `A4` thunk table created near startup.
4. Overlay segment routines (`0x28208`, `0x28E3A`, `0x294xx` tail paths).

## Known Embedded Filenames

- `map.dat`
- `roster.dat`
- `str.dat`
- `event.dat`
- `disk.32`
- `dos.library`
- `icon.library`

## Suggested Method

- Track stack arguments for wrappers around `LAB_254CC` / `LAB_254F4` (open/read family).
- Label thunk offsets in a shared header as symbolic names.
- Lift one function family at a time (map loop, then loader, then combat).

