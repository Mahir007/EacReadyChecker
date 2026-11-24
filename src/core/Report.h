// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CheckResult.h"
#include "HostSnapshot.h"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

// Everything one run produced. Feeds the window, `--json` and "Copy report".
struct Report {
    QList<CheckResult> results;
    QDateTime generated;
    QString transport; // HostAccess::transportName()
    QString sysroot;
    QString hostReadError; // set if the unprivileged batch read failed as a whole
    bool elevationRequested = false;
    PrivilegedState privilegedState = PrivilegedState::NotRequested;
    QString privilegedError;

    Verdict verdict() const { return overallVerdict(results); }
    // Why results may be missing: the host read failed, or admin access wasn't granted. Lowercase
    // phrases, such as "admin access wasn't granted". Empty when neither happened.
    QStringList accessProblems() const;
    QJsonObject toJson() const;
    QString toText() const;
};

// Short "as of" date for the published requirements the checks follow.
QString requirementsAsOf();

// Raw value for a host entry: trimmed text for small text files, hex for small binaries such as EFI
// variables, otherwise size + sha256.
QJsonValue rawValue(const HostEntry &entry);

// Raw values for everything a check requested, keyed by path (or by pattern for List requests and
// globs without matches).
QJsonObject rawValues(const HostSnapshot &snapshot, const QList<HostRequest> &requests);
