#ifndef SIGNALS_H
#define SIGNALS_H

#include <sys/types.h>

void setup_shell_signals();

// This 'extern' keyword tells other files:
// "This variable exists, but it is defined somewhere else.
// You are allowed to use it."
extern volatile pid_t foreground_pgid;

#endif // SIGNALS_H