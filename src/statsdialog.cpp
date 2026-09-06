#include "statsdialog.h"
#include "focusmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QFont>
#include <DPushButton>

DWIDGET_USE_NAMESPACE

// ---------- BarsWidget ----------
BarsWidget::BarsWidget(FocusManager *mgr, QWidget *parent)
    : QWidget(parent)
    , m_mgr(mgr)
{
    refresh();
}

void BarsWidget::refresh()
{
    m_days.clear();
    const auto days = m_mgr ? m_mgr->lastSevenDays() : QList<FocusManager::DayStat>();
    for (const auto &d : days)
        m_days.append({ d.label, d.seconds / 60 });
    update();
}

void BarsWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor textCol = palette().color(QPalette::WindowText);
    const QColor faint = textCol;
    QFont small = font();
    small.setPixelSize(11);
    QFont tiny = small;
    tiny.setPixelSize(9);

    const int n = m_days.size();
    if (n == 0)
        return;
    const int topPad = 18;
    const int bottomPad = 20;
    const int areaH = height() - topPad - bottomPad;
    const int slotW = width() / n;
    const int barW = qMin(34, slotW - 18);
    int maxMin = 1;
    for (const auto &d : m_days)
        maxMin = qMax(maxMin, d.second);
    maxMin = qMax(maxMin, 25); // 至少一格

    for (int i = 0; i < n; ++i) {
        const int cx = slotW * i + slotW / 2;
        const int h = (int)((qreal)m_days[i].second / maxMin * (areaH - 8));
        const bool today = (i == n - 1);
        QColor barCol = today ? QColor("#4cc96f") : QColor("#8fcaa3");
        barCol.setAlpha(today ? 235 : 150);
        p.setPen(Qt::NoPen);
        p.setBrush(barCol);
        const QRect barRect(cx - barW / 2, topPad + areaH - h, barW, h);
        p.drawRoundedRect(barRect, 4, 4);

        // 分钟数
        p.setPen(textCol);
        p.setFont(tiny);
        const QString val = (m_days[i].second > 0) ? QString::number(m_days[i].second) + QStringLiteral("m") : QString();
        p.drawText(QRect(cx - barW / 2 - 6, barRect.y() - 15, barW + 12, 14),
                   Qt::AlignCenter, val);
        // 日期
        p.setPen(today ? textCol : faint);
        p.setFont(small);
        p.drawText(QRect(cx - slotW / 2, topPad + areaH + 4, slotW, 16),
                   Qt::AlignCenter, m_days[i].first);
    }
}

// ---------- StatsDialog ----------
StatsDialog::StatsDialog(FocusManager *mgr, QWidget *parent)
    : QDialog(parent)
    , m_mgr(mgr)
{
    setWindowTitle(QStringLiteral("专注助手 · 成长记录"));
    setFixedWidth(470);
    setMinimumHeight(340);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 16);
    root->setSpacing(10);

    m_summary = new QWidget(this);
    auto *grid = new QGridLayout(m_summary);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(28);
    auto makeItem = [&](const QString &title) {
        auto *box = new QWidget(m_summary);
        auto *v = new QVBoxLayout(box);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        auto *cap = new QLabel(title, box);
        QFont cf = font(); cf.setPixelSize(11);
        cap->setFont(cf);
        cap->setStyleSheet("color: rgba(127,127,127,0.95);");
        auto *val = new QLabel(QStringLiteral("--"), box);
        QFont vf = font(); vf.setPixelSize(20); vf.setBold(true);
        val->setFont(vf);
        val->setObjectName("statVal");
        v->addWidget(val);
        v->addWidget(cap);
        return val;
    };
    auto *vToday = makeItem(QStringLiteral("今日专注"));
    auto *vSessions = makeItem(QStringLiteral("完成次数"));
    auto *vTotal = makeItem(QStringLiteral("累计专注"));
    auto *vStage = makeItem(QStringLiteral("成长阶段"));
    grid->addWidget(vToday, 0, 0);
    grid->addWidget(vSessions, 0, 1);
    grid->addWidget(vTotal, 0, 2);
    grid->addWidget(vStage, 0, 3);
    root->addWidget(m_summary);

    m_bars = new BarsWidget(m_mgr, this);
    auto *cap2 = new QLabel(QStringLiteral("最近 7 天"), this);
    QFont cf2 = font(); cf2.setPixelSize(11);
    cap2->setFont(cf2);
    cap2->setStyleSheet("color: rgba(127,127,127,0.95);");
    root->addWidget(cap2);
    root->addWidget(m_bars, 1);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto *closeBtn = new DPushButton(QStringLiteral("关闭"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    // 用 objectName 缓存引用便于刷新
    vToday->setObjectName(QStringLiteral("valToday"));
    vSessions->setObjectName(QStringLiteral("valSessions"));
    vTotal->setObjectName(QStringLiteral("valTotal"));
    vStage->setObjectName(QStringLiteral("valStage"));
}

void StatsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!m_mgr)
        return;
    auto setVal = [&](const char *name, const QString &text) {
        auto *lab = m_summary->findChild<QLabel *>(QString::fromLatin1(name));
        if (lab)
            lab->setText(text);
    };
    const int todayMin = m_mgr->todayFocusSeconds() / 60;
    const int totalMin = (int)(m_mgr->totalFocusSeconds() / 60);
    setVal("valToday", QStringLiteral("%1 分钟").arg(todayMin));
    setVal("valSessions", QStringLiteral("%1 次").arg(m_mgr->completedSessions()));
    setVal("valTotal", QStringLiteral("%1 分钟").arg(totalMin));
    setVal("valStage", m_mgr->stageName());
    m_bars->refresh();
}
