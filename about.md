# xBot

A tiny, high-accuracy bot with only a few tools:

- **Speedhack** (audio pitch follows the speed)
- **TPS** (fixed physics step, default 240, no upper limit)
- **Frame Stepper** (on-screen Step, +10 and Play controls on mobile)
- **Noclip** for P1 and P2 (gear icon in the menu)

It records and replays macros, and loads macros from other bots.

## Recording

Recordings are saved automatically when you leave practice mode, quit or finish a level. In Settings (gear icon) you can turn **Auto-save** off, or turn **Overwrite macro** off to get a new numbered file each time.

## Formats

| Format | Load | Save |
|--------|------|------|
| `.gdr` / `.gdr2` (xdBot, Mega Hack, Eclipse, Echo, and others) | yes | yes |
| `.txt` (`frame down [player2] [button]`, optional first line = TPS) | yes | no |
| `.slc` (Silicate, versions 2 and 3) | yes | no |

Macros live in `geode/game` inside the game directory.

## Controls

- Windows: `Tab` menu, `V` toggle stepper, `C` step
- Android: tap the floating **X** button (drag to move it)
