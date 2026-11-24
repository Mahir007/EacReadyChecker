// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

enum class CheckStatus {
    Pass,
    Fail,
    NotRequired,
    NeedsAdmin, // needs elevated access the user hasn't granted (yet)
    Unknown,    // couldn't be determined; counts as not passing
};

enum class Verdict {
    Ready,      // every check is Pass or NotRequired
    NotReady,   // at least one check failed
    Incomplete, // nothing failed, but something is NeedsAdmin or Unknown
};

struct SubCheckResult {
    QString title;
    CheckStatus status = CheckStatus::Unknown;
    QString detail;
};

struct CheckResult {
    QString id;
    QString title;
    CheckStatus status = CheckStatus::Unknown;
    QString detail;
    QList<SubCheckResult> subChecks;
    QJsonObject raw; // the values the check saw, for the detail line and --json
};

Verdict overallVerdict(const QList<CheckResult> &results);

// Status of a check from its sub-checks: Fail > NeedsAdmin > Unknown > Pass. NotRequired counts as
// passing; if every sub-check is NotRequired, so is the check. NeedsAdmin outranks Unknown because
// granting access is the one thing the user can do about it.
CheckStatus combineStatus(const QList<SubCheckResult> &subChecks);

QString statusKey(CheckStatus status);   // stable machine-readable name, such as "needs-admin"
QString statusLabel(CheckStatus status); // UI text, such as "Needs admin"
QString verdictKey(Verdict verdict);
QString verdictLabel(Verdict verdict);
