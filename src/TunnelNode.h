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

    // ── 认证 ─────────────────────────────────────────────────────
    enum AuthMethod { Bearer = 0, Basic = 1, CustomHeader = 2, None = 3 };
    AuthMethod authMethod = Bearer;
    QString bearerToken;
    QString basicUser;
    QString basicPassword;
    QString customHeaderName;
    QString customHeaderValue;

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

    // ── TUN ──────────────────────────────────────────────────────
    QString tunName    = "tun0";
    int     mtu        = 1400;
    // 留空 = 服务端分配（WaitForAddressAssign=true）
    QString ipv4CIDR;
    QString ipv6CIDR;
    QString dnsV4      = "1.1.1.1";
    QString dnsV6      = "2606:4700:4700::1111";

    // ── Bypass ───────────────────────────────────────────────────
    bool    bypassEnable = false;
    QString bypassAddr;               // 绕过路由的服务端地址，用于防止流量回环

    // ── HTTP3 ────────────────────────────────────────────────────
    bool    enableDatagrams = true;
    bool    allow0RTT       = false;

    // ── ConnectIP 高级 ───────────────────────────────────────────
    bool    enableReconnect   = true;
    int     numSessions       = 1;    // 多 session 并行（企业版）

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

        // TUN
        QJsonObject tun;
        tun["name"] = tunName;
        tun["mtu"]  = mtu;
        if (!ipv4CIDR.isEmpty()) tun["ipv4_cidr"] = ipv4CIDR;
        if (!ipv6CIDR.isEmpty()) tun["ipv6_cidr"] = ipv6CIDR;
        if (!dnsV4.isEmpty())    tun["dns_v4"]    = dnsV4;
        if (!dnsV6.isEmpty())    tun["dns_v6"]    = dnsV6;

        // Bypass
        QJsonObject bypass;
        bypass["enable"]      = bypassEnable;
        bypass["server_addr"] = bypassAddr;

        // TLS
        QJsonObject tls;
        tls["server_name"]          = sni;
        tls["insecure_skip_verify"] = insecureSkipVerify;
        tls["enable_pqc"]           = enablePQC;
        tls["use_system_cas"]       = useSystemCAs;
        tls["use_mozilla_ca"]       = useMozillaCA;
        tls["enable_ech"]           = enableECH;
        if (enableECH) {
            tls["ech_domain"]      = ech;
            tls["ech_doh_server"]  = echDohServer;
        }
        tls["enable_session_cache"] = enableSessionCache;
        tls["session_cache_size"]   = sessionCacheSize;

        // HTTP3
        QJsonObject http3;
        http3["enable_datagrams"] = enableDatagrams;
        http3["allow_0rtt"]       = allow0RTT;

        // Auth
        QJsonObject authObj;
        switch (authMethod) {
        case Bearer:
            authObj["method"]       = "bearer";
            authObj["bearer_token"] = bearerToken;
            break;
        case Basic:
            authObj["method"]   = "basic";
            authObj["username"] = basicUser;
            authObj["password"] = basicPassword;
            break;
        case CustomHeader:
            authObj["method"]       = "custom";
            authObj["header_name"]  = customHeaderName;
            authObj["header_value"] = customHeaderValue;
            break;
        case None:
            authObj["method"] = "none";
            break;
        }

        // ConnectIP
        QJsonObject connectip;
        connectip["addr"]                  = serverAddr;
        connectip["uri"]                   = uri;
        connectip["authority"]             = auth;
        connectip["auth"]                  = authObj;
        connectip["wait_for_address_assign"] = ipv4CIDR.isEmpty() && ipv6CIDR.isEmpty();
        connectip["enable_reconnect"]      = enableReconnect;
        connectip["num_sessions"]          = numSessions;

        // ClientConfig
        QJsonObject client;
        client["tun"]         = tun;
        client["bypass"]      = bypass;
        client["tls"]         = tls;
        client["http3"]       = http3;
        client["connect_ip"]  = connectip;
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
        o["id"]                  = id;
        o["name"]                = name;
        o["serverAddr"]          = serverAddr;
        o["uri"]                 = uri;
        o["authority"]           = authority;
        o["authMethod"]          = static_cast<int>(authMethod);
        o["bearerToken"]         = bearerToken;
        o["basicUser"]           = basicUser;
        o["basicPassword"]       = basicPassword;
        o["customHeaderName"]    = customHeaderName;
        o["customHeaderValue"]   = customHeaderValue;
        o["serverName"]          = serverName;
        o["insecureSkipVerify"]  = insecureSkipVerify;
        o["enablePQC"]           = enablePQC;
        o["useMozillaCA"]        = useMozillaCA;
        o["useSystemCAs"]        = useSystemCAs;
        o["enableECH"]           = enableECH;
        o["echDomain"]           = echDomain;
        o["echDohServer"]        = echDohServer;
        o["enableSessionCache"]  = enableSessionCache;
        o["sessionCacheSize"]    = sessionCacheSize;
        o["tunName"]             = tunName;
        o["mtu"]                 = mtu;
        o["ipv4CIDR"]            = ipv4CIDR;
        o["ipv6CIDR"]            = ipv6CIDR;
        o["dnsV4"]               = dnsV4;
        o["dnsV6"]               = dnsV6;
        o["bypassEnable"]        = bypassEnable;
        o["bypassAddr"]          = bypassAddr;
        o["enableDatagrams"]     = enableDatagrams;
        o["allow0RTT"]           = allow0RTT;
        o["enableReconnect"]     = enableReconnect;
        o["numSessions"]         = numSessions;
        return o;
    }

    static TunnelNode fromJson(const QJsonObject &o) {
        TunnelNode n;
        n.id                 = o["id"].toInt(-1);
        n.name               = o["name"].toString();
        n.serverAddr         = o["serverAddr"].toString();
        n.uri                = o["uri"].toString("/.well-known/masque/ip");
        n.authority          = o["authority"].toString();
        n.authMethod         = static_cast<AuthMethod>(o["authMethod"].toInt(0));
        n.bearerToken        = o["bearerToken"].toString();
        n.basicUser          = o["basicUser"].toString();
        n.basicPassword      = o["basicPassword"].toString();
        n.customHeaderName   = o["customHeaderName"].toString();
        n.customHeaderValue  = o["customHeaderValue"].toString();
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
        n.tunName            = o["tunName"].toString("tun0");
        n.mtu                = o["mtu"].toInt(1400);
        n.ipv4CIDR           = o["ipv4CIDR"].toString();
        n.ipv6CIDR           = o["ipv6CIDR"].toString();
        n.dnsV4              = o["dnsV4"].toString("1.1.1.1");
        n.dnsV6              = o["dnsV6"].toString("2606:4700:4700::1111");
        n.bypassEnable       = o["bypassEnable"].toBool(false);
        n.bypassAddr         = o["bypassAddr"].toString();
        n.enableDatagrams    = o["enableDatagrams"].toBool(true);
        n.allow0RTT          = o["allow0RTT"].toBool(false);
        n.enableReconnect    = o["enableReconnect"].toBool(true);
        n.numSessions        = o["numSessions"].toInt(1);
        return n;
    }

    // ── 显示辅助 ─────────────────────────────────────────────────
    bool isValid() const {
        if (serverAddr.isEmpty()) return false;
        switch (authMethod) {
        case Bearer:      return !bearerToken.isEmpty();
        case Basic:       return !basicUser.isEmpty();
        case CustomHeader: return !customHeaderName.isEmpty() && !customHeaderValue.isEmpty();
        case None:        return true;
        }
        return false;
    }

    QString displayLatency() const {
        if (latency < 0) return QObject::tr("失败");
        if (latency == 0) return "-";
        return QString("%1 ms").arg(latency);
    }

    QString displayAuth() const {
        switch (authMethod) {
        case Bearer:
            if (bearerToken.length() <= 4) return "****";
            return bearerToken.left(3) + "****";
        case Basic:       return basicUser + ":****";
        case CustomHeader: return customHeaderName + ": ****";
        case None:        return QObject::tr("无");
        }
        return "";
    }
};
