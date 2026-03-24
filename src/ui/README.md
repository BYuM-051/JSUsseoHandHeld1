SquareLine or other UI export files should live under src/ui/generated/.

Recommended layout:
- src/ui/generated/ui.h
- src/ui/generated/ui_events.h
- src/ui/generated/ui_helpers.h
- src/ui/generated/ui.cpp
- src/ui/generated/ui_events.cpp
- src/ui/generated/ui_helpers.cpp
- src/ui/generated/screens/*
- src/ui/generated/components/*

Keep hand-written application code outside src/ui/generated/ so exported files can be replaced safely.
