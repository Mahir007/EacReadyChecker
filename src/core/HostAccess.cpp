// SPDX-License-Identifier: GPL-3.0-or-later
#include "HostAccess.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

using namespace Qt::StringLiterals;

namespace
{

// Run as `sh -s -- <sysroot> <request>...` with this script on stdin.
// Arguments: <sysroot> <request>...   where each request is "r:<pattern>" or "l:<pattern>".
// Output, one line per entry: <request index> TAB <status> TAB <path> TAB <base64 data>
// Paths in /sys and /proc never contain tabs or newlines, and base64 has neither.
// Needs only POSIX sh and base64 (coreutils, uutils or busybox).
constexpr auto kHostScript = R"sh(export LC_ALL=C
IFS='
'
root=$1
shift
i=0
for req do
    kind=${req%%:*}
    pat=${req#?:}
    case $pat in *[*?[]*) glob=1 ;; *) glob=0 ;; esac
    for p in $root$pat; do
        rel=${p#"$root"}
        if [ ! -e "$p" ] && [ ! -L "$p" ]; then
            [ "$glob" = 1 ] || printf '%s\tmissing\t%s\t\n' "$i" "$rel"
        elif [ "$kind" = l ]; then
            printf '%s\tpresent\t%s\t\n' "$i" "$rel"
        elif [ -d "$p" ]; then
            printf '%s\tdirectory\t%s\t\n' "$i" "$rel"
        elif data=$(base64 -w0 < "$p" 2>/dev/null); then
            printf '%s\tok\t%s\t%s\n' "$i" "$rel" "$data"
        else
            printf '%s\terror\t%s\t\n' "$i" "$rel"
        fi
    done
    i=$((i + 1))
done
exit 0
)sh";

constexpr int kUserReadTimeoutMs = 30'000;
constexpr int kPollIntervalMs = 100;

bool hasWildcard(QStringView s)
{
    return s.contains(u'*') || s.contains(u'?') || s.contains(u'[');
}

// Expands `pattern` (absolute host path) below `root` the way sh would: wildcard components don't
// match dotfiles, and a pattern without wildcards is returned as-is even if it doesn't exist.
QStringList expand(const QString &root, const QString &pattern)
{
    if (!hasWildcard(pattern)) {
        return {pattern};
    }

    QStringList candidates{QString()};
    const QStringList components = pattern.split(u'/', Qt::SkipEmptyParts);
    for (const QString &component : components) {
        QStringList next;
        for (const QString &prefix : std::as_const(candidates)) {
            if (!hasWildcard(component)) {
                next.append(prefix + u'/' + component);
                continue;
            }
            const QDir dir(root + (prefix.isEmpty() ? u"/"_s : prefix));
            const QStringList names = dir.entryList({component},
                                                    QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot | QDir::CaseSensitive);
            for (const QString &name : names) {
                next.append(prefix + u'/' + name);
            }
        }
        candidates = std::move(next);
    }

    QStringList existing;
    for (const QString &path : std::as_const(candidates)) {
        const QFileInfo info(root + path);
        if (info.exists() || info.isSymLink()) {
            existing.append(path);
        }
    }
    return existing;
}

HostEntry readOne(const QString &root, const QString &path, HostRequest::Kind kind)
{
    HostEntry entry;
    entry.path = path;

    const QFileInfo info(root + path);
    if (!info.exists() && !info.isSymLink()) {
        entry.status = HostEntry::Status::Missing;
    } else if (kind == HostRequest::Kind::List) {
        entry.status = HostEntry::Status::Present;
    } else if (info.isDir()) {
        entry.status = HostEntry::Status::Directory;
    } else {
        QFile file(root + path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
            // sysfs reports a page-sized file size and /proc reports 0, so read until EOF.
            entry.data = file.readAll();
            entry.status = HostEntry::Status::Ok;
        } else {
            entry.status = HostEntry::Status::Error;
        }
    }
    return entry;
}

HostEntry::Status parseStatus(QByteArrayView s)
{
    if (s == "ok") {
        return HostEntry::Status::Ok;
    }
    if (s == "present") {
        return HostEntry::Status::Present;
    }
    if (s == "directory") {
        return HostEntry::Status::Directory;
    }
    if (s == "missing") {
        return HostEntry::Status::Missing;
    }
    return HostEntry::Status::Error;
}

void insertNotRead(const QList<HostRequest> &requests, HostSnapshot &snapshot)
{
    for (const HostRequest &request : requests) {
        snapshot.insert(request, {});
    }
}

} // namespace

HostAccess::HostAccess(Transport transport, const QStringList &hostCommandPrefix, const QString &sysroot)
    : m_transport(transport)
    , m_prefix(hostCommandPrefix)
    , m_sysroot(sysroot.endsWith(u'/') ? sysroot.chopped(1) : sysroot)
{
}

HostAccess HostAccess::forCurrentEnvironment(const QString &sysroot)
{
    if (isFlatpak()) {
        // --watch-bus: the host command dies with us, e.g. a pending pkexec when the window closes.
        return HostAccess(Transport::Shell, {u"flatpak-spawn"_s, u"--host"_s, u"--watch-bus"_s}, sysroot);
    }
    return HostAccess(Transport::Direct, {}, sysroot);
}

bool HostAccess::isFlatpak()
{
    return QFile::exists(u"/.flatpak-info"_s);
}

QString HostAccess::transportName() const
{
    if (m_transport == Transport::Direct) {
        return u"direct"_s;
    }
    return m_prefix.value(0) == u"flatpak-spawn" ? u"flatpak-spawn"_s : u"shell"_s;
}

HostAccess::Result HostAccess::read(const QList<HostRequest> &requests, Privilege privilege, HostSnapshot &snapshot, const std::atomic_bool *cancelled) const
{
    if (requests.isEmpty()) {
        Result result;
        if (privilege == Privilege::Admin) {
            result.privilegedState = PrivilegedState::Granted;
        }
        return result;
    }
    // Admin reads need pkexec, which only the script path supports. Fixture sysroots are read as-is.
    if (m_transport == Transport::Direct && (privilege == Privilege::User || !m_sysroot.isEmpty())) {
        Result result = readDirect(requests, snapshot);
        if (privilege == Privilege::Admin) {
            result.privilegedState = PrivilegedState::Granted;
        }
        return result;
    }
    return readShell(requests, privilege, snapshot, cancelled);
}

HostAccess::Result HostAccess::readDirect(const QList<HostRequest> &requests, HostSnapshot &snapshot) const
{
    for (const HostRequest &request : requests) {
        QList<HostEntry> entries;
        const QStringList paths = expand(m_sysroot, request.pattern);
        for (const QString &path : paths) {
            entries.append(readOne(m_sysroot, path, request.kind));
        }
        snapshot.insert(request, std::move(entries));
    }
    return {};
}

HostAccess::Result HostAccess::readShell(const QList<HostRequest> &requests, Privilege privilege, HostSnapshot &snapshot, const std::atomic_bool *cancelled) const
{
    const bool usePkexec = privilege == Privilege::Admin && m_sysroot.isEmpty();
    // For one exact file (the TPM event log), run plain `cat` as root: least privilege, and the
    // polkit dialog shows which file is being read.
    const bool useCat = usePkexec && requests.size() == 1 && requests[0].kind == HostRequest::Kind::Read && !requests[0].isGlob();

    QStringList command = m_prefix;
    if (usePkexec) {
        command << u"pkexec"_s;
    }
    if (useCat) {
        // cat is started by sh, not by pkexec directly. pkexec (polkit 127+) executes the
        // symlink-resolved path, and uutils' multi-call binary then sees `uu-coreutils` instead of
        // `cat` and fails. A shell runs cat by its own name.
        command << u"/bin/sh"_s << u"-c"_s << u"exec cat -- \"$1\""_s << u"sh"_s << requests[0].pattern;
    } else {
        // The script goes in on stdin so the command line (and any polkit prompt) stays short.
        command << u"/bin/sh"_s << u"-s"_s << u"--"_s << m_sysroot;
        for (const HostRequest &request : requests) {
            command << request.key();
        }
    }

    QProcess process;
    process.start(command.takeFirst(), command);
    bool finished = false;
    if (process.waitForStarted()) {
        if (!useCat) {
            process.write(kHostScript);
        }
        process.closeWriteChannel();

        // pkexec waits for the user to answer the password dialog, so only user reads time out.
        QElapsedTimer timer;
        timer.start();
        while (!(finished = process.waitForFinished(kPollIntervalMs))) {
            const bool timedOut = !usePkexec && timer.hasExpired(kUserReadTimeoutMs);
            if (timedOut || (cancelled && cancelled->load()) || process.state() == QProcess::NotRunning) {
                break;
            }
        }
    }

    Result result;
    if (privilege == Privilege::Admin) {
        result.privilegedState = PrivilegedState::Granted;
    }
    const bool exited = finished && process.exitStatus() == QProcess::NormalExit;
    const int exitCode = process.exitCode();
    const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();

    // cat ran (so admin access was granted) but couldn't read the file. Exit code 1 alone could
    // also be flatpak-spawn failing, so check the message came from cat.
    if (useCat && exited && exitCode == 1 && stderrText.contains(u"cat:"_s)) {
        HostEntry entry;
        entry.path = requests[0].pattern;
        entry.status = stderrText.contains(u"No such file"_s) ? HostEntry::Status::Missing : HostEntry::Status::Error;
        snapshot.insert(requests[0], {entry});
        return result;
    }

    if (!exited || exitCode != 0) {
        process.kill();
        process.waitForFinished();
        insertNotRead(requests, snapshot);
        result.ok = false;
        if (cancelled && cancelled->load()) {
            result.error = u"Cancelled"_s;
        } else if (!finished && process.error() != QProcess::FailedToStart) {
            result.error = u"Host read timed out"_s;
        } else {
            result.error = process.error() == QProcess::FailedToStart ? process.errorString() : stderrText;
        }
        if (result.error.isEmpty()) {
            result.error = u"Host read exited with code %1"_s.arg(exitCode);
        }

        if (usePkexec && exited) {
            // pkexec(1): 126 = the user dismissed the dialog, 127 = not authorised or an error.
            // Plasma's agent reports Cancel as "Not authorized" (127), like a wrong password.
            // Other 127s include having no authentication agent, or sh not finding cat.
            if (exitCode == 126 || (exitCode == 127 && stderrText.contains(u"Not authorized"_s))) {
                result.privilegedState = PrivilegedState::Cancelled;
            } else if (exitCode == 127 && stderrText.contains(u"authentication agent", Qt::CaseInsensitive)) {
                result.privilegedState = PrivilegedState::NoAgent;
            } else {
                result.privilegedState = PrivilegedState::Failed;
            }
        } else if (privilege == Privilege::Admin) {
            result.privilegedState = (cancelled && cancelled->load()) ? PrivilegedState::Cancelled : PrivilegedState::Failed;
        }
        return result;
    }

    const QByteArray out = process.readAllStandardOutput();
    if (useCat) {
        HostEntry entry;
        entry.path = requests[0].pattern;
        entry.status = HostEntry::Status::Ok;
        entry.data = out;
        snapshot.insert(requests[0], {entry});
        return result;
    }

    QList<QList<HostEntry>> entries(requests.size());
    const QList<QByteArray> lines = out.split('\n');
    for (const QByteArray &line : lines) {
        const QList<QByteArray> fields = line.split('\t');
        if (fields.size() != 4) {
            continue;
        }
        bool indexOk = false;
        const int index = fields[0].toInt(&indexOk);
        if (!indexOk || index < 0 || index >= requests.size()) {
            continue;
        }
        HostEntry entry;
        entry.status = parseStatus(fields[1]);
        entry.path = QString::fromUtf8(fields[2]);
        entry.data = QByteArray::fromBase64(fields[3]);
        entries[index].append(std::move(entry));
    }
    for (qsizetype i = 0; i < requests.size(); ++i) {
        snapshot.insert(requests[i], std::move(entries[i]));
    }
    return result;
}
