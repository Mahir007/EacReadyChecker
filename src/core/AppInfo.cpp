// SPDX-License-Identifier: GPL-3.0-or-later
#include "AppInfo.h"

// Only this file includes the version, which changes with every commit in CI's builds, so the
// compiler cache misses on this file alone.
#include "eacheck_version.h"

QString AppInfo::version() { return QStringLiteral(EACHECK_VERSION_STRING); }
