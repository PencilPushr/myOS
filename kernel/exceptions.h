// kernel/exceptions.h
// CPU Exception Handlers
#pragma once

#include "idt.h"

// Initialize all exception handlers
void exceptions_init(void);

// Exception names for display
extern const char *exception_names[32];
