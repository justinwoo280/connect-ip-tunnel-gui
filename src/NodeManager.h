#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include "TunnelNode.h"

// NodeManager 负责节点列表的内存管理和 JSON 持久化
class NodeManager : public QObject
{
    Q_OBJECT

public:
    explicit NodeManager(QObject *parent = nullptr);

    // 加载 / 保存
    void load();
    void save();

    // CRUD
    void addNode(const TunnelNode &node);
    void updateNode(const TunnelNode &node);
    void removeNode(int id);
    TunnelNode getNode(int id) const;
    const QList<TunnelNode> &nodes() const { return m_nodes; }

signals:
    void nodesChanged();

private:
    QString dataFilePath() const;
    int nextId() const;

    QList<TunnelNode> m_nodes;
};
