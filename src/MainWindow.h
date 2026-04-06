#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QTimer>

#include "TunnelNode.h"
#include "CoreProcess.h"
#include "NodeManager.h"
#include "StatsPoller.h"
#include "SystemTray.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 节点操作
    void onAddNode();
    void onEditNode();
    void onDeleteNode();
    void onNodeSelectionChanged();
    void onNodeDoubleClicked(int row, int col);

    // 连接控制
    void onConnect();
    void onDisconnect();

    // CoreProcess 信号
    void onCoreStarted();
    void onCoreStopped();
    void onCoreError(const QString &error);
    void onCoreLog(const QString &message);
    void onAdminPortReady(quint16 port);

    // StatsPoller 信号
    void onStatsUpdated(const TunnelStats &stats);

    // 节点列表变化
    void onNodesChanged();

    // 系统托盘
    void onTrayShow();
    void onTrayConnect();
    void onTrayDisconnect();
    void onTrayQuit();

private:
    void setupNodeTable();
    void setupStatusPanel();
    void setupConnections();
    void updateNodeTable();
    void updateStatusPanel(const TunnelStats &stats);
    void updateButtons();
    void appendLog(const QString &msg);
    int  selectedNodeId() const;

    Ui::MainWindow *ui;

    NodeManager  *m_nodeMgr   = nullptr;
    CoreProcess  *m_core      = nullptr;
    StatsPoller  *m_poller    = nullptr;
    SystemTray   *m_tray      = nullptr;

    int  m_activeNodeId  = -1;   // 当前连接的节点 ID
    bool m_isConnected   = false;
    bool m_forceQuit     = false;
    bool m_trayMsgShown  = false; // 是否已经显示过"最小化到托盘"提示
};
