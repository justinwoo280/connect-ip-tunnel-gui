#include "EditNodeDialog.h"
#include "ui_EditNodeDialog.h"

#include <QFileDialog>

EditNodeDialog::EditNodeDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EditNodeDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("编辑节点"));

    connect(ui->checkEnableECH, &QCheckBox::toggled,
            this, &EditNodeDialog::onECHToggled);
    connect(ui->btnBrowseCert, &QPushButton::clicked,
            this, &EditNodeDialog::onBrowseCert);
    connect(ui->btnBrowseKey, &QPushButton::clicked,
            this, &EditNodeDialog::onBrowseKey);

    updateECHVisibility();
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

    updateECHVisibility();
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

    return n;
}

void EditNodeDialog::onECHToggled(bool /*checked*/)
{
    updateECHVisibility();
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
