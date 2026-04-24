#include "EditNodeDialog.h"
#include "ui_EditNodeDialog.h"

#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace {
// 把指定 tab 的内容用一个 QScrollArea 包起来，
// 解决"高级"Tab 在 1080p 以下分辨率被裁剪、无法滚动的问题。
// 不改 .ui 结构，原有的 ui->xxx / findChild 引用全部继续有效。
void wrapTabInScrollArea(QTabWidget *tabs, QWidget *innerTab)
{
    if (!tabs || !innerTab) return;
    int idx = tabs->indexOf(innerTab);
    if (idx < 0) return;

    QString title = tabs->tabText(idx);
    QString tt    = tabs->tabToolTip(idx);

    // 1. 临时移除 tab（不删除 widget）
    tabs->removeTab(idx);

    // 2. 创建外层 page + ScrollArea
    auto *page = new QWidget(tabs);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(page);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 3. 把原 tab widget 作为 scroll area 的内容
    innerTab->setParent(nullptr); // 解除原 tabs 的所有权
    scroll->setWidget(innerTab);

    outer->addWidget(scroll);

    // 4. 插回原位
    tabs->insertTab(idx, page, title);
    if (!tt.isEmpty())
        tabs->setTabToolTip(idx, tt);
}
} // namespace

EditNodeDialog::EditNodeDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EditNodeDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("编辑节点"));

    // 适配低分屏：dialog 最小尺寸放宽，让 QScrollArea 起作用
    setMinimumSize(560, 480);
    resize(680, 700);

    // 把"高级"和"TLS / ECH"两个项目较多的 tab 包进 QScrollArea，
    // 1080p 以下也能完整滚动浏览，不会被裁剪。
    if (auto *tabs = ui->tabWidget) {
        wrapTabInScrollArea(tabs, ui->tabAdvanced);
        wrapTabInScrollArea(tabs, ui->tabTLS);
    }

    // 平台相关字段可见性调整：
    // - GSO/GRO（UDP 分段卸载）只有 Linux 内核支持，且当前内核实现会在 ApplyDefaults
    //   阶段强制把 EnableGSO 写回 true（quic-go 内部自动按平台能力降级），
    //   所以在非 Linux 平台上把开关隐藏，避免误导用户。
    // - UDP socket buffer 在 Windows/macOS 平台上系统会静默 cap 到很小的上限，
    //   配置 16-64MB 实际可能只生效几 MB，给字段加注释提示。
#ifndef Q_OS_LINUX
    if (auto *check = findChild<QCheckBox*>(QStringLiteral("checkEnableGSO"))) {
        check->setEnabled(false);
        check->setToolTip(tr("GSO/GRO 仅在 Linux 内核生效，当前平台已自动降级，无需配置。"));
        if (auto *form = qobject_cast<QFormLayout*>(check->parentWidget()->layout())) {
            if (auto *lbl = qobject_cast<QLabel*>(form->labelForField(check)))
                lbl->setText(tr("启用 GSO/GRO（仅 Linux）"));
        }
    }
    auto annotateLinuxOnly = [this](const QString &spinName, const QString &hint) {
        auto *spin = findChild<QSpinBox*>(spinName);
        if (!spin) return;
        QString old = spin->toolTip();
        spin->setToolTip(old + (old.isEmpty() ? QString() : QStringLiteral("\n\n")) + hint);
    };
    const QString udpHint = tr("注意：Windows / macOS 等非 Linux 平台对 SO_RCVBUF/SO_SNDBUF "
                               "有较低的系统级上限，过大的值会被内核静默 cap。\n"
                               "如需提升，请在系统级调优 net.core.rmem_max 等参数。");
    annotateLinuxOnly(QStringLiteral("spinUDPRecvBuffer"), udpHint);
    annotateLinuxOnly(QStringLiteral("spinUDPSendBuffer"), udpHint);
#endif

    connect(ui->checkEnableECH, &QCheckBox::toggled,
            this, &EditNodeDialog::onECHToggled);
    connect(ui->checkWaitForAddressAssign, &QCheckBox::toggled,
            this, &EditNodeDialog::onWaitForAddressAssignToggled);
    connect(ui->btnBrowseCert, &QPushButton::clicked,
            this, &EditNodeDialog::onBrowseCert);
    connect(ui->btnBrowseKey, &QPushButton::clicked,
            this, &EditNodeDialog::onBrowseKey);
    if (auto *btn = findChild<QPushButton*>(QStringLiteral("btnBrowseServerCA")))
        connect(btn, &QPushButton::clicked,
                this, &EditNodeDialog::onBrowseServerCA);

    updateECHVisibility();
    updateAddressAssignVisibility();
}

EditNodeDialog::~EditNodeDialog()
{
    delete ui;
}

void EditNodeDialog::setNode(const TunnelNode &node)
{
    m_node = node;

    // 基本
    ui->editName->setText(node.name);
    ui->editServerAddr->setText(node.serverAddr);
    ui->editURI->setText(node.uri);
    ui->editAuthority->setText(node.authority);

    // mTLS
    ui->editClientCertFile->setText(node.clientCertFile);
    ui->editClientKeyFile->setText(node.clientKeyFile);

    // TLS/ECH
    ui->editServerName->setText(node.serverName);
    // 注：insecureSkipVerify 字段已移除（内核拒绝该字段），UI 控件已删除
    ui->checkEnablePQC->setChecked(node.enablePQC);
    ui->checkUseMozillaCA->setChecked(node.useMozillaCA);
    ui->checkUseSystemCAs->setChecked(node.useSystemCAs);
    if (auto *e = findChild<QLineEdit*>(QStringLiteral("editServerCAFile")))
        e->setText(node.serverCAFile);
    ui->checkEnableECH->setChecked(node.enableECH);
    ui->editECHDomain->setText(node.echDomain);
    ui->editECHDohServer->setText(node.echDohServer);
    ui->editECHConfigListB64->setText(node.echConfigListB64);
    // 地址族偏好（auto=0, v4=1, v6=2）
    if (auto *combo = findChild<QComboBox*>(QStringLiteral("comboPreferAddressFamily"))) {
        int idx = 0;
        if (node.preferAddressFamily == "v4") idx = 1;
        else if (node.preferAddressFamily == "v6") idx = 2;
        combo->setCurrentIndex(idx);
    }
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinHappyEyeballsDelay")))
        spin->setValue(node.happyEyeballsDelayMs);

    // TUN
    ui->editTunName->setText(node.tunName);
    ui->spinMTU->setValue(node.mtu);
    ui->editDnsV4->setText(node.dnsV4);
    ui->editDnsV6->setText(node.dnsV6);
    ui->checkWaitForAddressAssign->setChecked(node.waitForAddressAssign);
    ui->editIPv4CIDR->setText(node.ipv4CIDR);
    ui->editIPv6CIDR->setText(node.ipv6CIDR);

    // Obfs（UDP 包级混淆）
    if (auto *comboObfsType = findChild<QComboBox*>(QStringLiteral("comboObfsType"))) {
        int index = 0;
        if (node.obfsType == "salamander") index = 1;
        comboObfsType->setCurrentIndex(index);
    }
    if (auto *editObfsPassword = findChild<QLineEdit*>(QStringLiteral("editObfsPassword")))
        editObfsPassword->setText(node.obfsPassword);
    
    // 高级
    ui->checkAllow0RTT->setChecked(node.allow0RTT);
    ui->checkEnableReconnect->setChecked(node.enableReconnect);
    ui->spinNumSessions->setValue(node.numSessions);
    ui->checkEnableSessionCache->setChecked(node.enableSessionCache);
    ui->spinSessionCacheSize->setValue(node.sessionCacheSize);
    ui->spinMaxIdleTimeout->setValue(node.maxIdleTimeoutSec);
    ui->spinKeepAlivePeriod->setValue(node.keepAlivePeriodSec);
    ui->spinAddressAssignTimeout->setValue(node.addressAssignTimeoutSec);
    ui->spinMaxReconnectDelay->setValue(node.maxReconnectDelaySec);

    // Bypass
    ui->checkEnableBypass->setChecked(node.enableBypass);
    if (auto *c = findChild<QCheckBox*>(QStringLiteral("checkBypassStrict")))
        c->setChecked(node.bypassStrict);
    // 拥塞控制：bbr2=index0, cubic=index1
    ui->comboCongestionAlgo->setCurrentIndex(node.congestionAlgo == "cubic" ? 1 : 0);

    // BBRv2 子参数（0/空 = 内核默认，UI 用 specialValueText 显示）
    if (auto *s = findChild<QDoubleSpinBox*>(QStringLiteral("spinBBR2LossThreshold")))
        s->setValue(node.bbr2LossThreshold);
    if (auto *s = findChild<QDoubleSpinBox*>(QStringLiteral("spinBBR2Beta")))
        s->setValue(node.bbr2Beta);
    if (auto *s = findChild<QSpinBox*>(QStringLiteral("spinBBR2StartupFullBwRounds")))
        s->setValue(node.bbr2StartupFullBwRounds);
    if (auto *s = findChild<QSpinBox*>(QStringLiteral("spinBBR2ProbeRTTPeriod")))
        s->setValue(node.bbr2ProbeRTTPeriodSec);
    if (auto *s = findChild<QSpinBox*>(QStringLiteral("spinBBR2ProbeRTTDuration")))
        s->setValue(node.bbr2ProbeRTTDurationMs);
    if (auto *combo = findChild<QComboBox*>(QStringLiteral("comboBBR2BwLoReduction"))) {
        // index 0 = 内核默认（留空），1=default, 2=minrtt, 3=inflight, 4=cwnd
        int idx = 0;
        if (node.bbr2BwLoReduction == "default")  idx = 1;
        else if (node.bbr2BwLoReduction == "minrtt")   idx = 2;
        else if (node.bbr2BwLoReduction == "inflight") idx = 3;
        else if (node.bbr2BwLoReduction == "cwnd")     idx = 4;
        combo->setCurrentIndex(idx);
    }
    if (auto *c = findChild<QCheckBox*>(QStringLiteral("checkBBR2Aggressive")))
        c->setChecked(node.bbr2Aggressive);

    // 管理 / 调试 API
    if (auto *e = findChild<QLineEdit*>(QStringLiteral("editAdminListen")))
        e->setText(node.adminListen);
    if (auto *e = findChild<QLineEdit*>(QStringLiteral("editAdminToken")))
        e->setText(node.adminToken);
    if (auto *c = findChild<QCheckBox*>(QStringLiteral("checkEnablePprof")))
        c->setChecked(node.enablePprof);

    // HTTP3 窗口调优
    ui->checkDisablePathMTUProbe->setChecked(node.disablePathMTUProbe);
    ui->spinInitialStreamWindow->setValue(static_cast<int>(node.initialStreamWindow));
    ui->spinMaxStreamWindow->setValue(static_cast<int>(node.maxStreamWindow));
    ui->spinInitialConnWindow->setValue(static_cast<int>(node.initialConnWindow));
    ui->spinMaxConnWindow->setValue(static_cast<int>(node.maxConnWindow));
    // 注：原 disableCompression / tlsHandshakeTimeoutSec / maxResponseHeaderSec
    // 三个字段已随内核同步移除（CONNECT-IP 不走常规 HTTP 路径，这些选项无作用）。

    // 应用层心跳（CONNECT-IP capsule ping/pong）
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinAppKeepalivePeriod")))
        spin->setValue(node.appKeepalivePeriodSec);
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinAppKeepaliveTimeout")))
        spin->setValue(node.appKeepaliveTimeoutSec);
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinUnhealthyThreshold")))
        spin->setValue(node.unhealthyThreshold);
    if (auto *check = findChild<QCheckBox*>(QStringLiteral("checkPerSessionReconnect")))
        check->setChecked(node.perSessionReconnect);

    // UDP socket buffer & GSO
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinUDPRecvBuffer")))
        spin->setValue(static_cast<int>(node.udpRecvBuffer));
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinUDPSendBuffer")))
        spin->setValue(static_cast<int>(node.udpSendBuffer));
    if (auto *check = findChild<QCheckBox*>(QStringLiteral("checkEnableGSO")))
        check->setChecked(node.enableGSO);

    // 调试
    ui->editKeyLogPath->setText(node.keyLogPath);

    updateECHVisibility();
    updateAddressAssignVisibility();
}

TunnelNode EditNodeDialog::getNode() const
{
    TunnelNode n = m_node;

    // 基本
    n.name       = ui->editName->text().trimmed();
    n.serverAddr = ui->editServerAddr->text().trimmed();
    n.uri        = ui->editURI->text().trimmed();
    n.authority  = ui->editAuthority->text().trimmed();

    // mTLS
    n.clientCertFile = ui->editClientCertFile->text().trimmed();
    n.clientKeyFile  = ui->editClientKeyFile->text().trimmed();

    // TLS/ECH
    n.serverName         = ui->editServerName->text().trimmed();
    // insecureSkipVerify 字段已移除：内核拒绝该字段，GUI 也不再读取
    n.enablePQC          = ui->checkEnablePQC->isChecked();
    n.useMozillaCA       = ui->checkUseMozillaCA->isChecked();
    n.useSystemCAs       = ui->checkUseSystemCAs->isChecked();
    if (auto *e = findChild<QLineEdit*>(QStringLiteral("editServerCAFile")))
        n.serverCAFile = e->text().trimmed();
    n.enableECH          = ui->checkEnableECH->isChecked();
    n.echDomain          = ui->editECHDomain->text().trimmed();
    n.echDohServer       = ui->editECHDohServer->text().trimmed();
    n.echConfigListB64   = ui->editECHConfigListB64->text().trimmed();
    // 地址族偏好
    if (auto *combo = findChild<QComboBox*>(QStringLiteral("comboPreferAddressFamily"))) {
        switch (combo->currentIndex()) {
        case 1:  n.preferAddressFamily = "v4"; break;
        case 2:  n.preferAddressFamily = "v6"; break;
        default: n.preferAddressFamily = "auto"; break;
        }
    }
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinHappyEyeballsDelay")))
        n.happyEyeballsDelayMs = spin->value();

    // TUN
    n.tunName = ui->editTunName->text().trimmed();
    n.mtu     = ui->spinMTU->value();
    n.dnsV4   = ui->editDnsV4->text().trimmed();
    n.dnsV6   = ui->editDnsV6->text().trimmed();
    n.waitForAddressAssign = ui->checkWaitForAddressAssign->isChecked();
    n.ipv4CIDR = ui->editIPv4CIDR->text().trimmed();
    n.ipv6CIDR = ui->editIPv6CIDR->text().trimmed();

    // 高级
    n.allow0RTT       = ui->checkAllow0RTT->isChecked();
    n.enableReconnect = ui->checkEnableReconnect->isChecked();
    n.numSessions     = ui->spinNumSessions->value();
    n.enableSessionCache = ui->checkEnableSessionCache->isChecked();
    n.sessionCacheSize   = ui->spinSessionCacheSize->value();
    n.maxIdleTimeoutSec  = ui->spinMaxIdleTimeout->value();
    n.keepAlivePeriodSec = ui->spinKeepAlivePeriod->value();
    n.addressAssignTimeoutSec = ui->spinAddressAssignTimeout->value();
    n.maxReconnectDelaySec    = ui->spinMaxReconnectDelay->value();

    // Obfs（UDP 包级混淆）
    if (auto *comboObfsType = findChild<QComboBox*>(QStringLiteral("comboObfsType"))) {
        n.obfsType = (comboObfsType->currentIndex() == 1) ? "salamander" : "";
    }
    if (auto *editObfsPassword = findChild<QLineEdit*>(QStringLiteral("editObfsPassword")))
        n.obfsPassword = editObfsPassword->text().trimmed();
    
    // Bypass
    n.enableBypass = ui->checkEnableBypass->isChecked();
    if (auto *c = findChild<QCheckBox*>(QStringLiteral("checkBypassStrict")))
        n.bypassStrict = c->isChecked();
    n.congestionAlgo = (ui->comboCongestionAlgo->currentIndex() == 1) ? "cubic" : "bbr2";

    // BBRv2 子参数
    if (auto *s = findChild<QDoubleSpinBox*>(QStringLiteral("spinBBR2LossThreshold")))
        n.bbr2LossThreshold = s->value();
    if (auto *s = findChild<QDoubleSpinBox*>(QStringLiteral("spinBBR2Beta")))
        n.bbr2Beta = s->value();
    if (auto *s = findChild<QSpinBox*>(QStringLiteral("spinBBR2StartupFullBwRounds")))
        n.bbr2StartupFullBwRounds = s->value();
    if (auto *s = findChild<QSpinBox*>(QStringLiteral("spinBBR2ProbeRTTPeriod")))
        n.bbr2ProbeRTTPeriodSec = s->value();
    if (auto *s = findChild<QSpinBox*>(QStringLiteral("spinBBR2ProbeRTTDuration")))
        n.bbr2ProbeRTTDurationMs = s->value();
    if (auto *combo = findChild<QComboBox*>(QStringLiteral("comboBBR2BwLoReduction"))) {
        switch (combo->currentIndex()) {
        case 1: n.bbr2BwLoReduction = "default"; break;
        case 2: n.bbr2BwLoReduction = "minrtt"; break;
        case 3: n.bbr2BwLoReduction = "inflight"; break;
        case 4: n.bbr2BwLoReduction = "cwnd"; break;
        default: n.bbr2BwLoReduction = ""; break; // 内核默认
        }
    }
    if (auto *c = findChild<QCheckBox*>(QStringLiteral("checkBBR2Aggressive")))
        n.bbr2Aggressive = c->isChecked();

    // 管理 / 调试 API
    if (auto *e = findChild<QLineEdit*>(QStringLiteral("editAdminListen")))
        n.adminListen = e->text().trimmed();
    if (auto *e = findChild<QLineEdit*>(QStringLiteral("editAdminToken")))
        n.adminToken = e->text();  // token 保留前后空格也无意义，但避免改密码语义
    if (auto *c = findChild<QCheckBox*>(QStringLiteral("checkEnablePprof")))
        n.enablePprof = c->isChecked();

    // HTTP3 窗口调优
    n.disablePathMTUProbe    = ui->checkDisablePathMTUProbe->isChecked();
    n.initialStreamWindow    = ui->spinInitialStreamWindow->value();
    n.maxStreamWindow        = ui->spinMaxStreamWindow->value();
    n.initialConnWindow      = ui->spinInitialConnWindow->value();
    n.maxConnWindow          = ui->spinMaxConnWindow->value();
    // 应用层心跳
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinAppKeepalivePeriod")))
        n.appKeepalivePeriodSec = spin->value();
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinAppKeepaliveTimeout")))
        n.appKeepaliveTimeoutSec = spin->value();
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinUnhealthyThreshold")))
        n.unhealthyThreshold = spin->value();
    if (auto *check = findChild<QCheckBox*>(QStringLiteral("checkPerSessionReconnect")))
        n.perSessionReconnect = check->isChecked();

    // UDP socket buffer & GSO
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinUDPRecvBuffer")))
        n.udpRecvBuffer = spin->value();
    if (auto *spin = findChild<QSpinBox*>(QStringLiteral("spinUDPSendBuffer")))
        n.udpSendBuffer = spin->value();
    if (auto *check = findChild<QCheckBox*>(QStringLiteral("checkEnableGSO")))
        n.enableGSO = check->isChecked();

    // 调试
    n.keyLogPath = ui->editKeyLogPath->text().trimmed();

    return n;
}

void EditNodeDialog::onECHToggled(bool /*checked*/)
{
    updateECHVisibility();
}

void EditNodeDialog::onWaitForAddressAssignToggled(bool /*checked*/)
{
    updateAddressAssignVisibility();
}

void EditNodeDialog::onBrowseCert()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("选择客户端证书"),
        {},
        tr("PEM 证书 (*.crt *.pem *.cer);;所有文件 (*)")
    );
    if (!path.isEmpty())
        ui->editClientCertFile->setText(path);
}

void EditNodeDialog::onBrowseKey()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("选择客户端私钥"),
        {},
        tr("PEM 私钥 (*.key *.pem);;所有文件 (*)")
    );
    if (!path.isEmpty())
        ui->editClientKeyFile->setText(path);
}

void EditNodeDialog::onBrowseServerCA()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("选择服务端 CA 证书"),
        {},
        tr("PEM 证书 (*.crt *.pem *.cer);;所有文件 (*)")
    );
    if (!path.isEmpty()) {
        if (auto *e = findChild<QLineEdit*>(QStringLiteral("editServerCAFile")))
            e->setText(path);
    }
}

void EditNodeDialog::updateECHVisibility()
{
    bool echOn = ui->checkEnableECH->isChecked();
    ui->editECHDomain->setEnabled(echOn);
    ui->editECHDohServer->setEnabled(echOn);
}

void EditNodeDialog::updateAddressAssignVisibility()
{
    bool waitMode = ui->checkWaitForAddressAssign->isChecked();
    // 静态 IP 字段仅在不等待服务端分配时可编辑
    ui->editIPv4CIDR->setEnabled(!waitMode);
    ui->editIPv6CIDR->setEnabled(!waitMode);
    // labelStaticIPHint / labelIPv4CIDR / labelIPv6CIDR 在 .ui 中
    // 可能未被 uic 生成为成员（QFormLayout 中的 QLabel 需通过 findChild 访问）
    if (auto *hint = findChild<QLabel*>(QStringLiteral("labelStaticIPHint")))
        hint->setVisible(!waitMode);
    if (auto *lv4 = findChild<QLabel*>(QStringLiteral("labelIPv4CIDR")))
        lv4->setEnabled(!waitMode);
    if (auto *lv6 = findChild<QLabel*>(QStringLiteral("labelIPv6CIDR")))
        lv6->setEnabled(!waitMode);
}
