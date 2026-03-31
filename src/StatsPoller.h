#pragma once

#include <QObject>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

// 从内核 /api/v1/stats 解析的统计数据
struct TunnelStats {
    QString status;         // "disconnected" / "connecting" / "connected"
    QString assignedIPv4;
    QString assignedIPv6;
    double  uptimeSeconds = 0;
    quint64 txBytes    = 0;
    quint64 rxBytes    = 0;
    quint64 txPackets  = 0;
    quint64 rxPackets  = 0;
    quint64 drops      = 0;
    QString server;
    QString tun;

    // 便捷格式化
    static QString formatBytes(quint64 bytes) {
        if (bytes < 1024)
            return QString("%1 B").arg(bytes);
        if (bytes < 1024 * 1024)
            return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
        if (bytes < 1024 * 1024 * 1024)
            return QString("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 2);
        return QString("%1 GB").arg(bytes / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2);
    }

    static QString formatUptime(double seconds) {
        int s = static_cast<int>(seconds);
        int h = s / 3600;
        int m = (s % 3600) / 60;
        int sec = s % 60;
        if (h > 0)
            return QString("%1:%2:%3")
                .arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
        return QString("%1:%2")
            .arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
    }
};

// StatsPoller 每秒轮询内核 admin HTTP 接口，发射统计更新信号
class StatsPoller : public QObject
{
    Q_OBJECT

public:
    explicit StatsPoller(QObject *parent = nullptr);
    ~StatsPoller();

    void startPolling(quint16 port, int intervalMs = 1000);
    void stopPolling();
    bool isPolling() const { return m_timer->isActive(); }

    const TunnelStats &lastStats() const { return m_lastStats; }

signals:
    void statsUpdated(const TunnelStats &stats);
    void pollError(const QString &error);

private slots:
    void poll();
    void onReplyFinished(QNetworkReply *reply);

private:
    QTimer                *m_timer   = nullptr;
    QNetworkAccessManager *m_nam     = nullptr;
    QNetworkReply         *m_pending = nullptr;
    quint16                m_port    = 0;
    TunnelStats            m_lastStats;
};
