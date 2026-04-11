#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrl>
#include <QUuid>

// TunnelNode 对应内核 ClientConfig 的完整结构
struct TunnelNode {
    int     id            = -1;
    QString name;

    // ── connect_ip ───────────────────────────────────────────────
    QString serverAddr;               // host:port，例如 proxy.example.com:443
    QString uri;                      // 完整 URL，例如 https://proxy.example.com/.well-known/masque/ip
    QString authority;                // HTTP :authority header，必须与服务端 uri_template 的 host 一致
                                      // 转发场景：serverAddr 是中继地址，authority 是真正代理的域名

    // ── TLS ──────────────────────────────────────────────────────
    QString serverName;               // SNI，留空则与 serverAddr host 相同
    bool    insecureSkipVerify = false;
    bool    enablePQC          = false;
    bool    useMozillaCA       = true;
    bool    useSystemCAs       = false;
    bool    enableECH          = true;
    QString echDomain;                // 留空则与 serverName 相同
    QString echDohServer       = "https://cloudflare-dns.com/dns-query";
    QString echConfigListB64;         // 静态 ECH 配置（base64，留空则动态 DoH 查询）
    bool    enableSessionCache = true;
    int     sessionCacheSize   = 128;

    // ── mTLS 鉴权（唯一支持的鉴权方式）──────────────────────────
    QString clientCertFile;           // 客户端证书文件路径（PEM）
    QString clientKeyFile;            // 客户端私钥文件路径（PEM）

    // ── TUN ──────────────────────────────────────────────────────
    // 当 waitForAddressAssign=true 时，IP 由服务端 ADDRESS_ASSIGN 分配
    // 当 waitForAddressAssign=false 时，使用下方静态 IP 配置（调试/特殊部署）
    QString tunName    = "tun0";
    int     mtu        = 1400;
    QString dnsV4      = "1.1.1.1";
    QString dnsV6      = "2606:4700:4700::1111";
    bool    waitForAddressAssign = true;  // 默认等待服务端分配 IP
    QString ipv4CIDR;                      // 静态模式: 如 "10.0.0.2/24"
    QString ipv6CIDR;                      // 静态模式: 如 "fd00::2/64"

    // ── HTTP3 ────────────────────────────────────────────────────
    bool    enableDatagrams       = true;
    bool    allow0RTT             = false;
    int     maxIdleTimeoutSec     = 30;   // HTTP3 连接空闲超时（秒）
    int     keepAlivePeriodSec    = 10;   // HTTP3 Keep-alive 周期（秒）
    bool    disablePathMTUProbe   = false; // 禁用路径 MTU 探测
    qint64  initialStreamWindow   = 16777216;   // 16 MB
    qint64  maxStreamWindow       = 67108864;   // 64 MB
    qint64  initialConnWindow     = 33554432;   // 32 MB
    qint64  maxConnWindow         = 134217728;  // 128 MB
    bool    disableCompression    = false; // 禁用 HTTP3 压缩
    int     tlsHandshakeTimeoutSec = 0;   // TLS 握手超时（秒，0=不限）
    int     maxResponseHeaderSec  = 0;    // 最大响应头等待（秒，0=不限）

    // ── Obfs（UDP 包级混��）─────────────────────────────────────
    QString obfsType;                     // 混淆类型："salamander" 或留空不启用
    QString obfsPassword;                 // 混淆密码（客户端/服务端必须相同）

    // ── ConnectIP 高级 ───────────────────────────────────────────
    bool    enableReconnect        = true;
    int     numSessions            = 1;    // 多 session 并行
    int     addressAssignTimeoutSec = 30;  // ADDRESS_ASSIGN 超时（秒）
    int     maxReconnectDelaySec    = 30;  // 最大重连延迟（秒）

    // ── Bypass ───────────────────────────────────────────────────
    bool    enableBypass = true;  // 路由绕行防止回环

    // ── 拥塞控制 ─────────────────────────────────────────────────
    // 留空或 "cubic" 使用默认 CUBIC，"bbr2" 使用 BBRv2（推荐，对抗运营商 QoS）
    QString congestionAlgo = "bbr2";

    // ── 调试 ─────────────────────────────────────────────────────
    QString keyLogPath;  // TLS key log 路径（Wireshark 调试用，留空不启用）

    // ── 测试结果（GUI 专用，不序列化到内核 config）────────────────
    int latency = 0; // ms，-1=失败，0=未测试

    // ────────────────────────────────────────────────────────────
    // 生成内核 config.json（ClientConfig）
    // ────────────────────────────────────────────────────────────
    QJsonObject toClientConfig(const QString &adminListen = "127.0.0.1:0") const {
        // 解析 serverAddr 的 host 部分（用于 SNI/ECH fallback）
        QString host = serverAddr;
        int colonPos = serverAddr.lastIndexOf(':');
        if (colonPos > 0)
            host = serverAddr.left(colonPos);

        QString sni = serverName.isEmpty() ? host : serverName;
        QString ech = echDomain.isEmpty()  ? sni  : echDomain;

        // authority：HTTP :authority header，必须与服务端 uri_template 的 host 完全一致。
        // 推断优先级：用户明确填写 > 从完整 URI 里提取 host > fallback 到 serverAddr host（直连场景）
        // 注意：转发场景下 serverAddr 是中继地址，不能用它做 authority，
        //       应由用户在 authority 字段或完整 URI 里明确写出真正代理的域名。
        QString auth = authority;
        if (auth.isEmpty() && !uri.isEmpty()) {
            // uri 是完整 URL 时提取 host（相对路径不含 host，跳过）
            QUrl parsedUri(uri);
            if (parsedUri.isValid() && !parsedUri.host().isEmpty())
                auth = parsedUri.host();
        }
        if (auth.isEmpty())
            auth = host; // 直连场景兜底：authority 同 serverAddr host

        // TUN
        QJsonObject tun;
        tun["name"] = tunName;
        tun["mtu"]  = mtu;
        if (!dnsV4.isEmpty()) tun["dns_v4"] = dnsV4;
        if (!dnsV6.isEmpty()) tun["dns_v6"] = dnsV6;
        // 静态 IP 模式：用户手动填写 CIDR（仅当 waitForAddressAssign=false 时生效）
        if (!waitForAddressAssign) {
            if (!ipv4CIDR.isEmpty()) tun["ipv4_cidr"] = ipv4CIDR;
            if (!ipv6CIDR.isEmpty()) tun["ipv6_cidr"] = ipv6CIDR;
        }

        // Bypass — 自动使用 serverAddr 防止流量回环
        QJsonObject bypass;
        bypass["enable"]      = enableBypass;
        bypass["server_addr"] = serverAddr;

        // TLS（含 mTLS 客户端证书，唯一鉴权方式）
        QJsonObject tls;
        tls["server_name"]          = sni;
        tls["insecure_skip_verify"] = insecureSkipVerify;
        tls["enable_pqc"]           = enablePQC;
        tls["use_system_cas"]       = useSystemCAs;
        tls["use_mozilla_ca"]       = useMozillaCA;
        tls["enable_ech"]           = enableECH;
        if (enableECH) {
            if (!echConfigListB64.isEmpty()) {
                // 静态 ECH：直接传 base64 字符串，内核自行解码
                tls["ech_config_list"] = echConfigListB64;
            } else {
                // 动态 ECH：通过 DoH 查询
                tls["ech_domain"]     = ech;
                tls["ech_doh_server"] = echDohServer;
            }
        }
        tls["enable_session_cache"] = enableSessionCache;
        tls["session_cache_size"]   = sessionCacheSize;
        if (!clientCertFile.isEmpty()) tls["client_cert_file"] = clientCertFile;
        if (!clientKeyFile.isEmpty())  tls["client_key_file"]  = clientKeyFile;
        if (!keyLogPath.isEmpty())     tls["key_log_path"]     = keyLogPath;

        // HTTP3
        QJsonObject http3;
        // Obfs（UDP 包级混淆）
        if (!obfsType.isEmpty()) {
            QJsonObject obfs;
            obfs["type"] = obfsType;
            if (!obfsPassword.isEmpty())
                obfs["password"] = obfsPassword;
            http3["obfs"] = obfs;
        }
        http3["enable_datagrams"]       = enableDatagrams;
        http3["allow_0rtt"]             = allow0RTT;
        http3["max_idle_timeout"]       = QString("%1s").arg(maxIdleTimeoutSec);
        http3["keep_alive_period"]      = QString("%1s").arg(keepAlivePeriodSec);
        http3["disable_path_mtu_probe"] = disablePathMTUProbe;
        http3["initial_stream_window"]  = initialStreamWindow;
        http3["max_stream_window"]      = maxStreamWindow;
        http3["initial_conn_window"]    = initialConnWindow;
        http3["max_conn_window"]        = maxConnWindow;
        http3["disable_compression"]    = disableCompression;
        if (tlsHandshakeTimeoutSec > 0)
            http3["tls_handshake_timeout"] = QString("%1s").arg(tlsHandshakeTimeoutSec);
        if (maxResponseHeaderSec > 0)
            http3["max_response_header_sec"] = maxResponseHeaderSec;
        // 拥塞控制：bbr2 推荐用于运营商 QoS 场景
        if (!congestionAlgo.isEmpty() && congestionAlgo != "cubic") {
            QJsonObject congestion;
            congestion["algorithm"] = congestionAlgo;
            http3["congestion"] = congestion;
        }

        // ConnectIP
        QJsonObject connectip;
        connectip["addr"]                    = serverAddr;
        connectip["uri"]                     = uri;  // 完整 URL，内核直接使用
        connectip["authority"]               = auth; // 必须与服务端 uri_template 的 host 一致
        connectip["wait_for_address_assign"] = waitForAddressAssign;
        connectip["address_assign_timeout"]  = QString("%1s").arg(addressAssignTimeoutSec);
        connectip["enable_reconnect"]        = enableReconnect;
        connectip["max_reconnect_delay"]     = QString("%1s").arg(maxReconnectDelaySec);
        connectip["num_sessions"]            = numSessions;

        // ClientConfig
        QJsonObject client;
        client["tun"]          = tun;
        client["bypass"]       = bypass;
        client["tls"]          = tls;
        client["http3"]        = http3;
        client["connect_ip"]   = connectip;
        client["admin_listen"] = adminListen;

        // Root Config
        QJsonObject root;
        root["mode"]   = "client";
        root["client"] = client;
        return root;
    }

    // ── 持久化（节点列表存储）────────────────────────────────────
    QJsonObject toJson() const {
        QJsonObject o;
        o["id"]                = id;
        o["name"]              = name;
        o["serverAddr"]        = serverAddr;
        o["uri"]               = uri;
        o["authority"]         = authority;
        o["serverName"]        = serverName;
        o["insecureSkipVerify"] = insecureSkipVerify;
        o["enablePQC"]         = enablePQC;
        o["useMozillaCA"]      = useMozillaCA;
        o["useSystemCAs"]      = useSystemCAs;
        o["enableECH"]         = enableECH;
        o["echDomain"]         = echDomain;
        o["echDohServer"]      = echDohServer;
        o["echConfigListB64"]  = echConfigListB64;
        o["enableSessionCache"] = enableSessionCache;
        o["sessionCacheSize"]  = sessionCacheSize;
        o["clientCertFile"]    = clientCertFile;
        o["clientKeyFile"]     = clientKeyFile;
        o["tunName"]           = tunName;
        o["mtu"]               = mtu;
        o["dnsV4"]             = dnsV4;
        o["dnsV6"]             = dnsV6;
        o["waitForAddressAssign"] = waitForAddressAssign;
        o["ipv4CIDR"]          = ipv4CIDR;
        o["ipv6CIDR"]          = ipv6CIDR;
        o["enableDatagrams"]   = enableDatagrams;
        o["allow0RTT"]         = allow0RTT;
        o["disablePathMTUProbe"] = disablePathMTUProbe;
        o["initialStreamWindow"] = initialStreamWindow;
        o["maxStreamWindow"]     = maxStreamWindow;
        o["initialConnWindow"]   = initialConnWindow;
        o["maxConnWindow"]       = maxConnWindow;
        o["disableCompression"]      = disableCompression;
        o["tlsHandshakeTimeoutSec"]  = tlsHandshakeTimeoutSec;
        o["maxResponseHeaderSec"]    = maxResponseHeaderSec;
        o["obfsType"]          = obfsType;
        o["obfsPassword"]      = obfsPassword;
        o["enableReconnect"]   = enableReconnect;
        o["numSessions"]       = numSessions;
        o["maxIdleTimeoutSec"]  = maxIdleTimeoutSec;
        o["keepAlivePeriodSec"] = keepAlivePeriodSec;
        o["addressAssignTimeoutSec"] = addressAssignTimeoutSec;
        o["maxReconnectDelaySec"]    = maxReconnectDelaySec;
        o["enableBypass"]      = enableBypass;
        o["congestionAlgo"]    = congestionAlgo;
        o["keyLogPath"]        = keyLogPath;
        return o;
    }

    static TunnelNode fromJson(const QJsonObject &o) {
        TunnelNode n;
        n.id                 = o["id"].toInt(-1);
        n.name               = o["name"].toString();
        n.serverAddr         = o["serverAddr"].toString();
        n.uri                = o["uri"].toString(); // 完整 URL，留空需用户填写
        n.authority          = o["authority"].toString();
        n.serverName         = o["serverName"].toString();
        n.insecureSkipVerify = o["insecureSkipVerify"].toBool(false);
        n.enablePQC          = o["enablePQC"].toBool(false);
        n.useMozillaCA       = o["useMozillaCA"].toBool(true);
        n.useSystemCAs       = o["useSystemCAs"].toBool(false);
        n.enableECH          = o["enableECH"].toBool(true);
        n.echDomain          = o["echDomain"].toString();
        n.echDohServer       = o["echDohServer"].toString("https://cloudflare-dns.com/dns-query");
        n.echConfigListB64   = o["echConfigListB64"].toString();
        n.enableSessionCache = o["enableSessionCache"].toBool(true);
        n.sessionCacheSize   = o["sessionCacheSize"].toInt(128);
        n.clientCertFile     = o["clientCertFile"].toString();
        n.clientKeyFile      = o["clientKeyFile"].toString();
        n.tunName            = o["tunName"].toString("tun0");
        n.mtu                = o["mtu"].toInt(1400);
        n.dnsV4              = o["dnsV4"].toString("1.1.1.1");
        n.dnsV6              = o["dnsV6"].toString("2606:4700:4700::1111");
        n.waitForAddressAssign = o["waitForAddressAssign"].toBool(true);
        n.ipv4CIDR           = o["ipv4CIDR"].toString();
        n.ipv6CIDR           = o["ipv6CIDR"].toString();
        n.enableDatagrams    = o["enableDatagrams"].toBool(true);
        n.allow0RTT          = o["allow0RTT"].toBool(false);
        n.disablePathMTUProbe = o["disablePathMTUProbe"].toBool(false);
        n.initialStreamWindow = o["initialStreamWindow"].toDouble(16777216);
        n.maxStreamWindow     = o["maxStreamWindow"].toDouble(67108864);
        n.initialConnWindow   = o["initialConnWindow"].toDouble(33554432);
        n.maxConnWindow       = o["maxConnWindow"].toDouble(134217728);
        n.disableCompression     = o["disableCompression"].toBool(false);
        n.tlsHandshakeTimeoutSec = o["tlsHandshakeTimeoutSec"].toInt(0);
        n.maxResponseHeaderSec   = o["maxResponseHeaderSec"].toInt(0);
        n.obfsType           = o["obfsType"].toString();
        n.obfsPassword       = o["obfsPassword"].toString();
        n.enableReconnect    = o["enableReconnect"].toBool(true);
        n.numSessions        = o["numSessions"].toInt(1);
        n.maxIdleTimeoutSec  = o["maxIdleTimeoutSec"].toInt(30);
        n.keepAlivePeriodSec = o["keepAlivePeriodSec"].toInt(10);
        n.addressAssignTimeoutSec = o["addressAssignTimeoutSec"].toInt(30);
        n.maxReconnectDelaySec    = o["maxReconnectDelaySec"].toInt(30);
        n.enableBypass       = o["enableBypass"].toBool(true);
        n.congestionAlgo     = o["congestionAlgo"].toString("bbr2");
        n.keyLogPath         = o["keyLogPath"].toString();
        return n;
    }

    // ── 显示辅助 ─────────────────────────────────────────────────
    bool isValid() const {
        // serverAddr 必填；uri 必填（相对路径或完整 URL 均可）
        return !serverAddr.isEmpty() && !uri.isEmpty();
    }

    QString displayLatency() const {
        if (latency < 0) return QObject::tr("失败");
        if (latency == 0) return "-";
        return QString("%1 ms").arg(latency);
    }

    QString displayAuth() const {
        if (!clientCertFile.isEmpty())
            return QObject::tr("mTLS");
        return QObject::tr("无证书");
    }
};
