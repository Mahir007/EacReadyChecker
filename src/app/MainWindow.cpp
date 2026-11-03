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
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QTimer>
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
    , m_runFull(new QPushButton(u"Run full check"_s, this))
    , m_refresh(new QPushButton(u"Refresh"_s, this))
    , m_copy(new QPushButton(u"Copy report"_s, this))
    , m_about(new QPushButton(u"About"_s, this))
    , m_note(new QLabel(this))
    , m_copiedReset(new QTimer(this))
{
    syncIconThemes();
    updateIcons();
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

    updateNote();
    m_note->setWordWrap(true);
    m_note->setOpenExternalLinks(true);
    m_note->setForegroundRole(QPalette::PlaceholderText);
    QFont noteFont = m_note->font();
    noteFont.setPointSizeF(noteFont.pointSizeF() * 0.9);
    m_note->setFont(noteFont);

    m_runFull->setToolTip(u"Reads the TPM event log as root. You'll be asked for an admin password once."_s);
    m_runFull->hide();
    connect(m_runFull, &QPushButton::clicked, this, [this] {
        runChecks(true);
    });
    // Not QKeySequence::Refresh: that keeps only the platform's first binding, Ctrl+R on GNOME.
    m_refresh->setShortcut(Qt::Key_F5);
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
    // After a copy the button says "Copied" for a moment. Keep its width so the row doesn't move.
    m_copy->setMinimumWidth(m_copy->sizeHint().width());
    m_copiedReset->setSingleShot(true);
    m_copiedReset->setInterval(2000);
    connect(m_copiedReset, &QTimer::timeout, this, [this] {
        m_copy->setText(u"Copy report"_s);
    });

    new QShortcut(QKeySequence::Quit, this, this, &QWidget::close);

    connect(m_about, &QPushButton::clicked, this, &MainWindow::showAbout);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_about);
    buttons->addStretch(1);
    buttons->addWidget(m_runFull);
    buttons->addWidget(m_copy);
    buttons->addWidget(m_refresh);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_banner);
    layout->addWidget(separator(this));
    layout->addWidget(scroll, 1);
    layout->addWidget(separator(this));
    layout->addWidget(m_note);
    layout->addLayout(buttons);

    connect(m_runner, &CheckRunner::started, this, &MainWindow::onStarted);
    connect(m_runner, &CheckRunner::finished, this, &MainWindow::onFinished);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (affectsIcons(event)) {
        syncIconThemes();
        updateIcons();
    }
    if (event->type() == QEvent::PaletteChange) {
        updateNote();
    }
    QWidget::changeEvent(event);
}

void MainWindow::updateIcons()
{
    m_runFull->setIcon(themeIcon({u"dialog-password"_s, u"security-high"_s}));
    m_refresh->setIcon(themeIcon({u"view-refresh"_s}));
    m_copy->setIcon(themeIcon({u"edit-copy"_s}));
    m_about->setIcon(themeIcon({u"help-about"_s}));
}

void MainWindow::updateNote()
{
    // Qt picks the link colour when it parses the text. setText() skips text it already has.
    m_note->clear();
    m_note->setText(u"Based on <a href=\"%1\">Epic's published requirements</a> as of %2. EAC performs its own verification at game launch."_s.arg(AppInfo::requirementsUrl(), requirementsAsOf()));
}

void MainWindow::runChecks(bool elevate)
{
    m_elevating = elevate;
    m_runner->start(elevate);
}

void MainWindow::onStarted()
{
    m_banner->setRunning(m_elevating);
    // Behind the password dialog, only the rows that need it wait. The others keep their results,
    // so it's clear what the password is for.
    const bool keepResults = m_elevating && !m_report.results.isEmpty();
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (!keepResults || !m_runner->checks()[i]->privilegedRequests().isEmpty()) {
            m_rows[i]->setPending();
        }
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
    m_copy->setText(u"Copied"_s);
    m_copiedReset->start();
}

void MainWindow::showAbout()
{
    // Not QMessageBox::about(): that blocks in its own event loop, so the window couldn't be tested.
    auto *box = new QMessageBox(this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowTitle(u"About %1"_s.arg(AppInfo::displayName()));
    box->setIconPixmap(appIcon().pixmap(QSize(64, 64), devicePixelRatio()));
    box->setTextFormat(Qt::RichText);
    box->setText(u"<h3>%1 %2</h3>"
                 "<p>%3.</p>"
                 "<p>Follows <a href=\"%4\">Epic's published requirements</a> as of %5.</p>"
                 "<p>%6<br>License: %7<br><a href=\"%8\">%8</a></p>"
                 "<p>Qt %9</p>"_s.arg(AppInfo::displayName(),
                                      AppInfo::version(),
                                      AppInfo::description(),
                                      AppInfo::requirementsUrl(),
                                      requirementsAsOf(),
                                      AppInfo::copyright(),
                                      AppInfo::license(),
                                      AppInfo::homepage(),
                                      QString::fromLatin1(qVersion())));
    box->setStandardButtons(QMessageBox::Close);
    box->open();
}
