// SPDX-License-Identifier: GPL-3.0-or-later
#include "HostSnapshot.h"

#include <algorithm>

using namespace Qt::StringLiterals;

bool hasWildcard(QStringView path)
{
    return path.contains(u'*') || path.contains(u'?') || path.contains(u'[');
}

bool HostRequest::isGlob() const
{
    return hasWildcard(pattern);
}

QString HostRequest::key() const
{
    return (kind == Kind::Read ? u"r:"_s : u"l:"_s) + pattern;
}

QString privilegedStateKey(PrivilegedState state)
{
    switch (state) {
    case PrivilegedState::NotRequested:
        return u"not-requested"_s;
    case PrivilegedState::Granted:
        return u"granted"_s;
    case PrivilegedState::Cancelled:
        return u"cancelled"_s;
    case PrivilegedState::NoAgent:
        return u"no-agent"_s;
    case PrivilegedState::Failed:
        break;
    }
    return u"failed"_s;
}

void HostSnapshot::insert(const HostRequest &request, QList<HostEntry> entries)
{
    // Byte-wise path order, so results don't depend on locale or on which transport read them.
    std::sort(entries.begin(), entries.end(), [](const HostEntry &a, const HostEntry &b) {
        return a.path.toUtf8() < b.path.toUtf8();
    });
    m_entries.insert(request.key(), std::move(entries));
}

QList<HostEntry> HostSnapshot::matches(const HostRequest &request) const
{
    return m_entries.value(request.key());
}

HostEntry HostSnapshot::file(const QString &path) const
{
    const QList<HostEntry> entries = m_entries.value(HostRequest::read(path).key());
    for (const HostEntry &entry : entries) {
        if (entry.path == path) {
            return entry;
        }
    }
    HostEntry notRead;
    notRead.path = path;
    return notRead;
}
