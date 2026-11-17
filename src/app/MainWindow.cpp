// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "CheckRow.h"
#include "ColorScheme.h"
#include "Icons.h"
#include "MessageBar.h"
#include "Motion.h"
#include "Revealer.h"
#include "SummaryBanner.h"
#include "checks/AllChecks.h"
#include "core/AppInfo.h"

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
// A 1 px line in Breeze's separator colour: the window colour with a fifth of the text colour. On
// KDE it's the line Breeze draws. Fusion's is two lines, a dark and a light one.
class Separator : public QWidget
{
public:
    explicit Separator(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(u"separator"_s);
        setFixedHeight(1);
    }

    static QColor colour(const QPalette &palette)
    {
        const QColor window = palette.color(QPalette::Window);
        const QColor text = palette.color(QPalette::WindowText);
        constexpr float textShare = 0.2F;
        return QColor::fromRgbF(window.redF() + (text.redF() - window.redF()) * textShare,
                                window.greenF() + (text.greenF() - window.greenF()) * textShare,
                                window.blueF() + (text.blueF() - window.blueF()) * textShare);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter(this).fillRect(rect(), colour(palette()));
    }
};

QWidget *separator(QWidget *parent)
{
    return new Separator(parent);
}

// Qt picks a link's colour when it parses the text, so after a switch between light and dark the
// links would keep the old one. Parse it again. setText() skips text it already has.
class AboutBox : public QMessageBox
{
public:
    using QMessageBox::QMessageBox;

protected:
    void changeEvent(QEvent *event) override
    {
        if (event->type() == QEvent::PaletteChange && !text().isEmpty()) {
            const QString current = text();
            setText(QString());
            setText(current);
        }
        QMessageBox::changeEvent(event);
    }
};
} // namespace

MainWindow::MainWindow(const HostAccess &host, QWidget *parent)
    : QWidget(parent)
    , m_runner(new CheckRunner(host, createAllChecks(), this))
    , m_banner(new SummaryBanner(this))
    , m_message(new MessageBar)
    , m_messageBox(new QWidget)
    , m_messageReveal(new Revealer(m_messageBox, this))
    , m_runFull(m_message->button())
    , m_copy(new QPushButton(u"Copy Report"_s, this))
    , m_about(new QPushButton(u"About"_s, this))
    , m_list(new QWidget)
    , m_footer(new QWidget(this))
    , m_scroll(new QScrollArea(this))
    , m_copiedReset(new QTimer(this))
{
    updateColours();
    syncIconThemes();
    updateIcons();
    setWindowTitle(u"EACheck"_s);
    resize(560, 640);

    // Rows, in the article's order. The list is a content view: it uses the View colours (Base)
    // and runs to the window's edges, like Dolphin's. The banner and footer keep the Window colours.
    m_list->setBackgroundRole(QPalette::Base);
    m_list->setAutoFillBackground(true);
    auto *listLayout = new QVBoxLayout(m_list);
    listLayout->setSpacing(0);
    for (const auto &check : m_runner->checks()) {
        auto *row = new CheckRow(check->title(), m_list);
        m_rows.append(row);
        connect(row, &CheckRow::heightChanged, this, &MainWindow::layOutList);
        listLayout->addWidget(row);
    }
    listLayout->addStretch(1);

    m_scroll->setWidget(m_list);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    updateScrollBar();

    m_runFull->setToolTip(u"Reads the TPM event log as root. You'll be asked for an admin password once."_s);
    auto *messageLayout = new QVBoxLayout(m_messageBox);
    messageLayout->addWidget(m_message);
    connect(m_runFull, &QPushButton::clicked, this, [this] {
        runChecks(true);
    });
    // No Refresh button: what the checks read rarely changes without a reboot. F5 runs them again,
    // with admin access once it's been granted.
    // Not QKeySequence::Refresh: that keeps only the platform's first binding, Ctrl+R on GNOME.
    new QShortcut(Qt::Key_F5, this, this, [this] {
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
        Motion::crossFade(m_copy, [this] {
            m_copy->setText(u"Copy Report"_s);
        });
    });

    new QShortcut(QKeySequence::Quit, this, this, &QWidget::close);

    connect(m_about, &QPushButton::clicked, this, &MainWindow::showAbout);

    auto *buttons = new QHBoxLayout(m_footer);
    buttons->addWidget(m_about);
    buttons->addStretch(1);
    buttons->addWidget(m_copy);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->addWidget(m_banner);
    layout->addWidget(m_messageReveal);
    layout->addWidget(separator(this));
    layout->addWidget(m_scroll, 1);
    layout->addWidget(separator(this));
    layout->addWidget(m_footer);
    updateMargins();

    // Start with the list focused, so About doesn't show a focus frame.
    m_scroll->setFocus();

    connect(m_runner, &CheckRunner::started, this, &MainWindow::onStarted);
    connect(m_runner, &CheckRunner::finished, this, &MainWindow::onFinished);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        updateColours();
    }
    if (affectsIcons(event)) {
        syncIconThemes();
        updateIcons();
        updateScrollBar();
    }
    if (event->type() == QEvent::StyleChange) {
        updateMargins();
    }
    QWidget::changeEvent(event);
}

void MainWindow::updateColours()
{
    const QPalette colours = windowColours(QGuiApplication::palette());
    // Setting them sends another PaletteChange, which then finds them already set.
    if (colours.resolveMask() == palette().resolveMask() && colours.resolve(palette()) == palette()) {
        return;
    }
    setPalette(colours);
}

void MainWindow::layOutList()
{
    // Before the next paint, so no frame shows the rows at their old positions. The scroll area
    // resizes the list when it gets a LayoutRequest.
    m_list->layout()->activate();
    QEvent request(QEvent::LayoutRequest);
    QCoreApplication::sendEvent(m_scroll, &request);
}

void MainWindow::updateIcons()
{
    m_runFull->setIcon(themeIcon({u"dialog-password"_s, u"security-high"_s}));
    m_copy->setIcon(themeIcon({u"edit-copy"_s}));
    m_about->setIcon(themeIcon({u"help-about"_s}));
}

void MainWindow::updateMargins()
{
    // The window has no margins of its own, so the list can reach its edges. The banner and the
    // buttons get the window's margins instead. The list's are what's left after its rows' own,
    // so the rows' arrows line up with the banner's icon.
    const int window = style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, this);
    const int row = style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, m_rows.constFirst());
    const int inset = std::max(0, window - row);
    m_banner->layout()->setContentsMargins(window, window, window, window);
    m_footer->layout()->setContentsMargins(window, window, window, window);
    m_messageBox->layout()->setContentsMargins(window, 0, window, window);
    m_list->layout()->setContentsMargins(inset, inset, inset, inset);
}

void MainWindow::updateScrollBar()
{
    // Fusion's scroll bar has arrows and, with GNOME's light colours, a handle lighter than its
    // groove. Under Fusion, draw a thin handle like GNOME's own apps. Other styles keep theirs.
    QScrollBar *bar = m_scroll->verticalScrollBar();
    if (style()->name().compare(u"fusion"_s, Qt::CaseInsensitive) != 0) {
        bar->setStyleSheet({});
        return;
    }
    const auto handle = [this](qreal opacity) {
        QColor colour = palette().color(QPalette::WindowText);
        colour.setAlphaF(opacity);
        return colour.name(QColor::HexArgb);
    };
    bar->setStyleSheet(uR"(
        QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }
        QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 32px; margin: 2px; }
        QScrollBar::handle:vertical:hover { background: %2; }
        QScrollBar::handle:vertical:pressed { background: %3; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    )"_s.arg(handle(0.2), handle(0.4), handle(0.6)));
}

void MainWindow::runChecks(bool elevate)
{
    // F5 works during a run too, now that no disabled button stands in its way.
    if (m_runner->isRunning()) {
        return;
    }
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
    m_copy->setEnabled(false);
}

void MainWindow::onFinished(const Report &report)
{
    m_report = report;
    m_banner->setReport(report);
    for (qsizetype i = 0; i < m_rows.size() && i < report.results.size(); ++i) {
        m_rows[i]->setResult(report.results[i]);
    }

    updateMessage(report);
    m_copy->setEnabled(true);
}

void MainWindow::updateMessage(const Report &report)
{
    const auto needsAdmin = std::count_if(report.results.begin(), report.results.end(), [](const CheckResult &r) {
        return r.status == CheckStatus::NeedsAdmin;
    });
    const QString checks = needsAdmin == 1 ? u"1 check"_s : u"%1 checks"_s.arg(needsAdmin);
    bool show = true;
    switch (report.privilegedState) {
    case PrivilegedState::NotRequested:
        show = needsAdmin > 0;
        m_message->setMessage(MessageBar::Type::Information,
                              u"%1 need%2 admin access to read the TPM event log."_s.arg(checks, needsAdmin == 1 ? u"s"_s : QString()),
                              u"Run Full Check"_s);
        break;
    case PrivilegedState::Cancelled:
        m_message->setMessage(MessageBar::Type::Warning, u"Admin access wasn't granted, so %1 couldn't finish."_s.arg(checks), u"Try Again"_s);
        break;
    case PrivilegedState::NoAgent:
        m_message->setMessage(MessageBar::Type::Warning,
                              u"No polkit authentication agent is running, so admin access can't be asked for."_s,
                              u"Try Again"_s);
        break;
    case PrivilegedState::Failed:
        m_message->setMessage(MessageBar::Type::Error, u"The admin read failed: %1"_s.arg(report.privilegedError), u"Try Again"_s);
        break;
    case PrivilegedState::Granted:
        show = false;
        break;
    }
    m_runFull->setEnabled(show);
    m_messageReveal->setRevealed(show, true);
}

void MainWindow::copyReport(bool json)
{
    const QString text = json ? QString::fromUtf8(QJsonDocument(m_report.toJson()).toJson(QJsonDocument::Indented)) : m_report.toText();
    QGuiApplication::clipboard()->setText(text);
    Motion::crossFade(m_copy, [this] {
        m_copy->setText(u"Copied"_s);
    });
    m_copiedReset->start();
}

void MainWindow::showAbout()
{
    // Not QMessageBox::about(): that blocks in its own event loop, so the window couldn't be tested.
    auto *box = new AboutBox(this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setAttribute(Qt::WA_WindowPropagation); // the window's colours
    box->setWindowTitle(u"About %1"_s.arg(AppInfo::displayName()));
    box->setIconPixmap(appIcon().pixmap(QSize(64, 64), devicePixelRatio()));
    box->setTextFormat(Qt::RichText);
    box->setText(u"<h3>%1 %2</h3>"
                 "<p>%3.</p>"
                 "<p>Follows <a href=\"%4\">Epic's published requirements</a> as of %5. EAC performs its own verification at game launch.</p>"
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
