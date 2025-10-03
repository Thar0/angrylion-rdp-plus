#pragma once
#include "common.h"

NORETURN void
msg_error(const char *err, ...);
void
msg_warning(const char *err, ...);
void
msg_debug(const char *err, ...);
