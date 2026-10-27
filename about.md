# xBot

A tiny, high-accuracy bot with only a few tools:

- **Speedhack** (audio pitch follows the speed)
- **TPS** (fixed physics step, default 240)
- **Frame Stepper** (on-screen Step, +10 and Play controls on mobile)
- **Noclip** for P1 and P2 (gear icon in the menu)

It records and replays macros, and loads macros from other bots.

## Formats

| Format | Load | Save |
|--------|------|------|
| `.gdr` / `.gdr2` (xdBot, Mega Hack, Eclipse, Echo, and others) | yes | yes |
| `.txt` (`frame down [player2] [button]`, optional first line = TPS) | yes | no |

Macros live in `geode/game` inside the game directory.

## Controls

- Windows: `Tab` menu, `V` toggle stepper, `C` step
- Android: tap the floating **X** button (drag to move it)
