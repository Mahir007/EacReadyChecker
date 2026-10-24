// SPDX-License-Identifier: GPL-3.0-or-later
#include "KernelCmdline.h"

namespace
{
bool sameName(QStringView a, QStringView b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (qsizetype i = 0; i < a.size(); ++i) {
        const QChar x = a[i] == u'-' ? u'_' : a[i];
        const QChar y = b[i] == u'-' ? u'_' : b[i];
        if (x != y) {
            return false;
        }
    }
    return true;
}
} // namespace

KernelCmdline KernelCmdline::parse(QByteArrayView cmdline)
{
    KernelCmdline result;
    const QString text = QString::fromUtf8(cmdline);

    QStringList tokens;
    QString token;
    bool inToken = false;
    bool inQuote = false;
    for (const QChar c : text) {
        if (c == u'"') {
            inQuote = !inQuote;
            inToken = true;
        } else if (c.isSpace() && !inQuote) {
            if (inToken) {
                tokens.append(token);
                token.clear();
                inToken = false;
            }
        } else {
            token.append(c);
            inToken = true;
        }
    }
    if (inToken) {
        tokens.append(token);
    }

    for (const QString &t : std::as_const(tokens)) {
        if (t == u"--") {
            break;
        }
        const qsizetype eq = t.indexOf(u'=');
        if (eq < 0) {
            result.m_params.append({t, std::nullopt});
        } else {
            result.m_params.append({t.left(eq), t.mid(eq + 1)});
        }
    }
    return result;
}

bool KernelCmdline::contains(QStringView name) const
{
    return std::any_of(m_params.begin(), m_params.end(), [name](const Param &p) {
        return sameName(p.name, name);
    });
}

QStringList KernelCmdline::values(QStringView name) const
{
    QStringList out;
    for (const Param &p : m_params) {
        if (sameName(p.name, name) && p.value) {
            out.append(*p.value);
        }
    }
    return out;
}

std::optional<bool> KernelCmdline::parseBool(QStringView value)
{
    if (value.isEmpty()) {
        return std::nullopt;
    }
    switch (value[0].unicode()) {
    case 'y':
    case 'Y':
    case '1':
        return true;
    case 'n':
    case 'N':
    case '0':
        return false;
    case 'o':
    case 'O':
        if (value.size() > 1) {
            if (value[1] == u'n' || value[1] == u'N') {
                return true;
            }
            if (value[1] == u'f' || value[1] == u'F') {
                return false;
            }
        }
        break;
    default:
        break;
    }
    return std::nullopt;
}
