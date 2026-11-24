// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "HostSnapshot.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <atomic>

// Every read of host state goes through here.
//
// - Direct: read files in-process (native builds, AppImage, tests).
// - Shell:  feed a small POSIX sh script to `sh -s` on the host. It reads all requests in one go
//           and prints them base64-framed. Inside Flatpak the command is prefixed with
//           `flatpak-spawn --host`, because the sandbox can't see /sys/firmware,
//           /sys/kernel/security or /sys/kernel/iommu_groups.
//
// Admin reads always go through `pkexec`, so there is one password prompt per batch. A batch of
// one exact file runs `pkexec cat -- <file>`; anything else runs the script. With a sysroot set
// (tests and fixtures), every path is read below that directory and admin reads skip pkexec.
class HostAccess
{
public:
    enum class Transport { Direct, Shell };
    enum class Privilege { User, Admin };

    struct Result {
        bool ok = true;
        PrivilegedState privilegedState = PrivilegedState::NotRequested; // Admin reads only
        QString error;
    };

    HostAccess(Transport transport, const QStringList &hostCommandPrefix = {}, const QString &sysroot = {});

    // Direct natively, Shell via `flatpak-spawn --host` inside Flatpak.
    static HostAccess forCurrentEnvironment(const QString &sysroot = {});
    static bool isFlatpak();

    // Reads every request into `snapshot`. If the read as a whole fails, the requests are inserted
    // with status NotRead and the error is returned. Setting `cancelled` aborts a pending read,
    // such as one waiting on the password dialog.
    Result read(const QList<HostRequest> &requests, Privilege privilege, HostSnapshot &snapshot, const std::atomic_bool *cancelled = nullptr) const;

    Transport transport() const { return m_transport; }
    QString transportName() const; // "direct", "shell" or "flatpak-spawn"
    QString sysroot() const { return m_sysroot; }

private:
    Result readDirect(const QList<HostRequest> &requests, HostSnapshot &snapshot) const;
    Result readShell(const QList<HostRequest> &requests, Privilege privilege, HostSnapshot &snapshot, const std::atomic_bool *cancelled) const;

    Transport m_transport;
    QStringList m_prefix;
    QString m_sysroot;
};
