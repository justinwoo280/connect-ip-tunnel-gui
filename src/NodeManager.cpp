#include "NodeManager.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

NodeManager::NodeManager(QObject *parent)
    : QObject(parent)
{
}

void NodeManager::load()
{
    QFile f(dataFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    m_nodes.clear();
    for (const auto &val : doc.array())
        m_nodes.append(TunnelNode::fromJson(val.toObject()));

    emit nodesChanged();
}

void NodeManager::save()
{
    QJsonArray arr;
    for (const auto &n : m_nodes)
        arr.append(n.toJson());

    QFileInfo fi(dataFilePath());
    fi.dir().mkpath(".");

    QFile f(dataFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void NodeManager::addNode(const TunnelNode &node)
{
    TunnelNode n = node;
    n.id = nextId();
    m_nodes.append(n);
    save();
    emit nodesChanged();
}

void NodeManager::updateNode(const TunnelNode &node)
{
    for (auto &n : m_nodes) {
        if (n.id == node.id) {
            n = node;
            save();
            emit nodesChanged();
            return;
        }
    }
}

void NodeManager::removeNode(int id)
{
    m_nodes.removeIf([id](const TunnelNode &n) { return n.id == id; });
    save();
    emit nodesChanged();
}

TunnelNode NodeManager::getNode(int id) const
{
    for (const auto &n : m_nodes)
        if (n.id == id) return n;
    return {};
}

QString NodeManager::dataFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/nodes.json";
}

int NodeManager::nextId() const
{
    int maxId = 0;
    for (const auto &n : m_nodes)
        if (n.id > maxId) maxId = n.id;
    return maxId + 1;
}
