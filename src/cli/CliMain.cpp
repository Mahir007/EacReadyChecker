// SPDX-License-Identifier: GPL-3.0-or-later
#include "CliMain.h"

#include "checks/AllChecks.h"
#include "core/CheckRunner.h"

#include <QJsonDocument>

#include <cstdio>
#include <cstring>

bool isCliInvocation(int argc, char **argv)
{
    static constexpr const char *kCliFlags[] = {"--json", "--text", "-h", "--help", "--help-all", "-v", "--version", "--author", "--license"};
    for (int i = 1; i < argc; ++i) {
        for (const char *flag : kCliFlags) {
            if (std::strcmp(argv[i], flag) == 0) {
                return true;
            }
        }
    }
    return false;
}

int runCli(const HostAccess &host, CliFormat format, bool elevate)
{
    const Report report = CheckRunner::run(host, createAllChecks(), elevate);

    const QByteArray out = format == CliFormat::Json ? QJsonDocument(report.toJson()).toJson(QJsonDocument::Indented) : report.toText().toUtf8();
    std::fwrite(out.constData(), 1, size_t(out.size()), stdout);
    std::fflush(stdout);

    switch (report.verdict()) {
    case Verdict::Ready:
        return 0;
    case Verdict::NotReady:
        return 2;
    case Verdict::Incomplete:
        break;
    }
    return 3;
}
