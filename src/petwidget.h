#ifndef PETWIDGET_H
#define PETWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QList>
#include <QPointF>
#include <QColor>

// 露露 —— 一株长在花盆里、有表情的小花。
// 纯 QPainter 矢量绘制，无外部图片依赖；成长阶段 0..3 改变形态，
// 通过 mood 表达专注/疲惫/开心/睡觉/庆祝等情绪，配合低频呼吸与眨眼动画。
class PetWidget : public QWidget
{
    Q_OBJECT
public:
    enum Mood { Idle, Focused, Tired, Sleepy, Happy, Celebrate };
    Q_ENUM(Mood)

    explicit PetWidget(QWidget *parent = nullptr);

    void setStage(int stage);
    void setMood(Mood mood);
    void setProgress(qreal p);       // 0..1 外圈倒计时环
    void startCelebration();
    Mood mood() const { return m_mood; }

    QSize sizeHint() const override { return QSize(300, 250); }

protected:
    void paintEvent(QPaintEvent *) override;

private slots:
    void onAnimTick();
    void onBlinkTimeout();

private:
    void scheduleBlink();
    void drawRing(QPainter &p);
    void drawPot(QPainter &p);
    void drawPlant(QPainter &p, QPointF &headPos, qreal &headR);
    void drawFace(QPainter &p, const QPointF &c, qreal r);
    void drawLeaves(QPainter &p, const QPointF &base, qreal size);
    void drawParticles(QPainter &p);
    void spawnCelebrationParticle();

    int m_stage = 0;
    Mood m_mood = Idle;
    qreal m_progress = 0.0;

    QTimer m_animTimer;         // 呼吸 / 庆祝驱动
    qreal m_breathPhase = 0.0;
    bool m_celebrating = false;
    int m_celebTicks = 0;

    QTimer m_blinkTimer;
    bool m_blinking = false;

    struct Particle {
        QPointF pos;
        QPointF vel;
        qreal age = 0;
        qreal life = 1.0;
        qreal size = 4;
        QColor color;
        int kind = 0;           // 0 圆点 1 星星
    };
    QList<Particle> m_particles;
};

#endif // PETWIDGET_H
