#pragma once

#include <QDialog>
#include "TunnelNode.h"

QT_BEGIN_NAMESPACE
namespace Ui { class EditNodeDialog; }
QT_END_NAMESPACE

class EditNodeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditNodeDialog(QWidget *parent = nullptr);
    ~EditNodeDialog();

    void setNode(const TunnelNode &node);
    TunnelNode getNode() const;

private slots:
    void onAuthMethodChanged(int index);
    void onECHToggled(bool checked);

private:
    void updateVisibility();

    Ui::EditNodeDialog *ui;
    TunnelNode m_node;
};
