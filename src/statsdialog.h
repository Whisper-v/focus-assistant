#ifndef STATSDIALOG_H
#define STATSDIALOG_H

#include <QDialog>
#include <QWidget>

class FocusManager;

class BarsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BarsWidget(FocusManager *mgr, QWidget *parent = nullptr);
    void refresh();
    QSize sizeHint() const override { return QSize(460, 150); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    FocusManager *m_mgr = nullptr;
    QList<QPair<QString, int>> m_days; // label, minutes
};

class StatsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StatsDialog(FocusManager *mgr, QWidget *parent = nullptr);
    void showEvent(QShowEvent *) override;
private:
    FocusManager *m_mgr = nullptr;
    BarsWidget *m_bars = nullptr;
    QWidget *m_summary = nullptr;
};

#endif // STATSDIALOG_H
