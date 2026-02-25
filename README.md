# Blekenstein64

N64 homebrew project using [libdragon](https://github.com/DragonMinded/libdragon).

## Quick start

- **Build this project** (produces `hello.z64`):
  ```bash
  DOCKER_DEFAULT_PLATFORM=linux/amd64 libdragon make
  ```

- **Run an example** from the libdragon repo:
  ```bash
  cd libdragon/examples/joypadtest   # or ctest, fontdemo, etc.
  DOCKER_DEFAULT_PLATFORM=linux/amd64 libdragon make
  ```
  Then open the resulting `.z64` ROM in an N64 emulator (e.g. [Project64](https://www.pj64-emu.com/), [ares](https://ares-emu.net/)).

## Apple Silicon (M1/M2/M3) note

The official Docker image is x86 only. Use the platform override so the correct image is used:

```bash
export DOCKER_DEFAULT_PLATFORM=linux/amd64
```

Then run `libdragon make` or `libdragon init` as needed. You can add this to your shell profile if you use libdragon often.

## Project layout

| Path | Description |
|------|-------------|
| `src/` | Your game code (e.g. `main.c`) |
| `libdragon/` | libdragon library (git submodule) |
| `libdragon/examples/` | Example projects (joypadtest, ctest, fontdemo, etc.) |
| `.libdragon/` | libdragon config and container metadata |
| `Makefile` | Build rules; output ROM is `hello.z64` by default |

## Useful commands

- `libdragon make` — build the ROM (from project root or an example directory)
- `libdragon install` — refresh libdragon in the container (e.g. after switching branch)
- [N64 Squid – Setting up Libdragon](https://n64squid.com/homebrew/libdragon/setup/)
