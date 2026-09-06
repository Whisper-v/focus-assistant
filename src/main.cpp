#include <DApplication>

#include <QIcon>
#include <QCommandLineParser>

#include "focusmanager.h"

DWIDGET_USE_NAMESPACE
#include "systemlinker.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    DApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("focus-assistant"));
    app.setApplicationName(QStringLiteral("focus-assistant"));
    app.setApplicationDisplayName(QStringLiteral("专注助手"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/focus-assistant.svg")));
    app.setQuitOnLastWindowClosed(false); // 主界面隐藏到托盘后保持运行

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Focus Assistant - a plant that grows with your focus"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    FocusManager mgr;
    SystemLinker linker;
    MainWindow win(&mgr, &linker);
    win.show();

    return app.exec();
}
