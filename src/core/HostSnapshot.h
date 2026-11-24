// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

// One thing a check wants from the host. `pattern` is an absolute host path. Any component may
// use shell wildcards (* ? [...]), e.g. "/sys/kernel/iommu_groups/*/type".
struct HostRequest {
    enum class Kind {
        Read, // read every matching file
        List, // only report which paths match (e.g. entries of /sys/class/iommu)
    };

    Kind kind = Kind::Read;
    QString pattern;

    static HostRequest read(const QString &pattern) { return {Kind::Read, pattern}; }
    static HostRequest list(const QString &pattern) { return {Kind::List, pattern}; }

    bool isGlob() const;
    QString key() const; // "r:<pattern>" or "l:<pattern>"

    friend bool operator==(const HostRequest &, const HostRequest &) = default;
};

struct HostEntry {
    enum class Status {
        Ok,        // Read: file was read, `data` holds its contents
        Present,   // List: path exists
        Directory, // Read: path is a directory, nothing to read
        Missing,   // path doesn't exist
        Error,     // exists but couldn't be read (usually permissions)
        NotRead,   // never requested, or the read as a whole failed / wasn't allowed
    };

    QString path; // host path, without any sysroot prefix
    Status status = Status::NotRead;
    QByteArray data;

    bool ok() const { return status == Status::Ok; }
    QByteArray trimmed() const { return data.trimmed(); }
};

// State of the one batch of privileged (pkexec) reads.
enum class PrivilegedState {
    NotRequested, // the user hasn't asked for the full check yet
    Granted,
    Cancelled, // the user dismissed the password dialog, or wasn't authorised
    NoAgent,   // no polkit agent to ask for the password
    Failed,    // anything else (pkexec missing, spawn failed, ...)
};

QString privilegedStateKey(PrivilegedState state);

// Everything read from the host in one run. Checks look entries up by the pattern they asked for.
class HostSnapshot
{
public:
    void insert(const HostRequest &request, QList<HostEntry> entries);

    // All matches for a request, sorted by path. Empty if nothing matched or it was never read.
    QList<HostEntry> matches(const HostRequest &request) const;
    QList<HostEntry> matches(const QString &readPattern) const { return matches(HostRequest::read(readPattern)); }

    // The entry for one exact path that was requested with HostRequest::read().
    // Status NotRead if it wasn't part of the snapshot.
    HostEntry file(const QString &path) const;

    bool contains(const HostRequest &request) const { return m_entries.contains(request.key()); }

    PrivilegedState privilegedState = PrivilegedState::NotRequested;
    QString privilegedError;

private:
    QHash<QString, QList<HostEntry>> m_entries;
};
