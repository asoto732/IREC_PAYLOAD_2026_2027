# ESP32 + BMP390 Logger

Reads pressure and temperature from a BMP390 and saves them to `/log.txt`
in the ESP32's flash. A one day project.

- **Start here:** `PROJECT_SPEC.md` (guides, schematic task, day plan)
- **Requirements:** `REQUIREMENTS.md`
- **Your code:** `src/main.cpp` (fill in the TODOs)

Wiring: VIN to 3V3, GND to GND, SDA to GPIO 13, SCL to GPIO 14.

Build, upload and monitor with the check mark, arrow and plug icons on
VS Code's bottom bar. After the 60 s recording, type `d` in the monitor to
print the file.
