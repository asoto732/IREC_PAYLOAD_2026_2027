#pragma once

// Serial monitor commands (E32F-25, E32F-26, E32F-27):
//   l  list files with sizes
//   d  dump the most recent log file
//   e  erase all log files (asks to confirm with Y)
//   x  stop recording
//
// Call once per loop(). Sets stopRequested when `x` is typed.
void handleSerialCommands(bool recording, bool& stopRequested);
