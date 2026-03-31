#include "EditNodeDialog.h"
#include "ui_EditNodeDialog.h"

EditNodeDialog::EditNodeDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EditNodeDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("编辑节点"));

    connect(ui->comboAuthMethod, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EditNodeDialog::onAuthMethodChanged);
    connect(ui->checkEnableECH, &QCheckBox::toggled,
            this, &EditNodeDialog::onECHToggled);

    updateVisibility();
}

EditNodeDialog::~EditNodeDialog()
{
    delete ui;
}

void EditNodeDialog::setNode(const TunnelNode &node)
{
    m_node = node;

    ui->editName->setText(node.name);
    ui->editServerAddr->setText(node.serverAddr);
    ui->editURI->setText(node.uri);
    ui->editAuthority->setText(node.authority);

    ui->comboAuthMethod->setCurrentIndex(static_cast<int>(node.authMethod));
    ui->editBearerToken->setText(node.bearerToken);
    ui->editBasicUser->setText(node.basicUser);
    ui->editBasicPassword->setText(node.basicPassword);
    ui->editCustomHeaderName->setText(node.customHeaderName);
    ui->editCustomHeaderValue->setText(node.customHeaderValue);

    ui->editServerName->setText(node.serverName);
    ui->checkInsecureSkipVerify->setChecked(node.insecureSkipVerify);
    ui->checkEnablePQC->setChecked(node.enablePQC);
    ui->checkUseMozillaCA->setChecked(node.useMozillaCA);
    ui->checkUseSystemCAs->setChecked(node.useSystemCAs);
    ui->checkEnableECH->setChecked(node.enableECH);
    ui->editECHDomain->setText(node.echDomain);
    ui->editECHDohServer->setText(node.echDohServer);

    ui->editTunName->setText(node.tunName);
    ui->spinMTU->setValue(node.mtu);
    ui->editIPv4CIDR->setText(node.ipv4CIDR);
    ui->editIPv6CIDR->setText(node.ipv6CIDR);
    ui->editDnsV4->setText(node.dnsV4);
    ui->editDnsV6->setText(node.dnsV6);

    ui->checkBypassEnable->setChecked(node.bypassEnable);
    ui->editBypassAddr->setText(node.bypassAddr);

    ui->checkAllow0RTT->setChecked(node.allow0RTT);
    ui->checkEnableReconnect->setChecked(node.enableReconnect);
    ui->spinNumSessions->setValue(node.numSessions);

    updateVisibility();
}

TunnelNode EditNodeDialog::getNode() const
{
    TunnelNode n = m_node;

    n.name        = ui->editName->text().trimmed();
    n.serverAddr  = ui->editServerAddr->text().trimmed();
    n.uri         = ui->editURI->text().trimmed();
    n.authority   = ui->editAuthority->text().trimmed();

    n.authMethod         = static_cast<TunnelNode::AuthMethod>(ui->comboAuthMethod->currentIndex());
    n.bearerToken        = ui->editBearerToken->text().trimmed();
    n.basicUser          = ui->editBasicUser->text().trimmed();
    n.basicPassword      = ui->editBasicPassword->text();
    n.customHeaderName   = ui->editCustomHeaderName->text().trimmed();
    n.customHeaderValue  = ui->editCustomHeaderValue->text().trimmed();

    n.serverName         = ui->editServerName->text().trimmed();
    n.insecureSkipVerify = ui->checkInsecureSkipVerify->isChecked();
    n.enablePQC          = ui->checkEnablePQC->isChecked();
    n.useMozillaCA       = ui->checkUseMozillaCA->isChecked();
    n.useSystemCAs       = ui->checkUseSystemCAs->isChecked();
    n.enableECH          = ui->checkEnableECH->isChecked();
    n.echDomain          = ui->editECHDomain->text().trimmed();
    n.echDohServer       = ui->editECHDohServer->text().trimmed();

    n.tunName    = ui->editTunName->text().trimmed();
    n.mtu        = ui->spinMTU->value();
    n.ipv4CIDR   = ui->editIPv4CIDR->text().trimmed();
    n.ipv6CIDR   = ui->editIPv6CIDR->text().trimmed();
    n.dnsV4      = ui->editDnsV4->text().trimmed();
    n.dnsV6      = ui->editDnsV6->text().trimmed();

    n.bypassEnable = ui->checkBypassEnable->isChecked();
    n.bypassAddr   = ui->editBypassAddr->text().trimmed();

    n.allow0RTT       = ui->checkAllow0RTT->isChecked();
    n.enableReconnect = ui->checkEnableReconnect->isChecked();
    n.numSessions     = ui->spinNumSessions->value();

    return n;
}

void EditNodeDialog::onAuthMethodChanged(int index)
{
    updateVisibility();
}

void EditNodeDialog::onECHToggled(bool checked)
{
    updateVisibility();
}

void EditNodeDialog::updateVisibility()
{
    int authIdx = ui->comboAuthMethod->currentIndex();
    ui->groupBearer->setVisible(authIdx == 0);
    ui->groupBasic->setVisible(authIdx == 1);
    ui->groupCustomHeader->setVisible(authIdx == 2);

    bool echOn = ui->checkEnableECH->isChecked();
    ui->editECHDomain->setEnabled(echOn);
    ui->editECHDohServer->setEnabled(echOn);
}
