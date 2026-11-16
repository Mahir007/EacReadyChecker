// SPDX-License-Identifier: GPL-3.0-or-later
#include "SummaryBanner.h"

#include "IconWidget.h"
#include "Icons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
constexpr int kIconSize = 48;
}

SummaryBanner::SummaryBanner(QWidget *parent)
    : QWidget(parent)
    , m_icon(new IconWidget(this))
    , m_title(new QLabel(this))
    , m_subtitle(new QLabel(this))
{
    m_icon->setFixedSize(kIconSize, kIconSize);

    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.5);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);
    m_subtitle->setWordWrap(true);
    m_subtitle->setForegroundRole(QPalette::PlaceholderText);

    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(m_title);
    text->addWidget(m_subtitle);

    auto *layout = new QHBoxLayout(this);
    layout->addWidget(m_icon, 0, Qt::AlignTop);
    layout->addLayout(text, 1);
}

void SummaryBanner::setRunning(bool elevated)
{
    m_verdict.reset();
    updateIcon();
    m_title->setText(u"Checking…"_s);
    m_subtitle->setText(elevated ? u"Waiting for admin access"_s : QString());
}

void SummaryBanner::setReport(const Report &report)
{
    const Verdict verdict = report.verdict();
    m_verdict = verdict;
    updateIcon(IconChange::Fade);
    m_title->setText(verdictLabel(verdict));

    const auto met = std::count_if(report.results.begin(), report.results.end(), [](const CheckResult &r) {
        return r.status == CheckStatus::Pass || r.status == CheckStatus::NotRequired;
    });
    QString subtitle = u"%1 of %2 requirements met"_s.arg(met).arg(report.results.size());
    const QStringList problems = report.accessProblems();
    for (const QString &problem : problems) {
        subtitle += u" · "_s + problem;
    }
    m_subtitle->setText(subtitle);
}

void SummaryBanner::changeEvent(QEvent *event)
{
    if (affectsIcons(event)) {
        syncIconThemes();
        updateIcon();
    }
    QWidget::changeEvent(event);
}

void SummaryBanner::updateIcon(IconChange change)
{
    const QIcon icon = m_verdict ? verdictIcon(*m_verdict) : themeIcon({u"view-refresh"_s});
    if (change == IconChange::Fade) {
        m_icon->fadeTo(icon);
    } else {
        m_icon->setIcon(icon);
    }
}
