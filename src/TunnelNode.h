#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUuid>

// TunnelNode 对应内核 ClientConfig 的完整结构
struct TunnelNode {
    int     id            = -1;
    QString name;

    // ── connect_ip ───────────────────────────────────────────────
    QString serverAddr;               // host:port，例如 proxy.example.com:443
    QString uri           = "/.well-known/masque/ip";
    QString authority;                // 留空则与 serverAddr host 相同

    // ── TLS ──────────────────────────────────────────────────────
    QString serverName;               // SNI，留空则与 serverAddr host 相同
    bool    insecureSkipVerify = false;
    bool    enablePQC          = false;
    bool    useMozillaCA       = true;
    bool    useSystemCAs       = false;
    bool    enableECH          = true;
    QString echDomain;                // 留空则与 serverName 相同
    QString echDohServer       = "https://cloudflare-dns.com/dns-query";
    bool    enableSessionCache = true;
    int     sessionCacheSize   = 128;

    // ── mTLS 鉴权（唯一支持的鉴权方式）──────────────────────────
    QString clientCertFile;           // 客户端证书文件路径（PEM）
    QString clientKeyFile;            // 客户端私钥文件路径（PEM）

    // ── TUN ──────────────────────────────────────────────────────
    // IP 地址由服务端通过 ADDRESS_ASSIGN 分配，无需手动配置
    QString tunName    = "tun0";
    int     mtu        = 1400;
    QString dnsV4      = "1.1.1.1";
    QString dnsV6      = "2606:4700:4700::1111";

    // ── HTTP3 ────────────────────────────────────────────────────
    bool    enableDatagrams = true;
    bool    allow0RTT       = false;

    // ── ConnectIP 高级 ───────────────────────────────────────────
    bool    enableReconnect   = true;
    int     numSessions       = 1;    // 多 session 并行

    // ── 测试结果（GUI 专用，不序列化到内核 config）────────────────
    int latency = 0; // ms，-1=失败，0=未测试

    // ────────────────────────────────────────────────────────────
    // 生成内核 config.json（ClientConfig）
    // ────────────────────────────────────────────────────────────
    QJsonObject toClientConfig(const QString &adminListen = "127.0.0.1:0") const {
        // 解析 host
        QString host = serverAddr;
        int colonPos = serverAddr.lastIndexOf(':');
        if (colonPos > 0)
            host = serverAddr.left(colonPos);

        QString sni  = serverName.isEmpty()  ? host : serverName;
        QString auth = authority.isEmpty()   ? host : authority;
        QString ech  = echDomain.isEmpty()   ? sni  : echDomain;

        // TUN — IP 由服务端 ADDRESS_ASSIGN 分配，无静态 IP 字段
        QJsonObject tun;
        tun["name"] = tunName;
        tun["mtu"]  = mtu;
        if (!dnsV4.isEmpty()) tun["dns_v4"] = dnsV4;
        if (!dnsV6.isEmpty()) tun["dns_v6"] = dnsV6;

        // Bypass — 自动使用 serverAddr 防止流量回环
        QJsonObject bypass;
        bypass["enable"]      = true;
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
            tls["ech_domain"]     = ech;
            tls["ech_doh_server"] = echDohServer;
        }
        tls["enable_session_cache"] = enableSessionCache;
        tls["session_cache_size"]   = sessionCacheSize;
        if (!clientCertFile.isEmpty()) tls["client_cert_file"] = clientCertFile;
        if (!clientKeyFile.isEmpty())  tls["client_key_file"]  = clientKeyFile;

        // HTTP3
        QJsonObject http3;
        http3["enable_datagrams"] = enableDatagrams;
        http3["allow_0rtt"]       = allow0RTT;

        // ConnectIP — wait_for_address_assign 始终为 true
        QJsonObject connectip;
        connectip["addr"]                    = serverAddr;
        connectip["uri"]                     = uri;
        connectip["authority"]               = auth;
        connectip["wait_for_address_assign"] = true;
        connectip["enable_reconnect"]        = enableReconnect;
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
        o["enableSessionCache"] = enableSessionCache;
        o["sessionCacheSize"]  = sessionCacheSize;
        o["clientCertFile"]    = clientCertFile;
        o["clientKeyFile"]     = clientKeyFile;
        o["tunName"]           = tunName;
        o["mtu"]               = mtu;
        o["dnsV4"]             = dnsV4;
        o["dnsV6"]             = dnsV6;
        o["enableDatagrams"]   = enableDatagrams;
        o["allow0RTT"]         = allow0RTT;
        o["enableReconnect"]   = enableReconnect;
        o["numSessions"]       = numSessions;
        return o;
    }

    static TunnelNode fromJson(const QJsonObject &o) {
        TunnelNode n;
        n.id                 = o["id"].toInt(-1);
        n.name               = o["name"].toString();
        n.serverAddr         = o["serverAddr"].toString();
        n.uri                = o["uri"].toString("/.well-known/masque/ip");
        n.authority          = o["authority"].toString();
        n.serverName         = o["serverName"].toString();
        n.insecureSkipVerify = o["insecureSkipVerify"].toBool(false);
        n.enablePQC          = o["enablePQC"].toBool(false);
        n.useMozillaCA       = o["useMozillaCA"].toBool(true);
        n.useSystemCAs       = o["useSystemCAs"].toBool(false);
        n.enableECH          = o["enableECH"].toBool(true);
        n.echDomain          = o["echDomain"].toString();
        n.echDohServer       = o["echDohServer"].toString("https://cloudflare-dns.com/dns-query");
        n.enableSessionCache = o["enableSessionCache"].toBool(true);
        n.sessionCacheSize   = o["sessionCacheSize"].toInt(128);
        n.clientCertFile     = o["clientCertFile"].toString();
        n.clientKeyFile      = o["clientKeyFile"].toString();
        n.tunName            = o["tunName"].toString("tun0");
        n.mtu                = o["mtu"].toInt(1400);
        n.dnsV4              = o["dnsV4"].toString("1.1.1.1");
        n.dnsV6              = o["dnsV6"].toString("2606:4700:4700::1111");
        n.enableDatagrams    = o["enableDatagrams"].toBool(true);
        n.allow0RTT          = o["allow0RTT"].toBool(false);
        n.enableReconnect    = o["enableReconnect"].toBool(true);
        n.numSessions        = o["numSessions"].toInt(1);
        return n;
    }

    // ── 显示辅助 ─────────────────────────────────────────────────
    bool isValid() const {
        // 服务端地址必填，mTLS 证书可选（服务端可配置为不强制 mTLS）
        return !serverAddr.isEmpty();
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
