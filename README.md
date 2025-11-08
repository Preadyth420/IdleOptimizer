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

> Uses `nlohmann/json` via CMake `FetchContent`. If your network blocks it,
> vendor the single-header `json.hpp` and include it instead.

## Run

1. Open `assets/optimizer_gui.html` in your browser.  
2. Click **Save JSON** to export `config.json`.  
3. Put `config.json` next to the EXE (or run from this folder).  
4. Optional: `RunWithLog.bat` captures output to `run_log.txt`.

## Notes

- Event duration in C++ is compiled as **14 days** for now (matches the GUI).  
- `resourceNames` are read at runtime from `config.json`.