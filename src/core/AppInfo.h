// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "eacheck_version.h"

#include <QString>

// App metadata for the About dialog, --version and the desktop integration.
namespace AppInfo
{
inline QString id() { return QStringLiteral(EACHECK_APP_ID); }
inline QString binaryName() { return QStringLiteral("eac-ready-checker"); }
inline QString displayName() { return QStringLiteral("EACheck"); }
inline QString version() { return QStringLiteral(EACHECK_VERSION_STRING); }
inline QString description() { return QStringLiteral("Checks whether this PC meets the Easy Anti-Cheat Linux v2 requirements"); }
inline QString copyright() { return QStringLiteral("© 2026 Mahir007"); }
inline QString license() { return QStringLiteral("GNU General Public License v3.0 or later"); }
inline QString homepage() { return QStringLiteral("https://github.com/Mahir007/EacReadyChecker"); }
// Epic's announcement that lists the requirements the checks follow.
inline QString requirementsUrl() { return QStringLiteral("https://onlineservices.epicgames.com/news/introducing-easy-anti-cheat-linux-v2"); }
} // namespace AppInfo
