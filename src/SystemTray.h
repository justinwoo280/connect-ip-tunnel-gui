#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class MainWindow;

class SystemTray : public QObject
{
    Q_OBJECT

public:
    explicit SystemTray(MainWindow *window, QObject *parent = nullptr);
    ~SystemTray();

    void setStatus(bool connected, const QString &tip = {});

signals:
    void showRequested();
    void connectRequested();
    void disconnectRequested();
    void quitRequested();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    QSystemTrayIcon *m_tray       = nullptr;
    QMenu           *m_menu       = nullptr;
    QAction         *m_actShow    = nullptr;
    QAction         *m_actConnect = nullptr;
    QAction         *m_actDisconnect = nullptr;
    QAction         *m_actQuit    = nullptr;
};
