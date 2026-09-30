#include "Commands.hpp"

#include <Arduino.h>
#include <LittleFS.h>
#include "Config.hpp"

namespace {

bool pendingErase = false;

// File::name() gives "log_000.txt" on current ESP32 cores. Put the leading
// slash back so the result can be passed to LittleFS.open(). PROVIDED.
String fullPath(File& f) {
    String name = f.name();
    return name.startsWith("/") ? name : "/" + name;
}

bool isLogFile(const String& path) {
    return path.startsWith(cfg::kLogPrefix) && path.endsWith(cfg::kLogSuffix);
}

// PROVIDED: worked example of walking the filesystem (E32F-26).
void listFiles() {
    File root = LittleFS.open("/");
    Serial.println("Files:");
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        Serial.printf("  %-16s %8lu bytes\n", fullPath(f).c_str(),
                      static_cast<unsigned long>(f.size()));
    }
    Serial.printf("Used %lu of %lu bytes\n",
                  static_cast<unsigned long>(LittleFS.usedBytes()),
                  static_cast<unsigned long>(LittleFS.totalBytes()));
}

// Return the path of the highest numbered log file, or "" if there is none.
String latestLogPath() {
    // TODO(E32F-26): Walk the files like listFiles() does. Keep the largest
    // path for which isLogFile() is true. Because the numbers are zero
    // padded (log_007 vs log_012), a plain string comparison (a > b) picks
    // the newest one.
    return "";
}

void dumpLatest() {
    // TODO(E32F-26, E32F-27):
    //   1. path = latestLogPath(); if empty, print "No log files" and return.
    //   2. Open it for reading (FILE_READ).
    //   3. Print "BEGIN FILE <path>", then copy every byte to Serial
    //      (while (f.available()) Serial.write(f.read());), then "END FILE".
    //   4. Close the file.
}

void eraseAllLogs() {
    // TODO(E32F-26): Walk the files, collect each path where isLogFile() is
    // true, then LittleFS.remove() each one. Collect first and remove after:
    // deleting while you are still walking the directory can skip files.
    // Print how many were removed.
}

} // namespace

void handleSerialCommands(bool recording, bool& stopRequested) {
    if (!Serial.available()) {
        return;
    }
    const char c = static_cast<char>(Serial.read());

    // Second half of the erase confirmation.
    if (pendingErase) {
        pendingErase = false;
        if (c == 'Y') {
            eraseAllLogs();
        } else {
            Serial.println("Erase cancelled.");
        }
        return;
    }

    switch (c) {
        case 'x':
            stopRequested = true;
            break;
        case 'l':
            listFiles();
            break;
        case 'd':
            // Reading the file while it is still being written is asking for
            // trouble. Stop first.
            if (recording) {
                Serial.println("Stop recording first (x or BOOT).");
            } else {
                dumpLatest();
            }
            break;
        case 'e':
            if (recording) {
                Serial.println("Stop recording first (x or BOOT).");
            } else {
                Serial.println("Erase ALL log files? Type Y to confirm.");
                pendingErase = true;
            }
            break;
        default:
            break;  // ignore newlines and anything else
    }
}
