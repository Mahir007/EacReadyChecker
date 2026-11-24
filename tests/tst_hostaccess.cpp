// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestSysroot.h"
#include "core/HostAccess.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QTest>
#include <QThread>

#include <atomic>

#include <unistd.h>

using namespace Qt::StringLiterals;
using Status = HostEntry::Status;
using Transport = HostAccess::Transport;
using Privilege = HostAccess::Privilege;

// The Shell transport runs the same script Flatpak runs through `flatpak-spawn --host`, just
// without the prefix, so both transports are tested against the same sysroot and must agree.
class TestHostAccess : public QObject
{
    Q_OBJECT

private:
    TestSysroot m_root;
    const QByteArray m_binary = QByteArray("\x00\x01\n\t\xff tail\n", 10);

    HostSnapshot read(Transport transport, const QList<HostRequest> &requests, Privilege privilege = Privilege::User)
    {
        HostSnapshot snapshot;
        const HostAccess host(transport, {}, m_root.path());
        const HostAccess::Result result = host.read(requests, privilege, snapshot);
        [&] {
            QVERIFY2(result.ok, qPrintable(result.error));
        }();
        return snapshot;
    }

    static void addTransports()
    {
        QTest::addColumn<Transport>("transport");
        QTest::newRow("direct") << Transport::Direct;
        QTest::newRow("shell") << Transport::Shell;
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_root.write(u"/proc/sys/kernel/tainted"_s, "8192\n");
        m_root.write(u"/sys/kernel/security/lsm"_s, "lockdown,capability,yama,bpf");
        m_root.write(u"/sys/empty"_s, "");
        m_root.write(u"/sys/binary"_s, m_binary);
        m_root.write(u"/sys/kernel/iommu_groups/0/type"_s, "DMA-FQ\n");
        m_root.write(u"/sys/kernel/iommu_groups/1/type"_s, "DMA\n");
        m_root.write(u"/sys/kernel/iommu_groups/10/type"_s, "identity\n");
        m_root.mkdir(u"/sys/kernel/iommu_groups/2"_s); // a group without a type file
        m_root.mkdir(u"/sys/class/iommu/dmar0"_s);
        m_root.mkdir(u"/sys/class/iommu/ivhd1"_s);
        m_root.write(u"/sys/class/iommu/.hidden"_s, "dotfiles don't match *");
        m_root.symlink(u"/nonexistent/target"_s, u"/sys/broken"_s);
        m_root.write(u"/sys/secret"_s, "root only");
        m_root.setPermissions(u"/sys/secret"_s, QFileDevice::WriteOwner);
    }

    void exactFile_data() { addTransports(); }
    void exactFile()
    {
        QFETCH(Transport, transport);
        const HostSnapshot s = read(transport, {HostRequest::read(u"/proc/sys/kernel/tainted"_s), HostRequest::read(u"/sys/kernel/security/lsm"_s)});

        const HostEntry tainted = s.file(u"/proc/sys/kernel/tainted"_s);
        QCOMPARE(tainted.status, Status::Ok);
        QCOMPARE(tainted.path, u"/proc/sys/kernel/tainted"_s);
        QCOMPARE(tainted.data, QByteArray("8192\n"));
        QCOMPARE(tainted.trimmed(), QByteArray("8192"));
        QCOMPARE(s.file(u"/sys/kernel/security/lsm"_s).data, QByteArray("lockdown,capability,yama,bpf"));
    }

    void specialFiles_data() { addTransports(); }
    void specialFiles()
    {
        QFETCH(Transport, transport);
        const HostSnapshot s = read(transport,
                                    {HostRequest::read(u"/sys/empty"_s),
                                     HostRequest::read(u"/sys/binary"_s),
                                     HostRequest::read(u"/sys/missing"_s),
                                     HostRequest::read(u"/sys/kernel"_s),
                                     HostRequest::read(u"/sys/broken"_s)});

        QCOMPARE(s.file(u"/sys/empty"_s).status, Status::Ok);
        QCOMPARE(s.file(u"/sys/empty"_s).data, QByteArray());
        QCOMPARE(s.file(u"/sys/binary"_s).status, Status::Ok);
        QCOMPARE(s.file(u"/sys/binary"_s).data, m_binary);
        QCOMPARE(s.file(u"/sys/missing"_s).status, Status::Missing);
        QCOMPARE(s.file(u"/sys/kernel"_s).status, Status::Directory);
        QCOMPARE(s.file(u"/sys/broken"_s).status, Status::Error);
        QCOMPARE(s.file(u"/sys/never-requested"_s).status, Status::NotRead);
    }

    void unreadable_data() { addTransports(); }
    void unreadable()
    {
        if (geteuid() == 0) {
            QSKIP("root can read everything");
        }
        QFETCH(Transport, transport);
        const HostSnapshot s = read(transport, {HostRequest::read(u"/sys/secret"_s)});
        QCOMPARE(s.file(u"/sys/secret"_s).status, Status::Error);
    }

    void globRead_data() { addTransports(); }
    void globRead()
    {
        QFETCH(Transport, transport);
        const HostSnapshot s = read(transport, {HostRequest::read(u"/sys/kernel/iommu_groups/*/type"_s)});

        const QList<HostEntry> groups = s.matches(u"/sys/kernel/iommu_groups/*/type"_s);
        QCOMPARE(groups.size(), 3);
        // byte-wise order
        QCOMPARE(groups[0].path, u"/sys/kernel/iommu_groups/0/type"_s);
        QCOMPARE(groups[1].path, u"/sys/kernel/iommu_groups/1/type"_s);
        QCOMPARE(groups[2].path, u"/sys/kernel/iommu_groups/10/type"_s);
        QCOMPARE(groups[0].trimmed(), QByteArray("DMA-FQ"));
        QCOMPARE(groups[2].trimmed(), QByteArray("identity"));
        // file() only covers exact-path requests.
        QCOMPARE(s.file(u"/sys/kernel/iommu_groups/0/type"_s).status, Status::NotRead);
    }

    void globNoMatch_data() { addTransports(); }
    void globNoMatch()
    {
        QFETCH(Transport, transport);
        const HostSnapshot s = read(transport, {HostRequest::read(u"/sys/class/tpm/*/tpm_version_major"_s), HostRequest::list(u"/nonexistent/*"_s)});
        QVERIFY(s.contains(HostRequest::read(u"/sys/class/tpm/*/tpm_version_major"_s)));
        QVERIFY(s.matches(u"/sys/class/tpm/*/tpm_version_major"_s).isEmpty());
        QVERIFY(s.matches(HostRequest::list(u"/nonexistent/*"_s)).isEmpty());
    }

    void list_data() { addTransports(); }
    void list()
    {
        QFETCH(Transport, transport);
        const HostRequest iommu = HostRequest::list(u"/sys/class/iommu/*"_s);
        const HostSnapshot s = read(transport, {iommu, HostRequest::list(u"/sys/broken"_s), HostRequest::list(u"/sys/missing"_s)});

        const QList<HostEntry> entries = s.matches(iommu);
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].path, u"/sys/class/iommu/dmar0"_s);
        QCOMPARE(entries[0].status, Status::Present);
        QCOMPARE(entries[1].path, u"/sys/class/iommu/ivhd1"_s);
        QCOMPARE(s.matches(HostRequest::list(u"/sys/broken"_s)).value(0).status, Status::Present);
        QCOMPARE(s.matches(HostRequest::list(u"/sys/missing"_s)).value(0).status, Status::Missing);
    }

    void transportsAgree()
    {
        const QList<HostRequest> requests{
            HostRequest::read(u"/proc/sys/kernel/tainted"_s),
            HostRequest::read(u"/sys/binary"_s),
            HostRequest::read(u"/sys/missing"_s),
            HostRequest::read(u"/sys/kernel/iommu_groups/*/type"_s),
            HostRequest::read(u"/sys/kernel/iommu_groups/*"_s),
            HostRequest::list(u"/sys/class/iommu/*"_s),
            HostRequest::read(u"/sys/*"_s),
        };
        const HostSnapshot direct = read(Transport::Direct, requests);
        const HostSnapshot shell = read(Transport::Shell, requests);
        for (const HostRequest &request : requests) {
            const QList<HostEntry> a = direct.matches(request);
            const QList<HostEntry> b = shell.matches(request);
            QCOMPARE(a.size(), b.size());
            for (qsizetype i = 0; i < a.size(); ++i) {
                QCOMPARE(a[i].path, b[i].path);
                QCOMPARE(a[i].status, b[i].status);
                QCOMPARE(a[i].data, b[i].data);
            }
        }
    }

    void adminWithSysrootSkipsPkexec_data() { addTransports(); }
    void adminWithSysrootSkipsPkexec()
    {
        QFETCH(Transport, transport);
        HostSnapshot s;
        const HostAccess host(transport, {}, m_root.path());
        const HostAccess::Result result = host.read({HostRequest::read(u"/sys/binary"_s)}, Privilege::Admin, s);
        QVERIFY(result.ok);
        QCOMPARE(result.privilegedState, PrivilegedState::Granted);
        QCOMPARE(s.file(u"/sys/binary"_s).data, m_binary);
    }

    void spawnFailure()
    {
        HostSnapshot s;
        const HostAccess host(Transport::Shell, {u"/nonexistent/flatpak-spawn"_s});
        const HostAccess::Result result = host.read({HostRequest::read(u"/proc/cmdline"_s)}, Privilege::User, s);
        QVERIFY(!result.ok);
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(s.file(u"/proc/cmdline"_s).status, Status::NotRead);
    }

    void pkexecExitCodes_data()
    {
        QTest::addColumn<QString>("fakeHost");
        QTest::addColumn<PrivilegedState>("expected");
        // The prefix stands in for the host: it gets "pkexec /bin/sh -c ..." as arguments and
        // exits like pkexec would.
        QTest::newRow("dismissed") << u"exit 126"_s << PrivilegedState::Cancelled;
        QTest::newRow("not authorised") << u"echo 'Error executing command as another user: Not authorized' >&2; exit 127"_s << PrivilegedState::Cancelled;
        QTest::newRow("no agent") << u"echo 'Error creating textual authentication agent: no tty' >&2; exit 127"_s << PrivilegedState::NoAgent;
        QTest::newRow("other 127") << u"echo 'sh: 1: exec: cat: not found' >&2; exit 127"_s << PrivilegedState::Failed;
        QTest::newRow("other error") << u"exit 1"_s << PrivilegedState::Failed;
    }
    void pkexecExitCodes()
    {
        QFETCH(QString, fakeHost);
        QFETCH(PrivilegedState, expected);

        HostSnapshot s;
        const HostAccess host(Transport::Shell, {u"/bin/sh"_s, u"-c"_s, fakeHost, u"fake-host"_s});
        const HostAccess::Result result = host.read({HostRequest::read(u"/sys/kernel/security/tpm0/binary_bios_measurements"_s)}, Privilege::Admin, s);
        QVERIFY(!result.ok);
        QCOMPARE(result.privilegedState, expected);
        QCOMPARE(s.file(u"/sys/kernel/security/tpm0/binary_bios_measurements"_s).status, Status::NotRead);
    }

    // Stands in for the host: checks it was asked to run pkexec with a shell (never a coreutils
    // binary directly, see HostAccess::readShell), then runs the rest unprivileged.
    static QStringList fakePkexecHost()
    {
        return {u"/bin/sh"_s, u"-c"_s, u"[ \"$1\" = pkexec ] && [ \"$2\" = /bin/sh ] || exit 99; shift; exec \"$@\""_s, u"fake-host"_s};
    }

    void adminSingleFileUsesCat()
    {
        HostSnapshot s;
        const HostAccess host(Transport::Shell, fakePkexecHost());
        const HostAccess::Result result = host.read({HostRequest::read(u"/proc/self/status"_s)}, Privilege::Admin, s);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.privilegedState, PrivilegedState::Granted);
        QCOMPARE(s.file(u"/proc/self/status"_s).status, Status::Ok);
        QVERIFY(s.file(u"/proc/self/status"_s).data.contains("Name:\tcat"));
    }

    void adminSingleFileMissing()
    {
        HostSnapshot s;
        const HostAccess host(Transport::Shell, fakePkexecHost());
        const HostAccess::Result result = host.read({HostRequest::read(u"/nonexistent/event-log"_s)}, Privilege::Admin, s);
        QVERIFY(result.ok);
        QCOMPARE(result.privilegedState, PrivilegedState::Granted);
        QCOMPARE(s.file(u"/nonexistent/event-log"_s).status, Status::Missing);
    }

    void adminBatchUsesScript()
    {
        HostSnapshot s;
        const HostAccess host(Transport::Shell, fakePkexecHost());
        const QList<HostRequest> requests{HostRequest::read(u"/proc/self/status"_s), HostRequest::read(u"/nonexistent/file"_s)};
        const HostAccess::Result result = host.read(requests, Privilege::Admin, s);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.privilegedState, PrivilegedState::Granted);
        QVERIFY(s.file(u"/proc/self/status"_s).data.contains("Name:\tbase64"));
        QCOMPARE(s.file(u"/nonexistent/file"_s).status, Status::Missing);
    }

    // uutils, as on Arch with uutils as coreutils, is one multi-call binary. Run both host reads
    // with its tools first on PATH, and pkexec emulated the way polkit 127+ runs a program: the
    // symlink-resolved path, with the original argv[0]. Set EACHECK_UUTILS_BIN to a directory of
    // plain-named uutils links; Homebrew's uutils-coreutils is found by default.
    void uutilsHost()
    {
        QString uubin = qEnvironmentVariable("EACHECK_UUTILS_BIN");
        if (uubin.isEmpty()) {
            uubin = u"/home/linuxbrew/.linuxbrew/opt/uutils-coreutils/libexec/uubin"_s;
        }
        if (!QFileInfo::exists(uubin + u"/cat"_s) || !QFileInfo::exists(u"/bin/bash"_s)) {
            QSKIP("uutils not found (set EACHECK_UUTILS_BIN)");
        }
        const QString path = u"PATH="_s + uubin + u":/usr/bin:/bin"_s;

        // User read: the batch script's base64 is uutils.
        HostSnapshot user;
        const HostAccess userHost(Transport::Shell, {u"/usr/bin/env"_s, path}, m_root.path());
        QVERIFY(userHost.read({HostRequest::read(u"/sys/binary"_s)}, Privilege::User, user).ok);
        QCOMPARE(user.file(u"/sys/binary"_s).data, m_binary);

        // Admin read of one file. /bin/<tool> stands for uutils when uutils has that tool.
        const QString fakePkexec = u"[ \"$1\" = pkexec ] || exit 99; p=$2; shift 2; q=$p; [ -e \"$UUBIN/${p##*/}\" ] && q=$UUBIN/${p##*/}; "
                                   "exec -a \"$p\" \"$(readlink -f \"$q\")\" \"$@\""_s;
        HostSnapshot admin;
        const HostAccess adminHost(Transport::Shell, {u"/usr/bin/env"_s, path, u"UUBIN="_s + uubin, u"/bin/bash"_s, u"-c"_s, fakePkexec, u"fake-host"_s});
        const HostAccess::Result result = adminHost.read({HostRequest::read(m_root.path() + u"/sys/binary"_s)}, Privilege::Admin, admin);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(admin.file(m_root.path() + u"/sys/binary"_s).data, m_binary);
    }

    void cancelPendingAdminRead()
    {
        // A password dialog nobody answers.
        HostSnapshot s;
        const HostAccess host(Transport::Shell, {u"/bin/sh"_s, u"-c"_s, u"sleep 30"_s, u"fake-host"_s});
        std::atomic_bool cancelled = false;
        QElapsedTimer timer;
        timer.start();
        // The read blocks this thread, so the timer has to fire from another one.
        QThread *thread = QThread::create([&] {
            QThread::msleep(200);
            cancelled = true;
        });
        thread->start();
        const HostAccess::Result result
            = host.read({HostRequest::read(u"/sys/kernel/security/tpm0/binary_bios_measurements"_s)}, Privilege::Admin, s, &cancelled);
        thread->wait();
        delete thread;
        QVERIFY(timer.elapsed() < 5000);
        QVERIFY(!result.ok);
        QCOMPARE(result.privilegedState, PrivilegedState::Cancelled);
    }
};

QTEST_GUILESS_MAIN(TestHostAccess)
#include "tst_hostaccess.moc"
