<div align="center">

# Eliotopy

### Tactical overlay for Eliotrope players in Dofus: line of sight, portal network and redirection, with a full-external approach.

[![license](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
![Dofus](https://img.shields.io/badge/Dofus-3.5.0%2B-brightgreen)
![platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![status](https://img.shields.io/badge/status-Beta%20%7C%20Active%20Development-orange)
[![build](https://github.com/Romain-P/Eliotopy-v2/actions/workflows/build.yml/badge.svg)](https://github.com/Romain-P/Eliotopy-v2/actions/workflows/build.yml)

[![Discord](https://img.shields.io/badge/Discord-Community-5865F2?logo=discord&logoColor=white)](https://discord.com/invite/xYMN5yXNkj)

<img src=".github/assets/preview.gif" width="820" alt="Eliotopy overlay">

</div>

## Overview

Eliotopy is an overlay for Dofus aimed at Eliotrope players: it helps you manage your portal network, compute distances and plan redirections, with a line-of-sight assist on top.

It's a full rewrite of the original [Eliotopy](https://github.com/Romain-P/Eliotopy). The first version relied on screen capture and OCR, which was slow and never fully reliable, so I scrapped that and read the game memory directly instead. The data is now exact and instant: full map layout, line of sight, walkable cells, entities and portals.

## Download

- **Stable**: grab the latest [release](https://github.com/Romain-P/Eliotopy-v2/releases/latest).
- **Nightly**: latest build from `master`, [Eliotopy.exe](https://nightly.link/Romain-P/Eliotopy-v2/workflows/build/master/Eliotopy.zip).

Nightly builds run on every push to `master`. They're not releases: they follow the current work in progress, so expect them to be unstable or sometimes broken. If you want something solid, take a release.

## Features

- **Fully external.** No DLL injection, no hooks inside the game, nothing written to its process. Eliotopy only reads memory and never sends any input.
- **Live game state.** Map grid, movement and line-of-sight flags, entities, portals and player stats, straight from the Unity / IL2CPP runtime.
- **Portal network and redirection.** Follows your real Eliotrope portals from memory, or plan your own by hand with binds using the in-game logic (four max, the oldest drops when you place a fifth). *Preview Exit* (hold) simulates dropping a portal on the cell under your cursor and draws where the redirection comes out, with the damage bonus in percent. *Preview Entrance* (toggle) does the opposite: pick an exit portal and every cell you could enter from to come out there lights up.
- **Portal numbers.** Drawn over the game's own number so you can actually read them, with a configurable shape (round, square, diamond), color, size and offset.
- **LOS assist.** Hold a key and it shows the line of sight from the cell under your cursor, using the same algorithm as the game. Obstacles and cells you can't see get a grey checkerboard.
- **Distance measure.** Hold a key to pin the cell under your cursor, then read the distance to wherever you hover.
- **Direct2D overlay.** Drawn on top of the game and split into small components (grid, dark filter, portals, LOS).
- **Control panel.** A draggable panel that snaps to the screen edges. It shows live state and lets you style every feature (colors, opacity, fill, thickness, ...) and rebind every action, each control with its own tooltip.
- **Multi-language.** English, French, Spanish, German and Portuguese, switchable on the fly.
- **Keybinds.** Bind any combo of modifiers plus a key, a mouse button or the wheel, including hold and toggle actions. Bound inputs are caught and never reach the game.

## Approach: MITM vs memory reading

MITM works, but Dofus changes its protocol obfuscation almost every patch. IL2CPP offsets barely move between updates, so reading memory is just more reliable in the long run.

And to keep accounts safe, everything stays external: no code injected, nothing written to the game process, no automated clicks.

## Structure

```
src/
├─ Application.h          threads, input hooks and lifecycle
├─ DataStore.h            reads the full game state every tick
├─ Keybind.h             low-level keyboard/mouse hooks + keybind registry
├─ memory/               typed external-memory model (CachedEntity, Field, Il2Cpp*)
├─ types/                Dofus structures (cells, entities, services, ...)
├─ datastore/            plain state structs (GameState, MapState, ...)
├─ features/             feature logic (LineOfSight, RedirectionHelper, PortalPlanner)
└─ overlay/
   ├─ Renderer.h         Direct2D device + window orchestrator
   ├─ WebViewPanel.h     control panel
   ├─ assets/            HTML/CSS/JS panel + i18n, packed into the binary
   └─ components/        RenderContext + GridRenderer / LosRenderer / PortalRenderer / ...
```

- Memory access goes through `er6`, an external IL2CPP resolver. It finds `UnityPlayer.dll`, the `GameObjectManager` slot and the MSID pointer table by scanning signatures.
- The model is typed: `CachedEntity` reads a chunk into a local cache, `Field<T, ...offsets>` walks offset chains, and `Il2CppList` / `Il2CppMap` / `Il2CppString` keep cross-process reads to a minimum.
- A background thread refreshes the game state at a rate you can change. The main thread draws the overlay and feeds the panel, and only redraws when something actually changed (around 30 fps).

## Build

You'll need Windows 10/11, Visual Studio 2022 and CMake 3.29+. Everything else (WebView2 SDK, glm, er6) already lives in `libs/`.

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

You get a standalone `build/Release/Eliotopy.exe` (static CRT, nothing to install).

## Usage

1. Start Dofus.
2. Run `Eliotopy.exe`.
3. The overlay attaches to the game window. Open the panel (Settings / Overlay / Debug) to toggle things and set your keybinds.

## Roadmap

- [x] External memory model and generic access logging
- [x] `GameObjectManager` / MSID resolution
- [x] Map data: cells, line of sight, walkable, entities, portals, player stats
- [x] Direct2D overlay (grid, dark filter, entity tracking)
- [x] LOS assist (hold), using the real Dofus algorithm
- [x] Control panel (live state, overlay customization, keybind editor)
- [x] Keybind system (keyboard / mouse / wheel, hold and toggle)
- [x] CI builds and automated releases
- [x] Portal network and redirection (preview exit / entrance, manual placement, damage bonus)
- [x] Distance measure
- [x] Multi-language UI (EN / FR / ES / DE / PT)
- [ ] Factor entities into line-of-sight calculations
- [ ] AI turn assist to land a hit on a target through portals
- [ ] Lua engine with a rich API to write scripts and build AIs
- [ ] Scripts to assist on mechanically hard bosses
- [ ] Automatic in-app updater
- [ ] Randomized process name so Eliotopy doesn't stand out in the Windows process list

## License

MIT, see [`LICENSE`](LICENSE). It's a read-only external tool meant for personal and educational use. Use at your own risk.
