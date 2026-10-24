// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "CheckRow.h"
#include "Icons.h"
#include "SummaryBanner.h"
#include "checks/AllChecks.h"
#include "core/AppInfo.h"

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace
{
QFrame *separator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}
} // namespace

MainWindow::MainWindow(const HostAccess &host, QWidget *parent)
    : QWidget(parent)
    , m_runner(new CheckRunner(host, createAllChecks(), this))
    , m_banner(new SummaryBanner(this))
    , m_runFull(new QPushButton(themeIcon({u"dialog-password"_s, u"security-high"_s}), u"Run full check"_s, this))
    , m_refresh(new QPushButton(themeIcon({u"view-refresh"_s}), u"Refresh"_s, this))
    , m_copy(new QPushButton(themeIcon({u"edit-copy"_s}), u"Copy report"_s, this))
{
    setWindowTitle(u"EACheck"_s);
    resize(560, 640);

    // Rows, in the article's order.
    auto *list = new QWidget;
    auto *listLayout = new QVBoxLayout(list);
    for (const auto &check : m_runner->checks()) {
        if (!m_rows.isEmpty()) {
            listLayout->addWidget(separator(list));
        }
        auto *row = new CheckRow(check->title(), list);
        m_rows.append(row);
        listLayout->addWidget(row);
    }
    listLayout->addStretch(1);

    auto *scroll = new QScrollArea(this);
    scroll->setWidget(list);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *note = new QLabel(u"Based on Epic's published requirements as of %1. EAC performs its own verification at game launch."_s.arg(requirementsAsOf()), this);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    QFont noteFont = note->font();
    noteFont.setPointSizeF(noteFont.pointSizeF() * 0.9);
    note->setFont(noteFont);

    m_runFull->setToolTip(u"Reads the TPM event log as root. You'll be asked for an admin password once."_s);
    m_runFull->hide();
    connect(m_runFull, &QPushButton::clicked, this, [this] {
        runChecks(true);
    });
    connect(m_refresh, &QPushButton::clicked, this, [this] {
        // Keep admin results once granted; otherwise a plain refresh.
        runChecks(m_report.privilegedState == PrivilegedState::Granted);
    });

    auto *copyMenu = new QMenu(m_copy);
    copyMenu->addAction(u"Copy as Text"_s, this, [this] {
        copyReport(false);
    });
    copyMenu->addAction(u"Copy as JSON"_s, this, [this] {
        copyReport(true);
    });
    m_copy->setMenu(copyMenu);

    auto *about = new QPushButton(themeIcon({u"help-about"_s}), u"About"_s, this);
    connect(about, &QPushButton::clicked, this, &MainWindow::showAbout);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(about);
    buttons->addStretch(1);
    buttons->addWidget(m_runFull);
    buttons->addWidget(m_copy);
    buttons->addWidget(m_refresh);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_banner);
    layout->addWidget(separator(this));
    layout->addWidget(scroll, 1);
    layout->addWidget(separator(this));
    layout->addWidget(note);
    layout->addLayout(buttons);

    connect(m_runner, &CheckRunner::started, this, &MainWindow::onStarted);
    connect(m_runner, &CheckRunner::finished, this, &MainWindow::onFinished);
}

void MainWindow::runChecks(bool elevate)
{
    m_elevating = elevate;
    m_runner->start(elevate);
}

void MainWindow::onStarted()
{
    m_banner->setRunning(m_elevating);
    for (CheckRow *row : std::as_const(m_rows)) {
        row->setPending();
    }
    m_runFull->setEnabled(false);
    m_refresh->setEnabled(false);
    m_copy->setEnabled(false);
}

void MainWindow::onFinished(const Report &report)
{
    m_report = report;
    m_banner->setReport(report);
    for (qsizetype i = 0; i < m_rows.size() && i < report.results.size(); ++i) {
        m_rows[i]->setResult(report.results[i]);
    }

    const bool needsAdmin = std::any_of(report.results.begin(), report.results.end(), [](const CheckResult &r) {
        return r.status == CheckStatus::NeedsAdmin;
    });
    m_runFull->setVisible(needsAdmin);
    m_runFull->setEnabled(true);
    m_refresh->setEnabled(true);
    m_copy->setEnabled(true);
}

void MainWindow::copyReport(bool json)
{
    const QString text = json ? QString::fromUtf8(QJsonDocument(m_report.toJson()).toJson(QJsonDocument::Indented)) : m_report.toText();
    QGuiApplication::clipboard()->setText(text);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this,
                       u"About %1"_s.arg(AppInfo::displayName()),
                       u"<h3>%1 %2</h3><p>%3</p><p>%4</p><p><a href=\"%5\">%5</a></p><p>License: %6</p>"_s.arg(AppInfo::displayName(),
                                                                                                         AppInfo::version(),
                                                                                                         AppInfo::description(),
                                                                                                         AppInfo::copyright(),
                                                                                                         AppInfo::homepage(),
                                                                                                         AppInfo::license()));
}
