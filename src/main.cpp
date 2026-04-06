#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("connect-ip-tunnel-gui");
    app.setApplicationDisplayName("Connect-IP Tunnel");
    app.setOrganizationName("connect-ip-tunnel");

    // 设置多尺寸应用图标，系统会自动选择最合适的尺寸
    QIcon appIcon;
    appIcon.addFile(":/app_16.png",  QSize(16,  16));
    appIcon.addFile(":/app_32.png",  QSize(32,  32));
    appIcon.addFile(":/app_48.png",  QSize(48,  48));
    appIcon.addFile(":/app_64.png",  QSize(64,  64));
    appIcon.addFile(":/app_128.png", QSize(128, 128));
    appIcon.addFile(":/app_256.png", QSize(256, 256));
    app.setWindowIcon(appIcon);

    // 允许关闭所有窗口后仍保持运行（托盘驻留）
    app.setQuitOnLastWindowClosed(false);

    MainWindow w;
    w.show();

    return app.exec();
}
