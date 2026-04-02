#include "EditNodeDialog.h"
#include "ui_EditNodeDialog.h"

#include <QFileDialog>
#include <QLabel>

EditNodeDialog::EditNodeDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EditNodeDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("编辑节点"));

    connect(ui->checkEnableECH, &QCheckBox::toggled,
            this, &EditNodeDialog::onECHToggled);
    connect(ui->checkWaitForAddressAssign, &QCheckBox::toggled,
            this, &EditNodeDialog::onWaitForAddressAssignToggled);
    connect(ui->btnBrowseCert, &QPushButton::clicked,
            this, &EditNodeDialog::onBrowseCert);
    connect(ui->btnBrowseKey, &QPushButton::clicked,
            this, &EditNodeDialog::onBrowseKey);

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
    ui->checkInsecureSkipVerify->setChecked(node.insecureSkipVerify);
    ui->checkEnablePQC->setChecked(node.enablePQC);
    ui->checkUseMozillaCA->setChecked(node.useMozillaCA);
    ui->checkUseSystemCAs->setChecked(node.useSystemCAs);
    ui->checkEnableECH->setChecked(node.enableECH);
    ui->editECHDomain->setText(node.echDomain);
    ui->editECHDohServer->setText(node.echDohServer);

    // TUN
    ui->editTunName->setText(node.tunName);
    ui->spinMTU->setValue(node.mtu);
    ui->editDnsV4->setText(node.dnsV4);
    ui->editDnsV6->setText(node.dnsV6);
    ui->checkWaitForAddressAssign->setChecked(node.waitForAddressAssign);
    ui->editIPv4CIDR->setText(node.ipv4CIDR);
    ui->editIPv6CIDR->setText(node.ipv6CIDR);

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

    // HTTP3 窗口调优
    ui->checkDisablePathMTUProbe->setChecked(node.disablePathMTUProbe);
    ui->spinInitialStreamWindow->setValue(static_cast<int>(node.initialStreamWindow));
    ui->spinMaxStreamWindow->setValue(static_cast<int>(node.maxStreamWindow));
    ui->spinInitialConnWindow->setValue(static_cast<int>(node.initialConnWindow));
    ui->spinMaxConnWindow->setValue(static_cast<int>(node.maxConnWindow));

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
    n.insecureSkipVerify = ui->checkInsecureSkipVerify->isChecked();
    n.enablePQC          = ui->checkEnablePQC->isChecked();
    n.useMozillaCA       = ui->checkUseMozillaCA->isChecked();
    n.useSystemCAs       = ui->checkUseSystemCAs->isChecked();
    n.enableECH          = ui->checkEnableECH->isChecked();
    n.echDomain          = ui->editECHDomain->text().trimmed();
    n.echDohServer       = ui->editECHDohServer->text().trimmed();

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

    // Bypass
    n.enableBypass = ui->checkEnableBypass->isChecked();

    // HTTP3 窗口调优
    n.disablePathMTUProbe = ui->checkDisablePathMTUProbe->isChecked();
    n.initialStreamWindow = ui->spinInitialStreamWindow->value();
    n.maxStreamWindow     = ui->spinMaxStreamWindow->value();
    n.initialConnWindow   = ui->spinInitialConnWindow->value();
    n.maxConnWindow       = ui->spinMaxConnWindow->value();

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
