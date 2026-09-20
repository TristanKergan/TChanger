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

## Step-by-step guide
### building

```git clone (https://github.com/TristanKergan/TChanger)``` clone repo

```cd hamzex``` enter repo

```./build_lkm.sh``` build kernel module for bypass TracerPid (optional)

```./build.sh``` build cheat

```sudo ./inject.sh``` inject cheat to cs2 (With TrackerPid bypass)

or

```sudo ./inject-direct.sh``` inject without bypass (I didn't experience any problems.)

### Configuration & Skin Customization

Configuration file path: `~/.config/hamzex/config.json` (or `~/.config/Hamzex/config.json`).

Hamzex features a modern **V2 Configuration System** supporting human-readable strings, wear presets, weapon aliases, and team splits, while remaining **100% backward compatible** with legacy V1 numeric configs.

#### V2 Configuration Example (`config.json`)

```json
{
  "config_version": 2,
  "knives": {
    "ct": {
      "model": "karambit",
      "skin": "doppler_ruby",
      "wear": "factory_new",
      "seed": 1
    },
    "t": {
      "model": "butterfly",
      "skin": "fade",
      "wear": "factory_new",
      "seed": 1
    }
  },
  "agents": {
    "ct": "sas",
    "t": "phoenix"
  },
  "weapons": {
    "ak47": {
      "skin": "head_shot",
      "wear": "factory_new",
      "seed": 1
    },
    "m4a1-s": {
      "skin": "printstream",
      "wear": "factory_new",
      "seed": 1
    },
    "usp-s": {
      "skin": "printstream",
      "wear": "factory_new",
      "seed": 1
    },
    "glock": {
      "skin": "vogue",
      "wear": "factory_new",
      "seed": 1
    },
    "awp": {
      "skin": "chrome_cannon",
      "wear": "factory_new",
      "seed": 1
    },
    "deagle": {
      "skin": "printstream",
      "wear": "factory_new",
      "seed": 1
    },
    "ssg08": {
      "skin": "dragonfire",
      "wear": "factory_new",
      "seed": 1
    }
  }
}
```

---

### Data Sources of Truth (Where IDs Come From)

In Counter-Strike 2, visual customization is split across distinct numeric ID spaces defined in Valve's `scripts/items/items_game.txt` (inside `game/csgo/pak01_dir.vpk`):

1. **Weapon / Knife Item Definition Index (`m_iItemDefinitionIndex`)**:
   - The unique item identifier for weapons and knives in Valve's schema.
   - *Examples*: AK-47 is `7`, M4A1-S is `60`, USP-S is `61`, Karambit is `507`, Butterfly Knife is `515`, Kukri Knife is `526`.
2. **Paint Kit / Finish Catalog ID (`m_nFallbackPaintKit` / `CEconItemAttribute` 6)**:
   - The texture pattern / finish catalog applied *onto* a weapon or knife mesh.
   - *Examples*: Fade is `38`, Doppler Ruby is `415`, Case Hardened is `44`, Head Shot is `1171`.
   - Accessible from `items_game.txt` under `paint_kits` or online at databases like [csgoskins.gg](https://csgoskins.gg) (labeled *Finish Catalog* at bottom left).
   - ⚠️ **Critical Distinction**: A knife definition index (e.g. `515` for Butterfly) is **NOT** a paint kit. Setting `"paint_kit": 515` applies `am_bamboo_print` (a green bamboo texture), causing broken or missing textures on knives.
3. **Agent Definition Index (`m_iItemDefinitionIndex`)**:
   - Player character model identifiers under `items` in `items_game.txt`.
   - *Examples*: SAS Officer is `5601` (CT), Phoenix Enforcer is `5206` (T).

---

### Knives Reference (20 CS2 Knives)

You can specify knives by name or defIndex:

| Knife Name | DefIndex | Model Subclass | Aliases |
| :--- | :--- | :--- | :--- |
| `bayonet` | 500 | 3933374535 | `bayonet` |
| `classic` | 503 | 3787235507 | `css`, `classic` |
| `flip` | 505 | 4046390180 | `flip` |
| `gut` | 506 | 2047704618 | `gut` |
| `karambit` | 507 | 1731408398 | `karambit` |
| `m9` | 508 | 1638561588 | `m9_bayonet`, `m9-bayonet`, `m9bayonet` |
| `huntsman` | 509 | 2282479884 | `tactical`, `huntsman` |
| `falchion` | 512 | 3412259219 | `falchion` |
| `bowie` | 514 | 2511498851 | `bowie` |
| `butterfly` | 515 | 1353709123 | `butterfly` |
| `shadow_daggers` | 516 | 4269888884 | `push`, `daggers`, `shadow_daggers` |
| `paracord` | 517 | 1105782941 | `cord`, `paracord` |
| `survival` | 518 | 275962944 | `canis`, `survival` |
| `ursus` | 519 | 1338637359 | `ursus` |
| `navaja` | 520 | 3230445913 | `navaja` |
| `nomad` | 521 | 3206681373 | `outdoor`, `nomad` |
| `stiletto` | 522 | 2595277776 | `stiletto` |
| `talon` | 523 | 4029975521 | `talon` |
| `skeleton` | 525 | 365028728 | `skeleton` |
| `kukri` | 526 | 3845286452 | `kukri` |

---

### Popular Paint Kits & Finish Names

You can specify either the human-friendly name or the raw integer ID:

| Skin Name | Finish ID | Supported Weapons / Knives |
| :--- | :--- | :--- |
| `vanilla` | 0 | All knives / weapons |
| `fade` | 38 | Knives, Glock-18, AWP, etc. |
| `case_hardened` | 44 | Knives, AK-47, Five-SeveN |
| `slaughter` | 59 | Knives |
| `crimson_web` | 12 | Knives, Desert Eagle |
| `tiger_tooth` | 409 | Knives |
| `marble_fade` | 413 | Knives |
| `doppler_ruby` / `ruby` | 415 | Knives |
| `doppler_sapphire` / `sapphire` | 416 | Knives |
| `doppler_black_pearl` | 417 | Knives |
| `doppler_phase1` / `phase1` | 418 | Knives |
| `doppler_phase2` / `phase2` | 419 | Knives |
| `doppler_phase3` / `phase3` | 420 | Knives |
| `doppler_phase4` / `phase4` | 421 | Knives |
| `lore` | 561 | Knives |
| `autotronic` | 569 | Knives |
| `gamma_emerald` / `emerald` | 568 | Knives |
| `head_shot` | 1171 | AK-47 |
| `printstream` | Auto (962/984/1142) | Deagle (962), M4A1-S (984), USP-S (1142) |
| `asiimov` | Auto (255/279/801) | M4A4 (255), AWP (279), AK-47 (801) |
| `dragon_lore` / `dlore` | 344 | AWP |
| `chrome_cannon` | 1206 | AWP |
| `duality` | 1222 | AWP |
| `dragonfire` | 624 | SSG 08 |
| `vogue` | 963 | Glock-18 |
| `redline` | 282 | AK-47, AWP |
| `vulcan` | 302 | AK-47 |
| `hyper_beast` | Auto (430/475/574) | M4A1-S (430), AWP (475), Five-SeveN (574) |
| `howl` | 309 | M4A4 |

*Note: For any unlisted skin, you can directly supply its numeric finish catalog ID (e.g. `"skin": 1175` or `"paint_kit": 1175`).*

---

### Wear Presets

In place of numeric wear floats (e.g. `0.0001`), you can use standard wear presets:
- `"fn"` or `"factory_new"` $\rightarrow$ `0.001`
- `"mw"` or `"minimal_wear"` $\rightarrow$ `0.080`
- `"ft"` or `"field_tested"` $\rightarrow$ `0.200`
- `"ww"` or `"well_worn"` $\rightarrow$ `0.400`
- `"bs"` or `"battle_scarred"` $\rightarrow$ `0.600`

---

### Supported Weapon Aliases

Hamzex automatically canonicalizes weapon names regardless of hyphens, casing, or common aliases:
- `"ak-47"`, `"AK_47"`, `"ak"` $\rightarrow$ `ak47`
- `"m4a1-s"`, `"m4a1_s"`, `"m4a1_silencer"` $\rightarrow$ `m4a1`
- `"usp-s"`, `"usp_s"`, `"usp_silencer"` $\rightarrow$ `usp`
- `"sg553"`, `"sg-553"`, `"sg556"` $\rightarrow$ `sg556`
- `"mp5"`, `"mp5-sd"`, `"mp5sd"` $\rightarrow$ `mp5sd`
- `"scout"`, `"ssg-08"`, `"ssg08"` $\rightarrow$ `ssg08`
- `"desert_eagle"`, `"desert-eagle"`, `"deagle"` $\rightarrow$ `deagle`
- Any knife model name (`"butterfly"`, `"karambit"`, `"m9"`, etc.) can also be used directly as a weapon key in `"weapons"`.

---

### Backward Compatibility (Legacy V1 Format)

If you have an existing V1 `config.json` with `"settings"` and `"skins"` (`"both"`, `"ct"`, `"t"`), Hamzex reads it without any modifications required. Furthermore:
- If a legacy config accidentally set `"knife": { "paint_kit": 515 }`, Hamzex automatically catches the knife defIndex collision and safely falls back to Fade (`38`) to prevent broken black/untextured knives.

---

### Offline Test Suite

Hamzex includes an automated standalone test suite verifying parsing, resolution, aliasing, and memory safety:

```bash
./tests/run_tests.sh
```

---

## License

> Copyright (c) 2026 Enes Hamza

This project is licensed under the GPL 3.0 - see the [LICENSE](https://gitlab.com/eneshamza/hamzex/-/blob/main/LICENSE) file for details.

