// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

// A throwaway sysroot for tests: write host-style paths below a temporary directory.
class TestSysroot
{
public:
    QString path() const { return m_dir.path(); }

    void write(const QString &hostPath, const QByteArray &data) const
    {
        const QString full = m_dir.path() + hostPath;
        QDir().mkpath(QFileInfo(full).path());
        QFile file(full);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
        }
    }

    void mkdir(const QString &hostPath) const { QDir().mkpath(m_dir.path() + hostPath); }

    void symlink(const QString &target, const QString &hostPath) const
    {
        const QString full = m_dir.path() + hostPath;
        QDir().mkpath(QFileInfo(full).path());
        QFile::link(target, full);
    }

    void setPermissions(const QString &hostPath, QFileDevice::Permissions permissions) const
    {
        QFile::setPermissions(m_dir.path() + hostPath, permissions);
    }

private:
    QTemporaryDir m_dir;
};
