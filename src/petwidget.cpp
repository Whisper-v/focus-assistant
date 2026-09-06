#include "petwidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QFont>
#include <QRandomGenerator>
#include <QtMath>

PetWidget::PetWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(300, 250);
    setAttribute(Qt::WA_TranslucentBackground);
    applySkin();
    m_animTimer.setInterval(90);
    connect(&m_animTimer, &QTimer::timeout, this, &PetWidget::onAnimTick);
    m_blinkTimer.setInterval(2400);
    connect(&m_blinkTimer, &QTimer::timeout, this, &PetWidget::onBlinkTimeout);
    m_blinkTimer.start();
}

QString PetWidget::skinKey(Skin s)
{
    switch (s) {
    case Skin::Classic:   return QStringLiteral("classic");
    case Skin::Sunflower: return QStringLiteral("sunflower");
    }
    return QStringLiteral("classic");
}

PetWidget::Skin PetWidget::skinFromKey(const QString &key)
{
    if (key == QStringLiteral("sunflower"))
        return Skin::Sunflower;
    return Skin::Classic;
}

QString PetWidget::skinName(Skin s)
{
    switch (s) {
    case Skin::Classic:   return QStringLiteral("经典粉花 · 露露");
    case Skin::Sunflower: return QStringLiteral("阳光向日葵");
    }
    return QStringLiteral("经典粉花 · 露露");
}

void PetWidget::setSkin(Skin skin)
{
    if (skin == m_skin)
        return;
    m_skin = skin;
    applySkin();
    update();
}

void PetWidget::applySkin()
{
    switch (m_skin) {
    case Skin::Sunflower:
        m_petal      = QColor("#ffb300");              // 阳光金
        m_petalSoft  = QColor("#ffc94d");
        m_petal2     = QColor(255, 213, 79, 200);      // 内层亮金
        m_disc       = QColor("#8a5a2b");              // 棕色大花盘
        m_discBorder = QColor("#5b3a1e");
        m_faceInk    = QColor("#3e2723");
        m_stem       = QColor("#558b2f");
        m_leafA      = QColor("#9ccc65");
        m_leafB      = QColor("#7cb342");
        m_headCol0   = QColor("#c5e1a5");
        m_headCol1   = QColor("#aed581");
        m_headBorder = QColor("#558b2f");
        break;
    case Skin::Classic:
    default:
        m_petal      = QColor("#ff8fae");
        m_petalSoft  = QColor("#ffa3bc");
        m_petal2     = QColor(255, 175, 198, 200);
        m_disc       = QColor("#ffe08a");
        m_discBorder = QColor("#e0a62e");
        m_faceInk    = QColor("#5d4037");
        m_stem       = QColor("#7cb342");
        m_leafA      = QColor("#aed581");
        m_leafB      = QColor("#8bc34a");
        m_headCol0   = QColor("#b7e39c");
        m_headCol1   = QColor("#a5dc7d");
        m_headBorder = QColor("#689f38");
        break;
    }
}

void PetWidget::setStage(int stage)
{
    stage = qBound(0, stage, 3);
    if (stage == m_stage)
        return;
    m_stage = stage;
    update();
}

void PetWidget::setMood(Mood mood)
{
    if (mood == m_mood)
        return;
    m_mood = mood;
    if (mood == Focused || mood == Sleepy) {
        m_animTimer.start();
    } else if (!m_celebrating && m_particles.isEmpty()) {
        m_animTimer.stop();
    }
    update();
}

void PetWidget::setProgress(qreal p)
{
    p = qBound<qreal>(0.0, p, 1.0);
    if (qFuzzyCompare(p, m_progress))
        return;
    m_progress = p;
    update();
}

void PetWidget::startCelebration()
{
    m_celebrating = true;
    m_celebTicks = 0;
    m_animTimer.start();
    for (int i = 0; i < 8; ++i)
        spawnCelebrationParticle();
    update();
}

void PetWidget::scheduleBlink()
{
    m_blinkTimer.start(1800 + QRandomGenerator::global()->bounded(3400));
}

void PetWidget::onBlinkTimeout()
{
    if (m_mood == Idle || m_mood == Happy) {
        m_blinking = true;
        update();
        QTimer::singleShot(140, this, [this] {
            m_blinking = false;
            update();
            scheduleBlink();
        });
    } else {
        scheduleBlink();
    }
}

void PetWidget::onAnimTick()
{
    if (m_celebrating && m_celebTicks < 40) {
        ++m_celebTicks;
        spawnCelebrationParticle();
        spawnCelebrationParticle();
        if (m_celebTicks >= 40)
            m_celebrating = false;
    }
    if (m_mood == Focused || m_mood == Sleepy || m_celebrating) {
        m_breathPhase += 0.075;
        if (m_breathPhase > 1.0)
            m_breathPhase -= 1.0;
    }
    for (int i = m_particles.size() - 1; i >= 0; --i) {
        Particle &pt = m_particles[i];
        pt.age += 0.09;
        pt.pos += pt.vel * 0.09;
        pt.vel.setY(pt.vel.y() + 26 * 0.09);
        if (pt.age >= pt.life)
            m_particles.removeAt(i);
    }
    if (!m_celebrating && m_particles.isEmpty()
        && m_mood != Focused && m_mood != Sleepy) {
        m_animTimer.stop();
    }
    update();
}

void PetWidget::spawnCelebrationParticle()
{
    static const QColor palette[] = {
        QColor("#ffd54f"), QColor("#ff8a80"), QColor("#ffab91"),
        QColor("#80deea"), QColor("#c5e1a5"), QColor("#f8bbd0")
    };
    Particle pt;
    QRandomGenerator *rg = QRandomGenerator::global();
    pt.pos = QPointF(150, 100) + QPointF(rg->bounded(-30, 31), rg->bounded(-20, 41));
    const qreal ang = rg->generateDouble() * 2 * M_PI;
    const qreal spd = 20 + rg->generateDouble() * 55;
    pt.vel = QPointF(qCos(ang) * spd, qSin(ang) * spd - 34);
    pt.life = 0.6 + rg->generateDouble() * 0.9;
    pt.size = 3.0 + rg->generateDouble() * 4.5;
    pt.color = palette[rg->bounded(6)];
    pt.kind = (rg->bounded(4) == 0) ? 1 : 0;
    m_particles.append(pt);
}

void PetWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    drawRing(p);
    QPointF headPos;
    qreal headR = 20;
    drawPlant(p, headPos, headR);

    // 情绪叠加：泪珠 / 困倦 zZz
    if (m_mood == Focused) {
        QPen pen(QColor("#63b6ff"), 2.4, Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        p.setBrush(QColor("#a8d8ff"));
        const QPointF drop(headPos.x() + headR * 0.85, headPos.y() - headR * 0.9);
        p.drawEllipse(drop, 3.2, 4.6);
        p.drawLine(drop + QPointF(0, -6), drop + QPointF(0, -1));
    }
    if (m_mood == Tired || m_mood == Sleepy) {
        QFont f = font();
        f.setPixelSize(15);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor(120, 120, 120, 190));
        QPointF base(headPos.x() + headR * 0.85, headPos.y() - headR * 0.85);
        p.drawText(QRectF(base.x(), base.y() - 16, 40, 16), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("z"));
        QFont fs = f;
        fs.setPixelSize(10);
        p.setFont(fs);
        p.setPen(QColor(120, 120, 120, 130));
        p.drawText(QRectF(base.x() + 8, base.y() - 26, 30, 12), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("Z"));
    }

    drawParticles(p);
}

void PetWidget::drawRing(QPainter &p)
{
    const QPointF c(150, 126);
    const qreal R = 94;
    const qreal th = 7;
    QRectF rr(c.x() - R, c.y() - R, R * 2, R * 2);

    QColor accent("#28c76f");
    if (m_mood == Sleepy || m_mood == Tired)
        accent = QColor("#4fa3ff");
    else if (m_mood == Celebrate || m_mood == Happy)
        accent = QColor("#ffb020");
    else if (m_mood == Focused)
        accent = QColor("#28c76f");

    QPen track(QColor(128, 128, 128, 42), th);
    track.setCapStyle(Qt::RoundCap);
    p.setPen(track);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(rr);

    if (m_progress > 0.001) {
        QPen prog(accent, th, Qt::SolidLine, Qt::RoundCap);
        p.setPen(prog);
        const int span = -qRound(m_progress * 360.0) * 16;
        p.drawArc(rr, 90 * 16, span);
    }
}

void PetWidget::drawPot(QPainter &p)
{
    // 土壤
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#8d6e63"));
    p.drawEllipse(QPointF(150, 187), 45, 9);
    p.setBrush(QColor("#a1887f"));
    p.drawEllipse(QPointF(150, 185), 45, 8);

    // 花盆主体(梯形)
    QPainterPath pot;
    pot.moveTo(106, 191);
    pot.lineTo(194, 191);
    pot.lineTo(183, 231);
    pot.quadTo(150, 238, 117, 231);
    pot.closeSubpath();
    QLinearGradient lg(0, 190, 0, 238);
    lg.setColorAt(0.0, QColor("#e08d78"));
    lg.setColorAt(1.0, QColor("#b8604a"));
    p.setBrush(lg);
    p.setPen(Qt::NoPen);
    p.drawPath(pot);

    // 盆沿
    QPainterPath rim;
    rim.addRoundedRect(QRectF(102, 186, 96, 13), 6, 6);
    p.setBrush(QColor("#e99a86"));
    p.drawPath(rim);
}

void PetWidget::drawLeaves(QPainter &p, const QPointF &base, qreal size)
{
    QPen stemPen(m_stem, 2.2, Qt::SolidLine, Qt::RoundCap);
    p.setPen(stemPen);
    // 左叶
    QPainterPath l;
    l.moveTo(base);
    l.quadTo(base + QPointF(-size * 0.7, -size * 0.55), base + QPointF(-size * 1.35, -size * 0.15));
    p.drawPath(l);
    // 右叶
    QPainterPath r;
    r.moveTo(base);
    r.quadTo(base + QPointF(size * 0.7, -size * 0.55), base + QPointF(size * 1.35, -size * 0.15));
    p.drawPath(r);

    p.setPen(Qt::NoPen);
    p.setBrush(m_leafA);
    p.drawEllipse(base + QPointF(-size * 1.05, -size * 0.12), size * 0.62, size * 0.30);
    p.drawEllipse(base + QPointF(size * 1.05, -size * 0.12), size * 0.62, size * 0.30);
    p.setBrush(m_leafB);
    p.drawEllipse(base + QPointF(-size * 1.05, -size * 0.12), size * 0.36, size * 0.20);
    p.drawEllipse(base + QPointF(size * 1.05, -size * 0.12), size * 0.36, size * 0.20);
}

void PetWidget::drawPlant(QPainter &p, QPointF &headPos, qreal &headR)
{
    drawPot(p);

    const qreal bob = qSin(2 * M_PI * m_breathPhase) * (m_stage >= 1 ? 2.2 : 1.6);
    p.save();
    p.translate(0, bob);

    // ---------- 向日葵皮肤（PVZ 风格：大花盘 + 双层花瓣 + 招牌表情） ----------
    if (m_skin == Skin::Sunflower) {
        drawSunflowerPlant(p, headPos, headR);
        p.restore();
        return;
    }

    // 主茎(随阶段变高)
    QPointF stemTop;
    qreal stemW = 6;
    switch (m_stage) {
    case 0:
        stemTop = QPointF(150, 152);
        break;
    case 1:
        stemTop = QPointF(150, 126);
        stemW = 7;
        break;
    default:
        stemTop = QPointF(150, 122);
        stemW = 8;
        break;
    }
    QPainterPath stem;
    stem.moveTo(150, 186);
    stem.quadTo(150, (186 + stemTop.y()) / 2.0 + 2, stemTop.x(), stemTop.y());
    QPen stemPen(m_stem, stemW, Qt::SolidLine, Qt::RoundCap);
    p.setPen(stemPen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(stem);

    if (m_stage == 0) {
        drawLeaves(p, QPointF(150, 168), 9);
        headPos = QPointF(150, 140);
        headR = 17.0;
    } else if (m_stage == 1) {
        drawLeaves(p, QPointF(150, 150), 14);
        headPos = QPointF(150, 112);
        headR = 22.0;
    } else {
        drawLeaves(p, QPointF(150, 148), 15);
        headPos = QPointF(150, 104);
        headR = m_stage == 2 ? 23.0 : 26.0;

        // 光晕(阶段3)
        if (m_stage == 3) {
            QRadialGradient glow(headPos, headR * 2.6);
            glow.setColorAt(0.0, QColor(255, 235, 140, 90));
            glow.setColorAt(1.0, QColor(255, 235, 140, 0));
            p.setBrush(glow);
            p.setPen(Qt::NoPen);
            p.drawEllipse(headPos, headR * 2.6, headR * 2.6);
        }
        // 花瓣
        const QColor petal = (m_stage == 3) ? m_petal : m_petalSoft;
        for (int k = 0; k < 6; ++k) {
            p.save();
            p.translate(headPos);
            p.rotate(k * 60.0);
            p.setPen(Qt::NoPen);
            p.setBrush(petal);
            p.drawEllipse(QPointF(0, -(headR + 7)), headR * 0.62, headR * 0.34);
            p.restore();
        }
        if (m_stage == 3) {
            for (int k = 0; k < 6; ++k) {
                p.save();
                p.translate(headPos);
                p.rotate(k * 60.0 + 30.0);
                p.setPen(Qt::NoPen);
                p.setBrush(m_petal2);
                p.drawEllipse(QPointF(0, -(headR + 16)), headR * 0.4, headR * 0.24);
                p.restore();
            }
        }
        // 花盘
        p.setPen(QPen(m_discBorder, 2));
        p.setBrush(m_disc);
        p.drawEllipse(headPos, headR, headR);
        if (m_skin == Skin::Sunflower) {
            // 花盘上点缀葵花籽点，增加向日葵辨识度
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(m_faceInk.red(), m_faceInk.green(), m_faceInk.blue(), 95));
            const int rings = (m_stage == 3) ? 2 : 1;
            for (int ring = 1; ring <= rings; ++ring) {
                const qreal rr = headR * (ring == 1 ? 0.42 : 0.78);
                const int n = ring == 1 ? 6 : 12;
                const qreal off = (ring == 1) ? 0.0 : M_PI / n;
                for (int i = 0; i < n; ++i) {
                    const qreal a = 2.0 * M_PI * i / n + off;
                    p.drawEllipse(QPointF(headPos.x() + qCos(a) * rr,
                                          headPos.y() + qSin(a) * rr),
                                  headR * 0.055, headR * 0.055);
                }
            }
        }
    }

    // 头(画表情的基底)
    if (m_stage < 2) {
        QColor headCol = (m_stage == 0) ? m_headCol0 : m_headCol1;
        p.setPen(QPen(m_headBorder, 2));
        p.setBrush(headCol);
        p.drawEllipse(headPos, headR, headR);
    }
    drawFace(p, headPos, headR);
    p.restore();
}

// ---------------------------------------------------------------------------
// 向日葵皮肤：整株植物（茎 / 叶 / 头 / 表情）PVZ 风格
// ---------------------------------------------------------------------------
void PetWidget::drawSunflowerPlant(QPainter &p, QPointF &headPos, qreal &headR)
{
    const int st = m_stage;
    QPointF stemTop;
    qreal stemW;
    switch (st) {
    case 0: stemTop = QPointF(150, 152); stemW = 4;  headPos = QPointF(150, 140); headR = 15.0; break;
    case 1: stemTop = QPointF(150, 130); stemW = 5.5; headPos = QPointF(150, 118); headR = 17.0; break;
    case 2: stemTop = QPointF(150, 134); stemW = 6.5; headPos = QPointF(150, 116); headR = 20.0; break;
    default:stemTop = QPointF(150, 128); stemW = 7.5; headPos = QPointF(150, 110); headR = 29.0; break;
    }

    // 主茎
    QPainterPath stem;
    stem.moveTo(150, 186);
    stem.quadTo(150, (186 + stemTop.y()) / 2.0 + 2, stemTop.x(), stemTop.y());
    QPen stemPen(m_stem, stemW, Qt::SolidLine, Qt::RoundCap);
    p.setPen(stemPen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(stem);

    // 叶子（随阶段变大）
    if (st == 0)
        drawLeaves(p, QPointF(150, 168), 9);
    else if (st == 1)
        drawLeaves(p, QPointF(150, 150), 12);
    else if (st == 2)
        drawLeaves(p, QPointF(150, 150), 13);
    else
        drawLeaves(p, QPointF(150, 152), 15);

    if (st < 2) {
        // 种子 / 幼苗：小绿脑袋 + 幼芽小表情
        const QColor col = (st == 0) ? QColor("#c8e6a0") : QColor("#aedc7a");
        QLinearGradient hg(headPos + QPointF(-headR, 0), headPos + QPointF(headR, 0));
        hg.setColorAt(0.0, col.darker(118));
        hg.setColorAt(0.55, col);
        hg.setColorAt(1.0, col.darker(122));
        p.setPen(QPen(QColor("#6da83a"), 1.8));
        p.setBrush(hg);
        p.drawEllipse(headPos, headR, headR);
        // 头顶小嫩叶
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#9ccc65"));
        p.drawEllipse(headPos + QPointF(-headR * 0.72, -headR * 0.35), headR * 0.42, headR * 0.20);
        p.drawEllipse(headPos + QPointF(headR * 0.72, -headR * 0.35), headR * 0.42, headR * 0.20);
        drawFace(p, headPos, headR);
        return;
    }

    // 盛开：双层花瓣 + 棕色大花盘 + PVZ 脸
    drawSunflowerBloom(p, headPos, headR, st);
    drawSunflowerFace(p, headPos, headR);
}

// 画一圈射线状花瓣（向日葵 / 花盘外圈）
void PetWidget::drawPetalRing(QPainter &p, const QPointF &c, qreal R, int n,
                              qreal visibleLen, qreal len, const QColor &col, qreal off)
{
    p.save();
    p.translate(c);
    const qreal halfW = qMax<qreal>(2.6, len * 0.46);
    const qreal baseY = -(R - len * 0.32);          // 基部藏进花盘
    const qreal tipY  = -(R + visibleLen);
    const qreal midY  = -(R + visibleLen * 0.42);
    for (int k = 0; k < n; ++k) {
        const qreal ang = 2.0 * M_PI * k / n + off;
        p.save();
        p.rotate(ang * 180.0 / M_PI);
        QPainterPath pet;
        pet.moveTo(-halfW * 0.5, baseY);
        pet.quadTo(-halfW, midY, 0, tipY);
        pet.quadTo(halfW, midY, halfW * 0.5, baseY);
        pet.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawPath(pet);
        p.restore();
    }
    p.restore();
}

// PVZ 向日葵花头：外深内浅双层花瓣 + 渐变棕色花盘
void PetWidget::drawSunflowerBloom(QPainter &p, const QPointF &c, qreal R, int stage)
{
    if (stage == 3) {
        QRadialGradient glow(c, R * 2.5);
        glow.setColorAt(0.0, QColor(255, 236, 150, 80));
        glow.setColorAt(1.0, QColor(255, 236, 150, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(c, R * 2.5, R * 2.5);

        // 外层长瓣（深金） + 内层短瓣（亮黄，错位半格）
        drawPetalRing(p, c, R, 14, R * 0.92, R * 0.72, QColor("#f59e0b"), 0.0);
        drawPetalRing(p, c, R, 14, R * 0.55, R * 0.48, QColor("#ffce2e"), M_PI / 14.0);
    } else {
        // 初绽：单层 12 瓣
        drawPetalRing(p, c, R, 12, R * 0.72, R * 0.62, QColor("#ffc21f"), 0.0);
    }

    // 花盘：暖棕径向渐变，边缘深
    QRadialGradient disc(c, R);
    disc.setColorAt(0.0, QColor("#a86f35"));
    disc.setColorAt(0.72, QColor("#8f5b28"));
    disc.setColorAt(1.0, QColor("#6f421a"));
    p.setPen(QPen(QColor("#543114"), 1.8));
    p.setBrush(disc);
    p.drawEllipse(c, R, R);

    // 花盘边缘淡淡的籽粒点（增强向日葵辨识度，不抢表情）
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(52, 28, 10, 52));
    const int n = 24;
    for (int i = 0; i < n; ++i) {
        const qreal a = 2.0 * M_PI * i / n + M_PI / n;
        p.drawEllipse(QPointF(c.x() + qCos(a) * R * 0.86,
                              c.y() + qSin(a) * R * 0.86),
                      R * 0.045, R * 0.045);
    }
}

// PVZ 向日葵招牌表情：竖长圆角眼 + 高光 + 大大张开的笑
void PetWidget::drawSunflowerFace(QPainter &p, const QPointF &c, qreal r)
{
    const QColor ink("#291708");
    const qreal ex = r * 0.44;
    const qreal ey = c.y() - r * 0.02;
    const qreal ew = r * 0.135;    // 眼半宽
    const qreal eh = r * 0.30;     // 眼半高
    const QPointF ecL(c.x() - ex, ey);
    const QPointF ecR(c.x() + ex, ey);

    auto drawOpenEyes = [&]() {
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawRoundedRect(QRectF(ecL.x() - ew, ecL.y() - eh, ew * 2, eh * 2), ew * 0.9, ew * 0.9);
        p.drawRoundedRect(QRectF(ecR.x() - ew, ecR.y() - eh, ew * 2, eh * 2), ew * 0.9, ew * 0.9);
        // 高光点
        p.setBrush(QColor(255, 255, 255, 235));
        p.drawEllipse(QPointF(ecL.x() - ew * 0.28, ecL.y() - eh * 0.55), ew * 0.34, eh * 0.22);
        p.drawEllipse(QPointF(ecR.x() - ew * 0.28, ecR.y() - eh * 0.55), ew * 0.34, eh * 0.22);
    };
    auto drawClosedHappy = [&](qreal k) {   // ∩ 眯眯笑
        p.setBrush(Qt::NoBrush);
        QPen pen(ink, qMax(2.0, r * 0.07), Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        QRectF bl(ecL.x() - ew * 1.5 * k, ecL.y() - eh * 1.1, ew * 3.0 * k, eh * 2.2);
        QRectF br(ecR.x() - ew * 1.5 * k, ecR.y() - eh * 1.1, ew * 3.0 * k, eh * 2.2);
        p.drawArc(bl, 180 * 16, -180 * 16);
        p.drawArc(br, 180 * 16, -180 * 16);
    };
    auto drawDroopy = [&]() {       // 疲惫 ∪
        p.setBrush(Qt::NoBrush);
        QPen pen(ink, qMax(2.0, r * 0.06), Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        QRectF bl(ecL.x() - ew * 1.4, ecL.y() - eh * 0.4, ew * 2.8, eh * 1.4);
        QRectF br(ecR.x() - ew * 1.4, ecR.y() - eh * 0.4, ew * 2.8, eh * 1.4);
        p.drawArc(bl, 20 * 16, -150 * 16);
        p.drawArc(br, 20 * 16, -150 * 16);
    };
    auto drawSleepLines = [&]() {
        QPen pen(ink, qMax(2.0, r * 0.06), Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(ecL.x() - ew, ecL.y()), QPointF(ecL.x() + ew, ecL.y()));
        p.drawLine(QPointF(ecR.x() - ew, ecR.y()), QPointF(ecR.x() + ew, ecR.y()));
    };

    // 张开的笑（开口笑，深色口腔）
    auto drawBigSmile = [&](qreal wf, qreal df) {
        QPainterPath mo;
        const qreal mw = r * wf;
        const qreal my = c.y() + r * 0.20;
        mo.moveTo(c.x() - mw, my);
        mo.quadTo(c.x() - mw * 0.45, my + r * df, c.x(), my + r * (df + 0.07));
        mo.quadTo(c.x() + mw * 0.45, my + r * df, c.x() + mw, my);
        // 上唇（中间略下垂的弧线）
        mo.quadTo(c.x() + mw * 0.5, my - r * 0.02, c.x(), my + r * 0.09);
        mo.quadTo(c.x() - mw * 0.5, my - r * 0.02, c.x() - mw, my);
        mo.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawPath(mo);
    };

    bool blush = false;
    switch (m_mood) {
    case Idle: {
        if (m_blinking) {
            QPen pen(ink, qMax(2.0, r * 0.06), Qt::SolidLine, Qt::RoundCap);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(ecL.x() - ew, ecL.y()), QPointF(ecL.x() + ew, ecL.y()));
            p.drawLine(QPointF(ecR.x() - ew, ecR.y()), QPointF(ecR.x() + ew, ecR.y()));
        } else {
            drawOpenEyes();
        }
        drawBigSmile(0.30, 0.20);
        break;
    }
    case Focused: {          // 专注：闭眼努力 + 小“o”嘴
        drawClosedHappy(0.8);
        p.setBrush(ink);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(c.x(), c.y() + r * 0.36), r * 0.075, r * 0.09);
        blush = true;
        break;
    }
    case Happy: {            // 开心：∩∩ + 大笑
        drawClosedHappy(1.0);
        drawBigSmile(0.36, 0.26);
        blush = true;
        break;
    }
    case Tired: {            // 疲惫：耷拉眼 + 小弧线嘴
        drawDroopy();
        QPen pen(ink, qMax(2.0, r * 0.05), Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(c.x() - r * 0.18, c.y() + r * 0.22, r * 0.36, r * 0.2), 200 * 16, -130 * 16);
        break;
    }
    case Sleepy:             // 睡觉：线眼 + 小圆嘴
        drawSleepLines();
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawEllipse(QPointF(c.x(), c.y() + r * 0.34), r * 0.07, r * 0.06);
        break;
    case Celebrate:          // 雀跃：∩∩ + 超大张嘴
        drawClosedHappy(1.15);
        drawBigSmile(0.40, 0.30);
        blush = true;
        break;
    }

    if (blush) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 158, 128, 110));
        p.drawEllipse(QPointF(c.x() - r * 0.66, c.y() + r * 0.28), r * 0.10, r * 0.065);
        p.drawEllipse(QPointF(c.x() + r * 0.66, c.y() + r * 0.28), r * 0.10, r * 0.065);
    }
}

void PetWidget::drawFace(QPainter &p, const QPointF &c, qreal r)
{
    const QColor ink = m_faceInk;
    const qreal ex = r * 0.34;
    const QPointF el(c.x() - ex, c.y() - r * 0.08);
    const QPointF er(c.x() + ex, c.y() - r * 0.08);
    const qreal eh = r * 0.17;         // 半宽
    const qreal ew = r * 0.09;         // 画笔粗细

    QPen pen(ink, qMax(2.0, ew), Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(ink);

    // ---- 眼睛 ----
    bool drawBlush = false;
    switch (m_mood) {
    case Idle:
        if (m_blinking) {
            p.drawLine(QPointF(el.x() - eh, el.y()), QPointF(el.x() + eh, el.y()));
            p.drawLine(QPointF(er.x() - eh, er.y()), QPointF(er.x() + eh, er.y()));
        } else {
            p.drawEllipse(el, eh * 0.52, eh * 0.72);
            p.drawEllipse(er, eh * 0.52, eh * 0.72);
        }
        break;
    case Focused:        // 认真闭眼(∩)
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(el.x() - eh, el.y() - eh * 1.05, eh * 2, eh * 2), 180 * 16, -180 * 16);
        p.drawArc(QRectF(er.x() - eh, er.y() - eh * 1.05, eh * 2, eh * 2), 180 * 16, -180 * 16);
        drawBlush = true;
        break;
    case Happy:          // 开心眯眼
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(el.x() - eh * 1.1, el.y() - eh * 1.2, eh * 2.2, eh * 2.2), 180 * 16, -180 * 16);
        p.drawArc(QRectF(er.x() - eh * 1.1, er.y() - eh * 1.2, eh * 2.2, eh * 2.2), 180 * 16, -180 * 16);
        drawBlush = true;
        break;
    case Tired:          // 疲惫耷拉眼(∪)
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(el.x() - eh * 1.1, el.y() - eh * 0.6, eh * 2.2, eh * 2.2), 0 * 16, -140 * 16);
        p.drawArc(QRectF(er.x() - eh * 1.1, er.y() - eh * 0.6, eh * 2.2, eh * 2.2), 0 * 16, -140 * 16);
        break;
    case Sleepy:         // 睡觉: 闭合线
        p.drawLine(QPointF(el.x() - eh, el.y()), QPointF(el.x() + eh, el.y()));
        p.drawLine(QPointF(er.x() - eh, er.y()), QPointF(er.x() + eh, er.y()));
        break;
    case Celebrate:      // 雀跃
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(el.x() - eh * 1.15, el.y() - eh * 1.25, eh * 2.3, eh * 2.3), 180 * 16, -180 * 16);
        p.drawArc(QRectF(er.x() - eh * 1.15, er.y() - eh * 1.25, eh * 2.3, eh * 2.3), 180 * 16, -180 * 16);
        drawBlush = true;
        break;
    }

    // ---- 嘴 ----
    const QRectF mouth(c.x() - r * 0.24, c.y() + r * 0.16, r * 0.48, r * 0.34);
    p.setBrush(ink);
    switch (m_mood) {
    case Idle:
        p.setBrush(Qt::NoBrush);
        p.drawArc(mouth, 0 * 16, -140 * 16);
        break;
    case Focused:
        p.setBrush(Qt::NoBrush);
        p.drawArc(mouth, 0 * 16, -160 * 16);
        break;
    case Happy:
        p.setBrush(Qt::NoBrush);
        p.drawArc(mouth.adjusted(0, -r * 0.02, 0, r * 0.06), 0 * 16, -180 * 16);
        break;
    case Tired:
        p.setBrush(Qt::NoBrush);
        p.drawArc(mouth, 0 * 16, -90 * 16);
        break;
    case Sleepy:
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawEllipse(QPointF(c.x(), c.y() + r * 0.36), r * 0.07, r * 0.065);
        break;
    case Celebrate: {
        p.setBrush(QColor("#8d3b2f"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(c.x(), c.y() + r * 0.36), r * 0.14, r * 0.17);
        p.setBrush(QColor("#ff8a80"));
        p.drawEllipse(QPointF(c.x(), c.y() + r * 0.46), r * 0.10, r * 0.08);
        break;
    }
    }

    // ---- 腮红 ----
    if (drawBlush) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 138, 128, 120));
        p.drawEllipse(QPointF(c.x() - r * 0.6, c.y() + r * 0.34), r * 0.11, r * 0.07);
        p.drawEllipse(QPointF(c.x() + r * 0.6, c.y() + r * 0.34), r * 0.11, r * 0.07);
    }
}

void PetWidget::drawParticles(QPainter &p)
{
    if (m_particles.isEmpty())
        return;
    for (const Particle &pt : m_particles) {
        const qreal k = 1.0 - (pt.age / pt.life);
        QColor col = pt.color;
        col.setAlphaF(qBound<qreal>(0.0, k, 1.0) * 0.95);
        if (pt.kind == 0) {
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            p.drawEllipse(pt.pos, pt.size * 0.5, pt.size * 0.5);
        } else {
            QPen star(col, qMax<qreal>(1.2, pt.size * 0.22), Qt::SolidLine, Qt::RoundCap);
            p.setPen(star);
            const qreal s = pt.size;
            p.drawLine(pt.pos + QPointF(-s, 0), pt.pos + QPointF(s, 0));
            p.drawLine(pt.pos + QPointF(0, -s), pt.pos + QPointF(0, s));
            p.drawLine(pt.pos + QPointF(-s * 0.6, -s * 0.6), pt.pos + QPointF(s * 0.6, s * 0.6));
            p.drawLine(pt.pos + QPointF(-s * 0.6, s * 0.6), pt.pos + QPointF(s * 0.6, -s * 0.6));
        }
    }
}
