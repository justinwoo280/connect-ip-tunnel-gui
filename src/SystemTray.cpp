#include "SystemTray.h"
#include "MainWindow.h"

#include <QApplication>

SystemTray::SystemTray(MainWindow *window, QObject *parent)
    : QObject(parent)
{
    m_tray = new QSystemTrayIcon(QApplication::windowIcon(), this);

    m_menu = new QMenu();
    m_actShow       = m_menu->addAction(tr("显示主窗口"));
    m_menu->addSeparator();
    m_actConnect    = m_menu->addAction(tr("连接"));
    m_actDisconnect = m_menu->addAction(tr("断开"));
    m_actDisconnect->setEnabled(false);
    m_menu->addSeparator();
    m_actQuit = m_menu->addAction(tr("退出"));

    m_tray->setContextMenu(m_menu);
    m_tray->setToolTip("Connect-IP Tunnel");
    m_tray->show();

    connect(m_tray, &QSystemTrayIcon::activated,
            this, &SystemTray::onActivated);
    connect(m_actShow,       &QAction::triggered, this, &SystemTray::showRequested);
    connect(m_actConnect,    &QAction::triggered, this, &SystemTray::connectRequested);
    connect(m_actDisconnect, &QAction::triggered, this, &SystemTray::disconnectRequested);
    connect(m_actQuit,       &QAction::triggered, this, &SystemTray::quitRequested);
}

SystemTray::~SystemTray()
{
    delete m_menu;
}

void SystemTray::setStatus(bool connected, const QString &tip)
{
    m_actConnect->setEnabled(!connected);
    m_actDisconnect->setEnabled(connected);

    QString tooltip = connected
        ? tr("Connect-IP Tunnel — 已连接")
        : tr("Connect-IP Tunnel — 未连接");
    if (!tip.isEmpty())
        tooltip += "\n" + tip;
    m_tray->setToolTip(tooltip);
}

void SystemTray::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick ||
        reason == QSystemTrayIcon::Trigger)
        emit showRequested();
}
