// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArrayView>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// Tokenises /proc/cmdline the way the kernel does: whitespace-separated, double quotes group
// spaces, and everything after a bare "--" goes to init (ignored). Parameter names compare with
// '-' and '_' treated as equal, like the kernel's parameq().
class KernelCmdline
{
public:
    struct Param {
        QString name;
        std::optional<QString> value; // nullopt for a bare flag like "quiet"
    };

    static KernelCmdline parse(QByteArrayView cmdline);

    const QList<Param> &params() const { return m_params; }
    bool contains(QStringView name) const;
    // Values of every `name=value` in command-line order.
    QStringList values(QStringView name) const;

    // kstrtobool(): 1/y/Y/on → true, 0/n/N/off → false.
    static std::optional<bool> parseBool(QStringView value);

private:
    QList<Param> m_params;
};
