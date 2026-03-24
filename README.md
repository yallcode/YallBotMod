# YallBot. 🤖

A **macro recorder & bot** mod for Geometry Dash, built with [Geode SDK](https://geode-sdk.org).

Record your inputs, replay them perfectly. Simple.

## Building

This mod is built automatically via **GitHub Actions** on every push.
Download the latest `.geode` file from the [Actions tab](../../actions).

To install: drop the `.geode` file into your `Geometry Dash/geode/mods/` folder.

## File Structure

```
YallBot/
├── src/
│   ├── main.cpp          # Hooks (GJBaseGameLayer, PlayLayer, PauseLayer)
│   └── MacroManager.cpp  # Recording/playback logic
├── .github/workflows/
│   └── build.yml         # GitHub Actions CI
├── mod.json              # Geode mod metadata
├── CMakeLists.txt        # Build config
├── about.md              # Geode store description
└── changelog.md
```

## How It Works

- **Recording**: Hooks `pushButton`/`releaseButton` to capture every input with its frame number
- **Playback**: On each physics step (`processCommands`), replays inputs matching the current frame
- **Storage**: Macros saved as binary `.ybot` files in the Geode save directory

## Developer

**yallcode** — [GitHub](https://github.com/yallcode)
