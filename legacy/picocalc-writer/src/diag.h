// Start-up diagnostics and crash reporting.
#pragma once
#include <cstdint>

#define FW_VERSION "2.1.0"

namespace diag {

enum Stage : uint16_t {
    kStart = 0,
    kDisplay,
    kKeyboard,
    kSdCard,
    kWorkDir,
    kScratch,
    kSettings,
    kLastFile,
    kFirstScreen,
};

void begin();          // call first thing in main(), before touching hardware
void displayReady();   // after lcd::init(): prints the header
void stage(Stage s);   // records a breadcrumb and prints a log line
void note(const char* s);
void done();           // start-up finished; clears the breadcrumb
bool hadIncompleteStart();
int previousStage();    // stage where the previous start stopped, or -1
[[noreturn]] void crashScreen(const char* what, uint32_t pc, uint32_t lr, const char* msg);

}  // namespace diag
