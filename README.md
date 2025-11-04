# IdleOptimizer



==ITRTG generator Event script with GUI==



==Originally by  Syokora \& AdlC \& rexfactor==



==Adapted by Praedyth \& ChatGPT==



We built a tiny browser GUI to make the C++ idle-event optimizer easy to use.


It lets you edit event duration, starting levels/resources, upgrade path, and a
weekly “busy time” schedule (e.g., 19:00–03:00). 



Overnight ranges are auto-collapsed
(19:00–03:00 → hours 19→27), 



and the GUI generates the busyTimesStart/End arrays for you.



==What’s labeled==



Upgrades are named: 0–9 = Level, 10–19 = Speed, 20 = Complete.,


FREE\_EXP (idx 7) and GROWTH (idx 8) are computed from DLs and UNLOCKED\_PETS (no manual input).,



==How to use==



Open assets/optimizer\_gui.html in your browser.,


Fill in scalars, levels, and resources (no slots for FREE\_EXP/GROWTH).,


(Optional) Set your daily or per-day busy times → click Generate Busy Arrays.,


Check the labeled upgrade preview, then Save JSON to export config.json.,


Put config.json next to IdleOptimizer.exe and run it. Logs mirror to run\_log.txt.


Want longer search? In main.cpp, bump optimizeUpgradePath(..., 10000) higher.,



==Roadmap==



After this event we’ll strip things to a base template and add a section to rename resources directly in the GUI.

