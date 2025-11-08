# IdleOptimizer + GUI (14-day default)

**Originally by Syokora, AdlC, rexfactor**  
**Adapted by Praedyth & ChatGPT**

- GUI in `assets/optimizer_gui.html` (v7) defaults to a **14-day event**.
- Start resources default: all **0** except **Bat=500000**, **FREE_EXP=1**, **GROWTH=1**.
- All **levels** and **speed levels** default to **0**.
- Second tab lets you **rename resources** per event; names are saved to `resourceNames` in JSON.
- C++ reads `resourceNames` from `config.json` and uses them for labels/logs.

## Build (Windows / VS 2022)

1. Install Visual Studio 2022 with "Desktop development with C++".  
2. Open this folder as a CMake project and build target `IdleOptimizer`.

> Bundled with a minimal single-header JSON parser under `../third_party/` so
> the project builds fully offline.

## Run

1. Open `assets/optimizer_gui.html` in your browser.
2. Fill in your starting resources and toggles on the **Optimizer Settings** tab.
   - "Write log file" enables disk logging and will create the target folder if it does not exist.
   - "Add to existing log file (append)" keeps prior runs in the same file when disk logging is enabled.
   - "Pause on exit" keeps the console window open after the run finishes.
   - The live preview on the right summarises which logging destinations (console / file) are active.
3. Click **Save JSON** to export `config.json` (or use the bundled `config.example.json` as a starting point).
4. Place `config.json` next to the EXE (or run from this folder).
5. Double-click `RunWithLog.bat` to capture output in `run_log.txt`, or launch the built executable directly.
6. Logs written to disk land in the folder configured by the GUI (defaults to `logs/IdleOptimizer.log`).

## Notes

- Event duration in C++ is compiled as **14 days** for now (matches the GUI).
- `resourceNames` are read at runtime from `config.json`.
- `busyTimesStart` / `busyTimesEnd` values in `config.json` are expressed as **hours from when you launch the optimizer**, not clock-of-day. For example, if you start a run at 08:00 and want a nightly pause from 19:00–03:00, enter start/end hours `11` and `19` (11 and 19 hours after launch) or use the GUI schedule generator, which outputs the correctly offset values.
- To disable log file output, uncheck **Write log file** in the GUI before exporting your configuration.
