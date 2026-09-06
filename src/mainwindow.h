#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QList>
#include <QSettings>

#include "focusmanager.h"

class SystemLinker;
class PetWidget;
class QLabel;
class QPushButton;
class QSystemTrayIcon;
class QMenu;
class QMouseEvent;
class QCloseEvent;
class QEvent;
class QAction;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(FocusManager *mgr, SystemLinker *linker, QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void quitApplication();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void closeEvent(QCloseEvent *) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void onPhaseChanged(FocusManager::Phase phase);
    void onTick(int remaining);
    void onFocusCompleted(int inRow, int stage);
    void onFocusAborted();
    void onStageChanged(int stage);
    void onBreakFinished(bool wasLong);
    void onStatusMessage(const QString &title, const QString &sub);
    void onPrimaryClicked();
    void onSecondaryClicked();
    void showStats();
    void openSettings();
    void toggleVisible();

private:
    void buildUi();
    void buildTray();
    void buildSkinActions();
    void showSkinMenu(const QPoint &globalPos);
    void applySkinKey(const QString &key);
    void updateSkinChecks();
    void applyPrefs();
    void setPinDesktop(bool on);   // 常驻：不受「显示桌面」影响
    // 无边框窗口：自由缩放（可大可小）
    void beginResize(int dir, const QPoint &globalPos);
    void doResize(const QPoint &globalPos);
    void endResize();
    void setEdgeCursor(int dir);
    void updateHoverCursor(const QPoint &pos);
    void updateMoodAndText();
    void updateActions();
    void updateStageLabel();
    void updateTimeLabel();
    void notify(const QString &t, const QString &b);
    static QString fmt(int seconds);

    FocusManager *m_mgr = nullptr;
    SystemLinker *m_linker = nullptr;
    PetWidget *m_pet = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_stageLabel = nullptr;
    QLabel *m_msgTitle = nullptr;
    QLabel *m_msgSub = nullptr;
    QPushButton *m_primary = nullptr;
    QPushButton *m_secondary = nullptr;
    QPushButton *m_statsBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_trayPlay = nullptr;
    QAction *m_trayPin = nullptr;
    QAction *m_trayToggle = nullptr;
    QList<QAction *> m_skinActs;
    QSettings m_prefs;
    bool m_quitting = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
    bool m_justCompleted = false;
    // 手动缩放状态
    bool m_resizing = false;
    int  m_resizeDir = 0;   // 位：1左 2右 4上 8下
    QPoint m_resizeStart;
    QRect  m_resizeGeom;
    QSize  m_resizeMin;
};

#endif // MAINWINDOW_H
