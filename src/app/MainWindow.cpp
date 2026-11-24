// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "CheckRow.h"
#include "ColorScheme.h"
#include "Desktop.h"
#include "Icons.h"
#include "MessageBar.h"
#include "Motion.h"
#include "Revealer.h"
#include "SummaryBanner.h"
#include "TouchScroll.h"
#include "checks/AllChecks.h"
#include "core/AppInfo.h"

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
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
    void paintEvent(QPaintEvent *) override { QPainter(this).fillRect(rect(), colour(palette())); }
};

QWidget *separator(QWidget *parent)
{
    return new Separator(parent);
}

// Qt colours links from the app's palette when it parses the text. So they'd miss the window's own
// colours, which have the accent in light, and keep the old colour after a
// switch between light and dark. The links get the box's own colour instead, set again when its
// palette changes. setText() skips text it already has.
class AboutBox : public QMessageBox
{
public:
    using QMessageBox::QMessageBox;

    void setLinkedText(const QString &html)
    {
        m_html = html;
        updateText();
    }

protected:
    void changeEvent(QEvent *event) override
    {
        if (event->type() == QEvent::PaletteChange) {
            updateText();
        }
        QMessageBox::changeEvent(event);
    }

private:
    void updateText()
    {
        QString html = m_html;
        setText(html.replace(u"<a href="_s, u"<a style=\"color: %1\" href="_s.arg(palette().color(QPalette::Link).name())));
    }

    QString m_html;
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
    // First: setting the colours can already update the icons, its actions' too.
    createMenuBar();
    updateColours();
    syncIconThemes();
    updateIcons();
    setWindowTitle(u"EACheck"_s);
    resize(460, 640);

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
    enableTouchScrolling(m_scroll);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *old, QWidget *now) {
        if ((old == m_scroll || now == m_scroll) && isFusionOnPlasma(this)) {
            updateScrollBar();
        }
    });

    m_runFull->setToolTip(u"Reads the TPM event log as root. You'll be asked for an admin password once."_s);
    auto *messageLayout = new QVBoxLayout(m_messageBox);
    messageLayout->addWidget(m_message);
    connect(m_runFull, &QPushButton::clicked, this, [this] {
        runChecks(true);
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

    connect(m_about, &QPushButton::clicked, this, &MainWindow::showAbout);

    auto *buttons = new QHBoxLayout(m_footer);
    buttons->addWidget(m_about);
    buttons->addStretch(1);
    buttons->addWidget(m_copy);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->setMenuBar(m_menuBar);
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

void MainWindow::createMenuBar()
{
    // For a global menu: Plasma's Global Menu widget or the title bar's application menu button.
    // Qt exports the bar over D-Bus when a menu registrar is running, and then hides it. Without
    // one, it stays hidden too: the footer and the message have the same actions.
    m_menuBar = new QMenuBar(this);
    m_menuBar->hide();

    QMenu *file = m_menuBar->addMenu(u"&File"_s);
    // No Refresh button: what the checks read rarely changes without a reboot. F5 runs them again,
    // with admin access once it's been granted.
    m_runAgainAction = file->addAction(u"Run &Again"_s, this, [this] {
        runChecks(m_report.privilegedState == PrivilegedState::Granted);
    });
    // Not QKeySequence::Refresh: that keeps only the platform's first binding, Ctrl+R on GNOME.
    m_runAgainAction->setShortcut(Qt::Key_F5);
    m_runFullAction = file->addAction(u"Run &Full Check…"_s, this, [this] {
        runChecks(true);
    });
    m_runFullAction->setEnabled(false);
    file->addSeparator();
    m_quitAction = file->addAction(u"&Quit"_s, this, &QWidget::close);
    m_quitAction->setShortcut(QKeySequence::Quit);

    QMenu *edit = m_menuBar->addMenu(u"&Edit"_s);
    m_copyTextAction = edit->addAction(u"Copy Report as &Text"_s, this, [this] {
        copyReport(false);
    });
    m_copyJsonAction = edit->addAction(u"Copy Report as &JSON"_s, this, [this] {
        copyReport(true);
    });

    QMenu *help = m_menuBar->addMenu(u"&Help"_s);
    m_aboutAction = help->addAction(u"&About %1"_s.arg(AppInfo::displayName()), this, &MainWindow::showAbout);

    // A hidden menu bar's shortcuts don't fire. The window's do.
    addActions({m_runAgainAction, m_quitAction});
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
    m_runAgainAction->setIcon(themeIcon({u"view-refresh"_s}));
    m_runFullAction->setIcon(m_runFull->icon());
    m_quitAction->setIcon(themeIcon({u"application-exit"_s}));
    m_copyTextAction->setIcon(m_copy->icon());
    m_copyJsonAction->setIcon(m_copy->icon());
    m_aboutAction->setIcon(m_about->icon());
}

void MainWindow::updateMargins()
{
    // The window has no margins of its own, so the list can reach its edges. The banner and the
    // buttons get the window's margins instead. The list's are what's left after its rows' own,
    // so the rows' arrows line up with the banner's icon.
    const int window = layoutMargin(this);
    const int row = layoutMargin(m_rows.constFirst());
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
    const QString sheet = uR"(
        QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }
        QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 32px; margin: 2px; }
        QScrollBar::handle:vertical:hover { background: %2; }
        QScrollBar::handle:vertical:pressed { background: %3; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    )"_s;
    if (isFusionOnPlasma(this)) {
        // Breeze's colours: 35% of the highlight while the list has focus, otherwise 17.5% of the
        // text colour, and 50% of the highlight under the pointer.
        const auto highlight = [this](qreal opacity) {
            QColor colour = palette().color(QPalette::Active, QPalette::Highlight);
            colour.setAlphaF(opacity);
            return colour.name(QColor::HexArgb);
        };
        const QString idle = m_scroll->hasFocus() ? highlight(0.35) : handle(0.175);
        bar->setStyleSheet(sheet.arg(idle, highlight(0.5), highlight(0.5)));
    } else {
        bar->setStyleSheet(sheet.arg(handle(0.2), handle(0.4), handle(0.6)));
    }
}

void MainWindow::runChecks(bool elevate)
{
    // Run Again and Run Full Check are disabled during a run, but --elevate and tests call this too.
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
    for (QAction *action : {m_runAgainAction, m_runFullAction, m_copyTextAction, m_copyJsonAction}) {
        action->setEnabled(false);
    }
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
    for (QAction *action : {m_runAgainAction, m_copyTextAction, m_copyJsonAction}) {
        action->setEnabled(true);
    }
}

void MainWindow::updateMessage(const Report &report)
{
    // Short: the rows already show which checks need it.
    bool show = true;
    switch (report.privilegedState) {
    case PrivilegedState::NotRequested:
        show = std::any_of(report.results.begin(), report.results.end(), [](const CheckResult &r) {
            return r.status == CheckStatus::NeedsAdmin;
        });
        m_message->setMessage(MessageBar::Type::Information, u"Some checks need admin access."_s, u"Run Full Check"_s);
        break;
    case PrivilegedState::Cancelled:
        m_message->setMessage(MessageBar::Type::Warning, u"Admin access wasn't granted."_s, u"Try Again"_s);
        break;
    case PrivilegedState::NoAgent:
        m_message->setMessage(MessageBar::Type::Warning, u"No polkit authentication agent is running."_s, u"Try Again"_s);
        break;
    case PrivilegedState::Failed:
        m_message->setMessage(MessageBar::Type::Error, u"Admin read failed: %1"_s.arg(report.privilegedError), u"Try Again"_s);
        break;
    case PrivilegedState::Granted:
        show = false;
        break;
    }
    m_runFull->setEnabled(show);
    m_runFullAction->setEnabled(show);
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
    box->setLinkedText(u"<h3>%1 %2</h3>"
                       "<p>%3.</p>"
                       "<p>Follows <a href=\"%4\">Epic's published requirements</a> as of %5.<br>EAC performs its own verification at game launch.</p>"
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
