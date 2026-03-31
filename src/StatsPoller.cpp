#include "StatsPoller.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

StatsPoller::StatsPoller(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_nam(new QNetworkAccessManager(this))
{
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &StatsPoller::poll);
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &StatsPoller::onReplyFinished);
}

StatsPoller::~StatsPoller()
{
    stopPolling();
}

void StatsPoller::startPolling(quint16 port, int intervalMs)
{
    m_port = port;
    m_lastStats = TunnelStats{};
    m_timer->start(intervalMs);
    // 立即轮询一次
    poll();
}

void StatsPoller::stopPolling()
{
    m_timer->stop();
    if (m_pending) {
        m_pending->abort();
        m_pending = nullptr;
    }
    m_lastStats = TunnelStats{};
}

void StatsPoller::poll()
{
    if (m_pending)  // 上一次请求还未完成，跳过
        return;

    QUrl url(QString("http://127.0.0.1:%1/api/v1/stats").arg(m_port));
    QNetworkRequest req(url);
    req.setTransferTimeout(800); // 800ms 超时，小于轮询间隔
    m_pending = m_nam->get(req);
}

void StatsPoller::onReplyFinished(QNetworkReply *reply)
{
    if (reply != m_pending) {
        reply->deleteLater();
        return;
    }
    m_pending = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        emit pollError(reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        emit pollError("JSON 解析错误: " + parseErr.errorString());
        return;
    }

    QJsonObject obj = doc.object();
    TunnelStats s;
    s.status        = obj["status"].toString();
    s.assignedIPv4  = obj["assigned_ipv4"].toString();
    s.assignedIPv6  = obj["assigned_ipv6"].toString();
    s.uptimeSeconds = obj["uptime_seconds"].toDouble();
    s.txBytes       = static_cast<quint64>(obj["tx_bytes"].toDouble());
    s.rxBytes       = static_cast<quint64>(obj["rx_bytes"].toDouble());
    s.txPackets     = static_cast<quint64>(obj["tx_packets"].toDouble());
    s.rxPackets     = static_cast<quint64>(obj["rx_packets"].toDouble());
    s.drops         = static_cast<quint64>(obj["drops"].toDouble());
    s.server        = obj["server"].toString();
    s.tun           = obj["tun"].toString();

    m_lastStats = s;
    emit statsUpdated(s);
}
