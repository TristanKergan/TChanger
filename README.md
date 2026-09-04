# Hamzex

Linux only internal skin changer for **Counter-Strike 2**.

> ⚠️ **Warning**: This software technically falls into the "cheat" category
> and could result in a ban from the game; I take no responsibility for it.

## Requirements

- **CMake** >= 4.4.2
- **Ninja**
- A C++26 compiler with module support (GCC >= 14)
- **Linux kernel headers** matching your running kernel (to build the `hide_tracer` kernel module)
- **gdb** (used by `inject.sh` to attach to the game and `dlopen` the library)

## Compiling

**build.sh** for main lib file

**build_lkm.sh** for kernel module

### Injecting into game process

run **inject.sh**, This script masks **TracerPid** by loading the lkm module.

### How do I change skins?

Move the **config.json** file to **$HOME/.config/Hamzex**

## TODO List

- Legacy skin support

- Glove support

- Fix knife inspect animation bug

## License

> Copyright (c) 2026 Enes Hamza

This project is licensed under the GPL 3.0 - see the [LICENSE](https://gitlab.com/eneshamza/hamzex/-/blob/main/LICENSE) file for details.
