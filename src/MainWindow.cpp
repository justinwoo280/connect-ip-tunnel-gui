#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "EditNodeDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStatusBar>

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

    // QSplitter 初始尺寸：左侧节点列表占 40%，右侧状态面板占 60%
    if (auto *sp = ui->splitterMain) {
        sp->setSizes({360, 540});
        sp->setStretchFactor(0, 0);
        sp->setStretchFactor(1, 1);
    }

    // 初始状态徽章
    setStatusIndicator(QStringLiteral("idle"), tr("未连接"));

    // statusBar：显示版本号 + admin port（连接后）
    statusBar()->showMessage(tr("就绪 · 版本 %1").arg(QStringLiteral("1.0.0")));

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
    // 隐藏垂直 header 的行号，节省空间；启用右键菜单
    ui->tableNodes->verticalHeader()->setVisible(false);
    ui->tableNodes->setContextMenuPolicy(Qt::CustomContextMenu);
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
    connect(ui->tableNodes, &QWidget::customContextMenuRequested,
            this, &MainWindow::onNodeContextMenu);

    // 日志面板工具按钮
    if (auto *btn = findChild<QPushButton*>(QStringLiteral("btnLogClear")))
        connect(btn, &QPushButton::clicked, this, &MainWindow::onLogClear);
    if (auto *btn = findChild<QPushButton*>(QStringLiteral("btnLogCopy")))
        connect(btn, &QPushButton::clicked, this, &MainWindow::onLogCopy);

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
    setStatusIndicator(QStringLiteral("connecting"), tr("正在连接…"));
    // 顶部卡片显示当前节点
    if (m_activeNodeId >= 0) {
        TunnelNode n = m_nodeMgr->getNode(m_activeNodeId);
        ui->labelActiveNode->setText(tr("→ %1 (%2)").arg(n.name, n.serverAddr));
    }
    ui->labelAssignedIPLarge->setText("…");
    appendLog(tr("[GUI] 内核已启动，等待连接..."));
    statusBar()->showMessage(tr("正在连接..."));
}

void MainWindow::onCoreStopped()
{
    m_isConnected = false;
    m_poller->stopPolling();
    updateButtons();
    m_tray->setStatus(false);
    setStatusIndicator(QStringLiteral("idle"), tr("未连接"));

    // 清空状态卡片
    ui->labelActiveNode->setText(" ");
    ui->labelAssignedIPLarge->setText("-");

    // 清空统计面板
    ui->labelAssignedIP->setText("-");
    ui->labelUptime->setText("-");
    ui->labelTxBytes->setText("-");
    ui->labelRxBytes->setText("-");
    ui->labelTxPackets->setText("-");
    ui->labelRxPackets->setText("-");
    ui->labelDrops->setText("-");
    ui->labelTxRate->setText("0 B/s");
    ui->labelRxRate->setText("0 B/s");
    ui->labelTunName->setText("-");

    // 重置速率历史
    m_hasPrevStats = false;
    m_prevTxBytes  = 0;
    m_prevRxBytes  = 0;
    m_prevStatsMs  = 0;

    appendLog(tr("[GUI] 内核已停止"));
    statusBar()->showMessage(tr("已断开 · 版本 %1").arg(QStringLiteral("1.0.0")));
}

void MainWindow::onCoreError(const QString &error)
{
    appendLog(tr("[错误] %1").arg(error));
    setStatusIndicator(QStringLiteral("error"), tr("出错"));
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
    statusBar()->showMessage(tr("Admin API: 127.0.0.1:%1").arg(port));
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
    // ── 顶部状态徽章 ──
    if (stats.status == "connected") {
        setStatusIndicator(QStringLiteral("connected"), tr("已连接"));
        m_tray->setStatus(true, stats.assignedIPv4);
    } else if (stats.status == "connecting") {
        setStatusIndicator(QStringLiteral("connecting"), tr("正在连接…"));
    } else {
        setStatusIndicator(QStringLiteral("idle"), tr("已断开"));
    }

    // ── 分配 IP（卡片大显示 + 详细行） ──
    QString ip = stats.assignedIPv4;
    if (!stats.assignedIPv6.isEmpty())
        ip += " / " + stats.assignedIPv6;
    QString ipDisplay = ip.isEmpty() ? "-" : ip;
    ui->labelAssignedIP->setText(ipDisplay);
    ui->labelAssignedIPLarge->setText(ipDisplay);

    // ── 实时速率（基于上一次轮询的累计字节数差值） ──
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_hasPrevStats && nowMs > m_prevStatsMs) {
        double dtSec = (nowMs - m_prevStatsMs) / 1000.0;
        // 防御：内核重启或 stats 重置导致字节数倒退，按 0 处理
        quint64 dTx = (stats.txBytes >= m_prevTxBytes) ? (stats.txBytes - m_prevTxBytes) : 0;
        quint64 dRx = (stats.rxBytes >= m_prevRxBytes) ? (stats.rxBytes - m_prevRxBytes) : 0;
        quint64 txRate = static_cast<quint64>(dTx / dtSec);
        quint64 rxRate = static_cast<quint64>(dRx / dtSec);
        ui->labelTxRate->setText(TunnelStats::formatBytes(txRate) + "/s");
        ui->labelRxRate->setText(TunnelStats::formatBytes(rxRate) + "/s");
    }
    m_prevTxBytes  = stats.txBytes;
    m_prevRxBytes  = stats.rxBytes;
    m_prevStatsMs  = nowMs;
    m_hasPrevStats = true;

    // ── 累计 / 包数 / 丢包 / 设备 ──
    ui->labelUptime->setText(stats.uptimeSeconds > 0
        ? TunnelStats::formatUptime(stats.uptimeSeconds) : "-");
    ui->labelTxBytes->setText(TunnelStats::formatBytes(stats.txBytes));
    ui->labelRxBytes->setText(TunnelStats::formatBytes(stats.rxBytes));
    ui->labelTxPackets->setText(QLocale::system().toString(static_cast<qulonglong>(stats.txPackets)));
    ui->labelRxPackets->setText(QLocale::system().toString(static_cast<qulonglong>(stats.rxPackets)));
    ui->labelDrops->setText(QLocale::system().toString(static_cast<qulonglong>(stats.drops)));
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
    // 自动滚动到底部（受用户开关控制）
    bool autoScroll = true;
    if (auto *cb = findChild<QCheckBox*>(QStringLiteral("checkAutoScroll")))
        autoScroll = cb->isChecked();
    if (autoScroll) {
        QScrollBar *sb = ui->textLog->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
}

// ── 状态徽章统一入口 ─────────────────────────────────────────────────────────
void MainWindow::setStatusIndicator(const QString &state, const QString &text)
{
    // 颜色定义（同时适配明/暗主题）：
    //   connected  → 绿  #4caf50
    //   connecting → 橙  #ff9800
    //   error      → 红  #f44336
    //   idle       → 灰  #9e9e9e
    QString dotColor, textColor;
    if (state == "connected")        { dotColor = "#4caf50"; textColor = "#4caf50"; }
    else if (state == "connecting")  { dotColor = "#ff9800"; textColor = "#ff9800"; }
    else if (state == "error")       { dotColor = "#f44336"; textColor = "#f44336"; }
    else                             { dotColor = "#9e9e9e"; textColor = "palette(mid)"; }

    if (auto *dot = findChild<QLabel*>(QStringLiteral("labelStatusDot"))) {
        dot->setStyleSheet(QString(
            "QLabel#labelStatusDot { background-color: %1; border-radius: 7px; }")
            .arg(dotColor));
    }
    if (ui->labelStatus) {
        ui->labelStatus->setText(text);
        ui->labelStatus->setStyleSheet(QString(
            "color: %1; font-size: 22px; font-weight: 600;").arg(textColor));
    }
}

// ── 节点表格右键菜单 ─────────────────────────────────────────────────────────
void MainWindow::onNodeContextMenu(const QPoint &pos)
{
    int row = ui->tableNodes->rowAt(pos.y());
    if (row < 0) return;
    ui->tableNodes->selectRow(row);

    int id = selectedNodeId();
    if (id < 0) return;

    QMenu menu(this);
    QAction *actConnect = menu.addAction(tr("连接"));
    QAction *actEdit    = menu.addAction(tr("编辑"));
    QAction *actCopy    = menu.addAction(tr("复制内核 JSON 配置"));
    menu.addSeparator();
    QAction *actDelete  = menu.addAction(tr("删除"));

    actConnect->setEnabled(!m_isConnected);
    actDelete->setEnabled(!m_isConnected);

    QAction *chosen = menu.exec(ui->tableNodes->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actConnect)        onConnect();
    else if (chosen == actEdit)      onEditNode();
    else if (chosen == actCopy)      onCopyConfigToClipboard();
    else if (chosen == actDelete)    onDeleteNode();
}

void MainWindow::onCopyConfigToClipboard()
{
    int id = selectedNodeId();
    if (id < 0) return;
    TunnelNode n = m_nodeMgr->getNode(id);
    QJsonDocument doc(n.toClientConfig());
    QApplication::clipboard()->setText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    statusBar()->showMessage(tr("已复制节点「%1」的内核 JSON 配置到剪贴板").arg(n.name), 3000);
}

// ── 日志面板按钮 ─────────────────────────────────────────────────────────────
void MainWindow::onLogClear()
{
    ui->textLog->clear();
}

void MainWindow::onLogCopy()
{
    QString text = ui->textLog->toPlainText();
    if (text.isEmpty()) {
        statusBar()->showMessage(tr("日志为空"), 2000);
        return;
    }
    QApplication::clipboard()->setText(text);
    statusBar()->showMessage(tr("已复制 %1 行日志到剪贴板")
        .arg(text.count('\n') + 1), 2000);
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
