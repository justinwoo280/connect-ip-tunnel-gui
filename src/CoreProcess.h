#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTemporaryFile>
#include "TunnelNode.h"

// CoreProcess 负责启动/停止 connect-ip-tunnel 内核进程
// 工作流：
//   1. 将 TunnelNode 序列化为 JSON 写入临时文件
//   2. 以 -c <tempfile> 启动内核进程
//   3. 解析 stdout 中的 admin server 端口
//   4. 发射 adminPortReady 信号供 StatsPoller 使用
class CoreProcess : public QObject
{
    Q_OBJECT

public:
    explicit CoreProcess(QObject *parent = nullptr);
    ~CoreProcess();

    bool start(const TunnelNode &node);
    void stop();
    bool isRunning() const;

    QString lastError() const { return m_lastError; }

signals:
    void started();
    void stopped();
    void errorOccurred(const QString &error);
    void logReceived(const QString &message);
    // 内核 admin HTTP server 端口就绪（从 stdout 解析）
    void adminPortReady(quint16 port);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onReadyRead();

private:
    QString findCoreExecutable() const;
    void cleanupTempFile();

    QProcess      *m_process    = nullptr;
    QString        m_lastError;
    QString        m_tempConfig; // 临时 config 文件路径
    bool           m_portEmitted = false; // 只发射一次 adminPortReady
    bool           m_stopping   = false;  // 标记为主动停止，避免误报崩溃
};
