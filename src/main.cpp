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
    app.setOrganizationName(QStringLiteral("focus-garden"));
    app.setApplicationName(QStringLiteral("focus-garden"));
    app.setApplicationDisplayName(QStringLiteral("时光花园"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/focus-garden.svg")));
    app.setQuitOnLastWindowClosed(false); // 主界面隐藏到托盘后保持运行

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Focus Garden - a plant that grows with your focus"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    FocusManager mgr;
    SystemLinker linker;
    MainWindow win(&mgr, &linker);
    win.show();

    return app.exec();
}
