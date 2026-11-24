// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

// X.509 certificates, identified by the SHA-256 of their DER bytes.
namespace Certificate
{
QByteArray sha256(const QByteArray &der);

// "A3:B3:...:2E", the form openssl and the key list use.
QString fingerprint(const QByteArray &sha256);

// True if `data` is exactly one DER SEQUENCE, the outer shape of an X.509 certificate.
bool isDer(const QByteArray &data);

// Short subject for display, such as "CN=fedoraca, OU=Fedora Secure Boot CA 20200709, O=Red Hat, Inc.".
// Empty if the certificate can't be parsed. Never used for matching.
QString subject(const QByteArray &der);
} // namespace Certificate
