#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "EditNodeDialog.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QMessageBox>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_nodeMgr(new NodeManager(this))
    , m_core(new CoreProcess(this))
    , m_poller(new StatsPoller(this))
    , m_tray(new SystemTray(this, this))
{
    ui->setupUi(this);
    setWindowTitle("Connect-IP Tunnel");

    setupNodeTable();
    setupConnections();

    m_nodeMgr->load();
    updateButtons();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ── 初始化 ────────────────────────────────────────────────────────────────────

void MainWindow::setupNodeTable()
{
    ui->tableNodes->setColumnCount(4);
    ui->tableNodes->setHorizontalHeaderLabels({tr("名称"), tr("服务端"), tr("认证"), tr("延迟")});
    ui->tableNodes->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableNodes->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableNodes->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableNodes->horizontalHeader()->setStretchLastSection(false);
    ui->tableNodes->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tableNodes->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableNodes->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableNodes->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->tableNodes->setAlternatingRowColors(true);
}

void MainWindow::setupStatusPanel()
{
    // Status panel widgets are defined in the UI file; nothing extra to set up here.
}

void MainWindow::setupConnections()
{
    // 工具栏按钮
    connect(ui->btnAdd,        &QPushButton::clicked, this, &MainWindow::onAddNode);
    connect(ui->btnEdit,       &QPushButton::clicked, this, &MainWindow::onEditNode);
    connect(ui->btnDelete,     &QPushButton::clicked, this, &MainWindow::onDeleteNode);
    connect(ui->btnConnect,    &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(ui->btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnect);

    // 节点表格
    connect(ui->tableNodes, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onNodeSelectionChanged);
    connect(ui->tableNodes, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onNodeDoubleClicked);

    // NodeManager
    connect(m_nodeMgr, &NodeManager::nodesChanged, this, &MainWindow::onNodesChanged);

    // CoreProcess
    connect(m_core, &CoreProcess::started,         this, &MainWindow::onCoreStarted);
    connect(m_core, &CoreProcess::stopped,         this, &MainWindow::onCoreStopped);
    connect(m_core, &CoreProcess::errorOccurred,   this, &MainWindow::onCoreError);
    connect(m_core, &CoreProcess::logReceived,     this, &MainWindow::onCoreLog);
    connect(m_core, &CoreProcess::adminPortReady,  this, &MainWindow::onAdminPortReady);

    // StatsPoller
    connect(m_poller, &StatsPoller::statsUpdated, this, &MainWindow::onStatsUpdated);

    // SystemTray
    connect(m_tray, &SystemTray::showRequested,       this, &MainWindow::onTrayShow);
    connect(m_tray, &SystemTray::connectRequested,    this, &MainWindow::onTrayConnect);
    connect(m_tray, &SystemTray::disconnectRequested, this, &MainWindow::onTrayDisconnect);
    connect(m_tray, &SystemTray::quitRequested,       this, &MainWindow::onTrayQuit);
}

// ── 节点操作 ──────────────────────────────────────────────────────────────────

void MainWindow::onAddNode()
{
    EditNodeDialog dlg(this);
    TunnelNode defaultNode;
    defaultNode.name = tr("新节点");
    dlg.setNode(defaultNode);

    if (dlg.exec() == QDialog::Accepted) {
        TunnelNode n = dlg.getNode();
        if (!n.isValid()) {
            QMessageBox::warning(this, tr("无效节点"), tr("请填写服务端地址和认证信息。"));
            return;
        }
        m_nodeMgr->addNode(n);
    }
}

void MainWindow::onEditNode()
{
    int id = selectedNodeId();
    if (id < 0) return;

    TunnelNode node = m_nodeMgr->getNode(id);
    EditNodeDialog dlg(this);
    dlg.setNode(node);

    if (dlg.exec() == QDialog::Accepted) {
        TunnelNode updated = dlg.getNode();
        updated.id = id;
        if (!updated.isValid()) {
            QMessageBox::warning(this, tr("无效节点"), tr("请填写服务端地址和认证信息。"));
            return;
        }
        m_nodeMgr->updateNode(updated);
    }
}

void MainWindow::onDeleteNode()
{
    int id = selectedNodeId();
    if (id < 0) return;

    TunnelNode node = m_nodeMgr->getNode(id);
    if (QMessageBox::question(this, tr("删除节点"),
            tr("确定要删除节点「%1」吗？").arg(node.name))
        != QMessageBox::Yes)
        return;

    if (m_isConnected && m_activeNodeId == id)
        onDisconnect();

    m_nodeMgr->removeNode(id);
}

void MainWindow::onNodeSelectionChanged()
{
    updateButtons();
}

void MainWindow::onNodeDoubleClicked(int row, int col)
{
    Q_UNUSED(col)
    if (row < 0 || row >= ui->tableNodes->rowCount()) return;
    if (!m_isConnected)
        onConnect();
    else
        onEditNode();
}

// ── 连接控制 ──────────────────────────────────────────────────────────────────

void MainWindow::onConnect()
{
    int id = selectedNodeId();
    if (id < 0) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个节点。"));
        return;
    }

    TunnelNode node = m_nodeMgr->getNode(id);
    if (!node.isValid()) {
        QMessageBox::warning(this, tr("无效节点"), tr("节点配置不完整，请先编辑。"));
        return;
    }

    if (!m_core->start(node)) {
        QMessageBox::critical(this, tr("启动失败"), m_core->lastError());
        return;
    }
    m_activeNodeId = id;
}

void MainWindow::onDisconnect()
{
    m_poller->stopPolling();
    m_core->stop();
    m_activeNodeId = -1;
}

// ── CoreProcess 信号 ──────────────────────────────────────────────────────────

void MainWindow::onCoreStarted()
{
    m_isConnected = true;
    updateButtons();
    m_tray->setStatus(true);
    ui->labelStatus->setText(tr("● 正在连接..."));
    ui->labelStatus->setStyleSheet("color: orange; font-weight: bold;");
    appendLog(tr("[GUI] 内核已启动，等待连接..."));
}

void MainWindow::onCoreStopped()
{
    m_isConnected = false;
    m_poller->stopPolling();
    updateButtons();
    m_tray->setStatus(false);
    ui->labelStatus->setText(tr("○ 未连接"));
    ui->labelStatus->setStyleSheet("color: gray;");

    // 清空统计面板
    ui->labelAssignedIP->setText("-");
    ui->labelUptime->setText("-");
    ui->labelTxBytes->setText("-");
    ui->labelRxBytes->setText("-");
    ui->labelTxPackets->setText("-");
    ui->labelRxPackets->setText("-");
    ui->labelDrops->setText("-");
    appendLog(tr("[GUI] 内核已停止"));
}

void MainWindow::onCoreError(const QString &error)
{
    appendLog(tr("[错误] %1").arg(error));
    QMessageBox::critical(this, tr("内核错误"), error);
}

void MainWindow::onCoreLog(const QString &message)
{
    appendLog(message);
}

void MainWindow::onAdminPortReady(quint16 port)
{
    appendLog(tr("[GUI] Admin 接口就绪，端口: %1，开始统计轮询").arg(port));
    m_poller->startPolling(port, 1000);
}

// ── StatsPoller 信号 ──────────────────────────────────────────────────────────

void MainWindow::onStatsUpdated(const TunnelStats &stats)
{
    updateStatusPanel(stats);
}

// ── NodeManager 信号 ──────────────────────────────────────────────────────────

void MainWindow::onNodesChanged()
{
    updateNodeTable();
    updateButtons();
}

// ── 系统托盘 ──────────────────────────────────────────────────────────────────

void MainWindow::onTrayShow()
{
    show();
    raise();
    activateWindow();
}

void MainWindow::onTrayConnect()
{
    onConnect();
}

void MainWindow::onTrayDisconnect()
{
    onDisconnect();
}

void MainWindow::onTrayQuit()
{
    m_forceQuit = true;
    if (m_isConnected) {
        // 等待内核进程真正退出后再退出 Qt 事件循环，避免竞态导致 UI 组件被访问时已析构
        connect(m_core, &CoreProcess::stopped,
                QApplication::instance(), &QCoreApplication::quit,
                Qt::SingleShotConnection);
        // 保底超时：若 5 秒内内核没退出则强制退出
        QTimer::singleShot(5000, QApplication::instance(), &QCoreApplication::quit);
        onDisconnect();
    } else {
        QApplication::quit();
    }
}

// ── UI 更新 ───────────────────────────────────────────────────────────────────

void MainWindow::updateNodeTable()
{
    int prevRow = ui->tableNodes->currentRow();
    ui->tableNodes->setRowCount(0);

    for (const TunnelNode &node : m_nodeMgr->nodes()) {
        int row = ui->tableNodes->rowCount();
        ui->tableNodes->insertRow(row);

        auto *itemName = new QTableWidgetItem(node.name);
        itemName->setData(Qt::UserRole, node.id);
        ui->tableNodes->setItem(row, 0, itemName);
        ui->tableNodes->setItem(row, 1, new QTableWidgetItem(node.serverAddr));
        ui->tableNodes->setItem(row, 2, new QTableWidgetItem(node.displayAuth()));
        ui->tableNodes->setItem(row, 3, new QTableWidgetItem(node.displayLatency()));

        // 高亮当前连接中的节点
        if (node.id == m_activeNodeId) {
            for (int c = 0; c < 4; c++) {
                if (auto *item = ui->tableNodes->item(row, c))
                    item->setForeground(Qt::darkGreen);
            }
        }
    }

    if (prevRow >= 0 && prevRow < ui->tableNodes->rowCount())
        ui->tableNodes->selectRow(prevRow);
}

void MainWindow::updateStatusPanel(const TunnelStats &stats)
{
    if (stats.status == "connected") {
        ui->labelStatus->setText(tr("● 已连接"));
        ui->labelStatus->setStyleSheet("color: green; font-weight: bold;");
        m_tray->setStatus(true, stats.assignedIPv4);
    } else if (stats.status == "connecting") {
        ui->labelStatus->setText(tr("● 正在连接..."));
        ui->labelStatus->setStyleSheet("color: orange; font-weight: bold;");
    } else {
        ui->labelStatus->setText(tr("○ 已断开"));
        ui->labelStatus->setStyleSheet("color: gray;");
    }

    QString ip = stats.assignedIPv4;
    if (!stats.assignedIPv6.isEmpty())
        ip += " / " + stats.assignedIPv6;
    ui->labelAssignedIP->setText(ip.isEmpty() ? "-" : ip);
    ui->labelUptime->setText(stats.uptimeSeconds > 0
        ? TunnelStats::formatUptime(stats.uptimeSeconds) : "-");
    ui->labelTxBytes->setText(TunnelStats::formatBytes(stats.txBytes));
    ui->labelRxBytes->setText(TunnelStats::formatBytes(stats.rxBytes));
    ui->labelTxPackets->setText(QString::number(stats.txPackets));
    ui->labelRxPackets->setText(QString::number(stats.rxPackets));
    ui->labelDrops->setText(QString::number(stats.drops));
    ui->labelTunName->setText(stats.tun.isEmpty() ? "-" : stats.tun);
}

void MainWindow::updateButtons()
{
    bool hasSelection = selectedNodeId() >= 0;
    ui->btnEdit->setEnabled(hasSelection);
    ui->btnDelete->setEnabled(hasSelection && !m_isConnected);
    ui->btnConnect->setEnabled(hasSelection && !m_isConnected);
    ui->btnDisconnect->setEnabled(m_isConnected);
}

void MainWindow::appendLog(const QString &msg)
{
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->textLog->append(QString("[%1] %2").arg(ts, msg));
    // 自动滚动到底部
    QScrollBar *sb = ui->textLog->verticalScrollBar();
    sb->setValue(sb->maximum());
}

int MainWindow::selectedNodeId() const
{
    int row = ui->tableNodes->currentRow();
    if (row < 0) return -1;
    auto *item = ui->tableNodes->item(row, 0);
    if (!item) return -1;
    return item->data(Qt::UserRole).toInt();
}

// ── 窗口关闭 ──────────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_forceQuit) {
        // 最小化到托盘，不退出
        hide();
        event->ignore();
        // 首次最小化时显示气泡提示，告知用户程序仍在运行
        if (!m_trayMsgShown) {
            m_trayMsgShown = true;
            m_tray->showTrayMessage(
                tr("Connect-IP Tunnel"),
                tr("程序已最小化到系统托盘，双击图标可重新打开窗口。"),
                3000);
        }
    } else {
        event->accept();
    }
}
