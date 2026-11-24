// SPDX-License-Identifier: GPL-3.0-or-later
#include "TcgEventLog.h"
#include "EfiGuid.h"

#include <QCryptographicHash>
#include <QtEndian>

using namespace Qt::StringLiterals;

namespace
{
const QByteArray SpecIdSignature("Spec ID Event03\0", 16);
const QByteArray StartupLocalitySignature("StartupLocality\0", 16);
// Far more than any TPM has. Keeps a corrupt count from walking off with the parser.
constexpr quint32 MaxAlgorithms = 16;

// Bounds-checked little-endian reader.
class Reader
{
public:
    explicit Reader(const QByteArray &data)
        : m_data(data)
    {
    }

    bool atEnd() const { return m_pos == m_data.size(); }
    qsizetype remaining() const { return m_data.size() - m_pos; }

    template<typename T>
    bool read(T &value)
    {
        if (remaining() < qsizetype(sizeof(T))) {
            return false;
        }
        value = qFromLittleEndian<T>(m_data.constData() + m_pos);
        m_pos += sizeof(T);
        return true;
    }

    bool bytes(quint64 size, QByteArray &out)
    {
        if (size > quint64(remaining())) {
            return false;
        }
        out = m_data.mid(m_pos, qsizetype(size));
        m_pos += qsizetype(size);
        return true;
    }

private:
    const QByteArray &m_data;
    qsizetype m_pos = 0;
};

std::optional<QCryptographicHash::Algorithm> hashAlgorithm(quint16 algorithm)
{
    switch (algorithm) {
    case TcgEventLog::Sha1:
        return QCryptographicHash::Sha1;
    case TcgEventLog::Sha256:
        return QCryptographicHash::Sha256;
    case TcgEventLog::Sha384:
        return QCryptographicHash::Sha384;
    case TcgEventLog::Sha512:
        return QCryptographicHash::Sha512;
    default:
        return std::nullopt;
    }
}

// PFP section 10.4.5.3: the locality the TPM started in, which becomes the last byte of PCR 0.
quint8 startupLocality(const TcgEventLog::Log &log)
{
    bool hcrtm = false;
    for (const TcgEventLog::Event &event : log.events) {
        if (event.pcr != 0) {
            continue;
        }
        if (event.type == TcgEventLog::NoAction && event.data.size() > StartupLocalitySignature.size() && event.data.startsWith(StartupLocalitySignature)) {
            return quint8(event.data.at(StartupLocalitySignature.size()));
        }
        hcrtm = hcrtm || event.type == TcgEventLog::EfiHcrtmEvent;
    }
    return hcrtm ? 4 : 0;
}
} // namespace

namespace TcgEventLog
{
ParseResult parse(const QByteArray &data)
{
    ParseResult result;
    result.error = ParseResult::Error::Malformed;
    Reader reader(data);

    quint32 pcr = 0;
    quint32 type = 0;
    QByteArray sha1;
    quint32 size = 0;
    QByteArray specId;
    if (!reader.read(pcr) || !reader.read(type) || !reader.bytes(20, sha1) || !reader.read(size) || !reader.bytes(size, specId)) {
        // Too short for even one SHA-1 event. A legacy log would have parsed this far.
        return result;
    }
    if (type != NoAction || !specId.startsWith(SpecIdSignature)) {
        result.error = ParseResult::Error::NotCryptoAgile;
        return result;
    }

    // TCG_EfiSpecIdEvent: signature, platformClass, version and errata, uintnSize,
    // numberOfAlgorithms, then {algorithmId, digestSize} pairs.
    Reader spec(specId);
    QByteArray skipped;
    quint32 algorithmCount = 0;
    if (!spec.bytes(SpecIdSignature.size() + 4 + 4, skipped) || !spec.read(algorithmCount) || algorithmCount == 0 || algorithmCount > MaxAlgorithms) {
        return result;
    }
    QHash<quint16, quint16> digestSizes;
    for (quint32 i = 0; i < algorithmCount; ++i) {
        quint16 algorithm = 0;
        quint16 digestSize = 0;
        if (!spec.read(algorithm) || !spec.read(digestSize) || digestSizes.contains(algorithm)) {
            return result;
        }
        digestSizes.insert(algorithm, digestSize);
        result.log.algorithms.append(algorithm);
    }

    // TCG_PCR_EVENT2: PCRIndex, EventType, digest count, {algorithmId, digest}..., EventSize, event.
    while (!reader.atEnd()) {
        Event event;
        quint32 digestCount = 0;
        if (!reader.read(event.pcr) || !reader.read(event.type) || !reader.read(digestCount) || digestCount > MaxAlgorithms) {
            return result;
        }
        for (quint32 i = 0; i < digestCount; ++i) {
            quint16 algorithm = 0;
            QByteArray digest;
            if (!reader.read(algorithm) || !digestSizes.contains(algorithm) || !reader.bytes(digestSizes.value(algorithm), digest)) {
                return result;
            }
            event.digests.insert(algorithm, digest);
        }
        quint32 eventSize = 0;
        if (!reader.read(eventSize) || !reader.bytes(eventSize, event.data)) {
            return result;
        }
        result.log.events.append(std::move(event));
    }

    result.error = ParseResult::Error::None;
    return result;
}

std::optional<QMap<quint32, QByteArray>> replay(const Log &log, quint16 algorithm)
{
    const auto hash = hashAlgorithm(algorithm);
    if (!hash || !log.algorithms.contains(algorithm)) {
        return std::nullopt;
    }
    const int digestSize = QCryptographicHash::hashLength(*hash);
    const quint8 locality = startupLocality(log);

    QMap<quint32, QByteArray> pcrs;
    for (const Event &event : log.events) {
        // EV_NO_ACTION events are informational and never extended.
        if (event.type == NoAction) {
            continue;
        }
        const QByteArray digest = event.digests.value(algorithm);
        if (digest.size() != digestSize) {
            return std::nullopt;
        }
        auto it = pcrs.find(event.pcr);
        if (it == pcrs.end()) {
            QByteArray initial(digestSize, '\0');
            if (event.pcr == 0) {
                initial[digestSize - 1] = char(locality);
            }
            it = pcrs.insert(event.pcr, initial);
        }
        QCryptographicHash extend(*hash);
        extend.addData(*it);
        extend.addData(digest);
        *it = extend.result();
    }
    return pcrs;
}

std::optional<VariableData> variableData(const Event &event)
{
    // VariableName (GUID), UnicodeNameLength, VariableDataLength, UnicodeName, VariableData.
    Reader reader(event.data);
    QByteArray guid;
    quint64 nameLength = 0;
    quint64 dataLength = 0;
    QByteArray name;
    VariableData variable;
    if (!reader.bytes(16, guid) || !reader.read(nameLength) || !reader.read(dataLength) || nameLength > quint64(reader.remaining()) / 2
        || !reader.bytes(nameLength * 2, name) || !reader.bytes(dataLength, variable.data)) {
        return std::nullopt;
    }
    variable.guid = readEfiGuid(guid.constData());
    variable.name = QString::fromUtf16(reinterpret_cast<const char16_t *>(name.constData()), name.size() / 2);
    return variable;
}

QString text(const Event &event)
{
    const QByteArray &data = event.data;
    // UTF-16LE if every second byte is zero. One-character strings decode the same either way.
    bool utf16 = data.size() >= 2 && data.size() % 2 == 0;
    for (qsizetype i = 1; utf16 && i < data.size(); i += 2) {
        utf16 = data.at(i) == '\0';
    }
    QString out = utf16 ? QString::fromUtf16(reinterpret_cast<const char16_t *>(data.constData()), data.size() / 2) : QString::fromUtf8(data);
    while (out.endsWith(QChar(u'\0'))) {
        out.chop(1);
    }
    return out;
}

QString algorithmName(quint16 algorithm)
{
    switch (algorithm) {
    case Sha1:
        return u"sha1"_s;
    case Sha256:
        return u"sha256"_s;
    case Sha384:
        return u"sha384"_s;
    case Sha512:
        return u"sha512"_s;
    default:
        return u"0x%1"_s.arg(algorithm, 4, 16, QChar(u'0'));
    }
}
} // namespace TcgEventLog
