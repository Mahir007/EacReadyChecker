// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/HostAccess.h"

// True if argv asks for a headless run (--json, --text, --help, --version, ...). Decided before
// the application object exists, so CLI runs never need a display.
bool isCliInvocation(int argc, char **argv);

enum class CliFormat { Json, Text };

// Runs every check, prints the report to stdout and returns the exit code:
// 0 ready, 2 not ready, 3 incomplete. (1 is left for usage errors from QCommandLineParser.)
int runCli(const HostAccess &host, CliFormat format, bool elevate);
