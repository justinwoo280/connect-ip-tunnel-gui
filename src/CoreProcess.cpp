#include "CoreProcess.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

CoreProcess::CoreProcess(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, &QProcess::started,
            this, &CoreProcess::onProcessStarted);
    connect(m_process, &QProcess::errorOccurred,
            this, &CoreProcess::onProcessError);
    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CoreProcess::onProcessFinished);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &CoreProcess::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &CoreProcess::onReadyRead);
}

CoreProcess::~CoreProcess()
{
    stop();
    cleanupTempFile();
}

bool CoreProcess::start(const TunnelNode &node)
{
    if (isRunning()) {
        m_lastError = "进程已在运行";
        return false;
    }

    // 1. 查找内核可执行文件
    QString exe = findCoreExecutable();
    if (exe.isEmpty()) {
        m_lastError = "找不到 connect-ip-tunnel 可执行文件";
        emit errorOccurred(m_lastError);
        return false;
    }

    // 2. 写临时 config JSON（admin_listen 使用随机端口 :0）
    cleanupTempFile();
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_tempConfig = tmpDir + "/connect-ip-tunnel-gui-config.json";

    QJsonObject cfg = node.toClientConfig("127.0.0.1:0");
    QFile f(m_tempConfig);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = "无法写入临时配置文件: " + m_tempConfig;
        emit errorOccurred(m_lastError);
        return false;
    }
    f.write(QJsonDocument(cfg).toJson(QJsonDocument::Indented));
    f.close();

    // 3. 启动进程
    m_portEmitted = false;
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->start(exe, {"-c", m_tempConfig});
    if (!m_process->waitForStarted(3000)) {
        m_lastError = "进程启动超时";
        emit errorOccurred(m_lastError);
        cleanupTempFile();
        return false;
    }
    return true;
}

void CoreProcess::stop()
{
    if (!isRunning())
        return;

    m_stopping = true;  // 标记为主动停止，onProcessError / onProcessFinished 不会误报崩溃
    // 注意：m_stopping 的清除在 onProcessFinished 里完成，不能在这里清除！
    // 原因：terminate() 是异步的，onProcessFinished slot 在事件循环里触发，
    //       如果在 waitForFinished() 返回后立即清除 m_stopping，
    //       slot 触发时 m_stopping 已经是 false，导致误报"内核进程异常退出"。

    m_process->terminate();

    // 用 QTimer 异步 kill，避免阻塞事件循环
    // 3 秒后如果进程还在运行，才强制 kill
    QTimer::singleShot(3000, this, [this]() {
        if (isRunning()) {
            emit logReceived("[GUI] 内核进程未响应 SIGTERM，强制终止");
            m_process->kill();
        }
    });
}

bool CoreProcess::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void CoreProcess::onProcessStarted()
{
    emit logReceived("[GUI] 内核进程已启动");
    emit started();
}

void CoreProcess::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    // 在这里清除 m_stopping 标记（stop() 里不能清，因为 slot 是异步触发的）
    bool wasStopping = m_stopping;
    m_stopping = false;

    if (status == QProcess::CrashExit && !wasStopping) {
        // 真实崩溃（非主动停止）：记录日志，由 onProcessError 负责发射 stopped
        emit logReceived(QString("[GUI] 内核进程异常退出，退出码: %1").arg(exitCode));
        return;
    }

    // 正常退出（主动停止或 exit 0）
    if (wasStopping) {
        emit logReceived("[GUI] 内核进程已正常停止");
    } else {
        emit logReceived(QString("[GUI] 内核进程已退出，退出码: %1").arg(exitCode));
    }
    cleanupTempFile();
    emit stopped();
}

void CoreProcess::onProcessError(QProcess::ProcessError error)
{
    // 主动停止（terminate/kill）时，QProcess 会报 Crashed，属于正常现象，忽略即可
    if (m_stopping && error == QProcess::Crashed)
        return;

    QString msg;
    switch (error) {
    case QProcess::FailedToStart: msg = "进程启动失败（找不到可执行文件或权限不足）"; break;
    case QProcess::Crashed:       msg = "内核进程意外崩溃"; break;
    case QProcess::Timedout:      msg = "进程超时"; break;
    default:                      msg = "进程未知错误"; break;
    }
    m_lastError = msg;
    emit errorOccurred(msg);
    cleanupTempFile();
    emit stopped();
}

void CoreProcess::onReadyRead()
{
    QByteArray data = m_process->readAll();
    QString text = QString::fromUtf8(data).trimmed();
    if (text.isEmpty())
        return;

    // 逐行处理
    for (const QString &line : text.split('\n', Qt::SkipEmptyParts)) {
        emit logReceived(line.trimmed());

        // 解析 admin server 端口
        // 内核日志格式：[engine] admin server listening on 127.0.0.1:PORT
        if (!m_portEmitted) {
            static QRegularExpression re(
                R"(admin server listening on [\d\.]+:(\d+))",
                QRegularExpression::CaseInsensitiveOption
            );
            auto match = re.match(line);
            if (match.hasMatch()) {
                bool ok = false;
                quint16 port = match.captured(1).toUShort(&ok);
                if (ok && port > 0) {
                    m_portEmitted = true;
                    emit adminPortReady(port);
                }
            }
        }
    }
}

QString CoreProcess::findCoreExecutable() const
{
    // 搜索顺序：
    // 1. 与 GUI 可执行文件同目录
    // 2. PATH 中查找
    QStringList candidates;
    QString appDir = QCoreApplication::applicationDirPath();

#ifdef Q_OS_WIN
    candidates << appDir + "/connect-ip-tunnel.exe"
               << appDir + "/connect-ip-tunnel-core.exe";
#else
    candidates << appDir + "/connect-ip-tunnel"
               << appDir + "/connect-ip-tunnel-core";
#endif

    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }

    // fallback: 从 PATH 查找
#ifdef Q_OS_WIN
    return QStandardPaths::findExecutable("connect-ip-tunnel");
#else
    return QStandardPaths::findExecutable("connect-ip-tunnel");
#endif
}

void CoreProcess::cleanupTempFile()
{
    if (!m_tempConfig.isEmpty()) {
        QFile::remove(m_tempConfig);
        m_tempConfig.clear();
    }
}
