// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CheckResult.h"
#include "HostSnapshot.h"

#include <QList>
#include <QString>

// One EAC requirement. Checks don't read anything themselves: they declare what they need, the
// runner reads it all in one batch (one flatpak-spawn call, one pkexec prompt), and evaluate()
// turns the snapshot into a result. That keeps checks pure and testable against fixture sysroots.
class Check
{
public:
    virtual ~Check() = default;

    virtual QString id() const = 0;    // stable key used in --json, e.g. "tpm"
    virtual QString title() const = 0; // UI title, e.g. "TPM 2.0"

    virtual QList<HostRequest> requests() const = 0;
    // Reads that need root. Only made after the user asks for the full check.
    virtual QList<HostRequest> privilegedRequests() const { return {}; }

    // id and title are filled in by the runner.
    virtual CheckResult evaluate(const HostSnapshot &snapshot) const = 0;
};
