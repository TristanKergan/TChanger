# TChanger

[![C++26](https://img.shields.io/badge/C%2B%2B-26%20Modules-blue.svg)](https://en.cppreference.com/w/cpp/26)
[![Qt](https://img.shields.io/badge/Qt-6.x%20Widgets-41cd52.svg)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20x86__64-orange.svg)]()
[![Build & Tests](https://img.shields.io/badge/Tests-11%2F11%20Passing-brightgreen.svg)]()
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

High-performance, internal skin, knife, and agent changer for **Counter-Strike 2** on Linux with a standalone native **Qt6 desktop configurator** and zero-latency **live IPC hot-reloading**.

> ⚠️ **Disclaimer**: This software is an internal modification tool. Use at your own risk. The authors accept no responsibility for any in-game sanctions or account restrictions.

---

## 🌟 Key Features

* **🖥️ Standalone Qt6 Desktop Configurator (`hamzex-configurator`)**:
  * Native Qt6 Widgets interface with dark theme and responsive layout.
  * Independent desktop app: configure skins **before** launching the game or live **during** gameplay.
  * Search filter and category selector (Rifles, Pistols, Snipers, SMGs, Heavy).
  * Knife customizer with independent CT / T models, one-click copy CT $\leftrightarrow$ T, and full Doppler finish presets (Ruby, Sapphire, Black Pearl, Phases).
  * Agent selector covering all 46 official CT and T models.
  * Dirty state indicator (`* [Unsaved Changes]`) and exit confirmation prompt.
  * Bulk weapon reset button (`[ 🗑 Reset All Weapons ]`) with safety prompt.
  * Real-time sync status badge (`● CS2 Runtime: Connected (vX) [hh:mm:ss]`).

* **⚡ Zero-Wait Lock-Free Hot-Reloading (`RuntimeConfig` & IPC)**:
  * Unix Domain Socket IPC (`AF_UNIX`) communication between GUI and game runtime.
  * RCU lock-free atomic snapshot architecture (`std::atomic<std::shared_ptr<const RuntimeConfigSnapshot>>`).
  * **Zero mutex locks, zero memory allocations, zero filesystem I/O, zero socket calls** on the rendering thread hot path.
  * Instant loadout switching without restarting rounds or the game.

* **🛡️ Hardened Memory Safety & Frame-Rate Protection**:
  * Unconfigured weapons cached on first frame, eliminating redundant hash lookups at 60–240+ FPS.
  * Atomic configuration backup (`~/.config/hamzex/config.json.bak`) with automatic failure rollback.
  * Length-safe IPC transport (`WriteAll`), symlink hijacking guards, and 64 KB packet capping.
  * Log rotation at 2 MB (`hamzex.log` $\rightarrow$ `hamzex.log.1`) with configurable `HAMZEX_DEBUG=1`.

---

## 📋 Requirements

* **OS**: Linux x86_64
* **Compiler**: GCC >= 14 or Clang >= 18 (with C++26 modules support)
* **Build System**: CMake >= 3.28 and Ninja
* **Libraries**: Qt6 (Widgets, Core, Gui)
  * **Debian / Ubuntu**: `sudo apt install cmake ninja-build g++ qt6-base-dev qt6-base-dev-tools`
  * **Arch Linux / Manjaro**: `sudo pacman -S cmake ninja gcc qt6-base`
  * **Fedora**: `sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel`
* **Optional**: Linux kernel headers matching your running kernel (only if building the `hide_tracer` LKM)
* **Optional**: `gdb` (used by `inject.sh` to attach and load `libHamzex.so`)

---

## 🚀 Quick Start Guide

### 1. Clone the Repository

```bash
git clone https://github.com/TristanKergan/TChanger.git
cd TChanger
```

### 2. Build Cheat & Configurator

```bash
./build.sh
```

*(Or build manually with CMake)*:
```bash
cmake -B build -G Ninja
cmake --build build
```

Artifacts produced in `build/`:
* `libHamzex.so` — CS2 internal injection library.
* `hamzex-configurator` — Standalone Qt6 GUI configurator.
* `libHamzexCommon.a` — Shared core library (ItemCatalog, JSON, Config).

### 3. Usage Workflows

#### Mode A: Pre-Game Configuration (Offline Mode)
1. Run the configurator before starting CS2:
   ```bash
   ./build/hamzex-configurator
   ```
2. Customize your weapons, knives, agents, and skins.
3. Click **«💾 Save to Disk»** (or press `Ctrl+S`).
4. Settings are saved to `~/.config/hamzex/config.json`. CS2 will automatically load them on startup.

#### Mode B: Live In-Game Hot-Reloading
1. Start Counter-Strike 2.
2. Inject the library:
   ```bash
   sudo ./inject.sh
   # Or without LKM bypass:
   sudo ./inject-direct.sh
   ```
3. Open `./build/hamzex-configurator` (status will show `● CS2 Runtime: Connected`).
4. Change any skins, wear, seed, or knife model.
5. Click **«⚡ Apply to Game»** (or `Ctrl+Enter`) for instant in-game updates.

#### Mode C: CLI Mode
```bash
# Test connection to running CS2 runtime
./build/hamzex-configurator --ping

# Apply JSON config directly to running game
./build/hamzex-configurator --apply my_config.json
```

---

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

### 🧪 Comprehensive Offline Test Suite

TChanger includes an automated standalone test suite verifying parsing, resolution, aliasing, concurrency, and memory safety:

```bash
./tests/run_tests.sh
```

All 11 test blocks run offline without requiring Counter-Strike 2:
1. Low-level safety, `ResolveRipRel`, and `IsBadReadPtr`.
2. Weapon alias canonicalization.
3. `ItemCatalog` validation against `items_game.txt`.
4. Wear preset bounds and string conversion.
5. Paint kit collision detection and knife guard.
6. JSON parser with single-line & multi-line comments.
7. Modern V2 config parsing and resolution.
8. Legacy V1 backward compatibility.
9. Knife & weapon skin catalogs.
10. `RuntimeConfig` concurrent multi-threaded RCU reads (80k+ iterations).
11. Unix Domain Socket IPC handshake, validation, and hot-reload.

---

## 📄 License

This project is licensed under the **GNU General Public License v3.0** — see the [LICENSE](LICENSE) file for details.

Repository: [https://github.com/TristanKergan/TChanger](https://github.com/TristanKergan/TChanger)


