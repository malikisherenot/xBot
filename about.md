# xBot

A tiny, high-accuracy bot with only three tools:

- **Speedhack** (audio pitch follows the speed)
- **TPS** (fixed physics step, default 240)
- **Frame Stepper**

It records and replays macros, and loads macros from other bots.

## Formats

| Format | Load | Save |
|--------|------|------|
| `.gdr` / `.gdr2` (xdBot, Mega Hack, Eclipse, Echo, and others) | yes | yes |
| `.txt` (`frame down [player2] [button]`, optional first line = TPS) | yes | no |

Macros live in the mod save folder under `macros/`.

## Controls

- Windows: `Tab` menu, `V` toggle stepper, `C` step
- Android: tap the floating **xBot** button (drag to move it)
