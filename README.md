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

Move the **config.json** file to **~/.config/Hamzex**

example config:

```
{
  "settings": {
    // "music_kit": 76, comming soon
    // "medal": 6107, comming soon
    "knife_ct_def": 508,
    "knife_t_def": 507,
    "agent_ct_def": 5601,
    "agent_t_def": 5206
  },
  "skins": { /* ak47, m4a4, m4a1, aug, awp, ssg08, sg553, deagle, knife, mag7, mp9, mp7, mp5, usp, p2000, famas, galilar, scar20, g3sg1, glock, p250, five_seven, dual_berettas, tec9, cz75a, revolver, p90, mac10, bizon, ump45, xm1014, nova, m249, zeus, negev, sawedoff */
    "both": {
      "ssg08": { "paint_kit": 253, "seed": 1, "wear": 0.000001 }
    },
    "ct": {
      "m4a1": { "paint_kit": 1017, "seed": 1, "wear": 0.000001 }
    },
    "t": {
      "knife": { "paint_kit": 246, "seed": 1, "wear": 0.000001 }
    }
  }
}
```

## TODO List

- Legacy skin support

- Glove support

- Fix knife inspect animation bug

## License

> Copyright (c) 2026 Enes Hamza

This project is licensed under the GPL 3.0 - see the [LICENSE](https://gitlab.com/eneshamza/hamzex/-/blob/main/LICENSE) file for details.
