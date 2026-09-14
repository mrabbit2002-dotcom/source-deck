# Source Deck

OBS Studio dock plugin that turns the current scene's sources into Stream Deck-style visibility buttons.

## v0.1

- `Docks > Source Deck`
- Automatically shows sources from the current scene
- Click a button to toggle source visibility
- Button state follows OBS visibility state
- Refreshes when switching scenes
- Windows x64 automatic build via GitHub Actions

## Download

Open **Actions** in this repository, select the latest **Build Source Deck for Windows** run, and download the `SourceDeck-Setup` artifact. Extract it and run `SourceDeck-Setup.exe`.

The installer copies `source-deck.dll` and locale files into the default OBS Studio installation directory.

Built for OBS Studio 30+ / Qt 6.
