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
    // 注意：内核已永久移除 tls.insecure_skip_verify（写出该字段会触发 deprecated 警告，
    // 写出 true 直接拒绝启动），GUI 不再生成该字段。
    bool    enablePQC          = false;
    bool    useMozillaCA       = true;
    bool    useSystemCAs       = false;
    // ServerCAFile 自签 CA 部署的首选信任锚（优先级高于 use_mozilla_ca / use_system_cas）
    // PEM 格式，可包含多个证书；企业 mTLS 部署通常用 certsrv 签发的 ca.crt。
    QString serverCAFile;
    bool    enableECH          = true;
    QString echDomain;                // 留空则与 serverName 相同
    QString echDohServer       = "https://cloudflare-dns.com/dns-query";
    QString echConfigListB64;         // 静态 ECH 配置（base64，留空则动态 DoH 查询）
    bool    enableSessionCache = true;
    int     sessionCacheSize   = 128;
    // 地址族偏好与 Happy Eyeballs（双栈环境调优）
    // "auto" - Happy Eyeballs（IPv6 优先，IPv4 兜底）
    // "v4"   - 仅 IPv4
    // "v6"   - 仅 IPv6
    QString preferAddressFamily = "auto";
    int     happyEyeballsDelayMs = 50;  // Happy Eyeballs 交错延迟（毫秒），0=用内核默认

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
    // 注意：默认值与内核 (option/validate.go) 保持一致
    int     maxIdleTimeoutSec     = 45;   // HTTP3 连接空闲超时（秒），内核默认 45s
    int     keepAlivePeriodSec    = 15;   // HTTP3 Keep-alive 周期（秒），内核默认 15s
    bool    disablePathMTUProbe   = false; // 禁用路径 MTU 探测
    qint64  initialStreamWindow   = 16777216;   // 16 MB
    qint64  maxStreamWindow       = 67108864;   // 64 MB
    qint64  initialConnWindow     = 33554432;   // 32 MB
    qint64  maxConnWindow         = 134217728;  // 128 MB
    // 注：原 disableCompression / tlsHandshakeTimeoutSec / maxResponseHeaderSec
    // 已随内核同步移除（CONNECT-IP 走 datagram + capsule，无 HTTP 响应头与 HTTP3 压缩）。

    // UDP socket buffer 与 GSO/GRO（性能调优，主要对 Linux/服务端有效）
    qint64  udpRecvBuffer         = 16777216; // 16 MB
    qint64  udpSendBuffer         = 16777216; // 16 MB
    bool    enableGSO             = true;     // Linux GSO/GRO 加速

    // ── Obfs（UDP 包级混��）─────────────────────────────────────
    QString obfsType;                     // 混淆类型："salamander" 或留空不启用
    QString obfsPassword;                 // 混淆密码（客户端/服务端必须相同）

    // ── ConnectIP 高级 ───────────────────────────────────────────
    bool    enableReconnect        = true;
    // 多 session 并行：默认 4，对齐 config.client.example.json 的推荐值。
    // 多核 / 高带宽场景建议 2-8（如 4 跑 8-16 Gbps）。
    // 客户端会并行建立 N 条 CONNECT-IP session 共享流量，
    // 建议同时启用 perSessionReconnect 让每条 session 独立重连。
    int     numSessions            = 4;
    int     addressAssignTimeoutSec = 30;  // ADDRESS_ASSIGN 超时（秒）
    int     maxReconnectDelaySec    = 30;  // 最大重连延迟（秒）

    // 应用层心跳（CONNECT-IP capsule ping/pong）
    int     appKeepalivePeriodSec  = 25;   // ping 发送周期（秒），0=禁用
    int     appKeepaliveTimeoutSec = 30;   // 单次 ping 等 pong 的超时（秒）
    int     unhealthyThreshold     = 3;    // 连续 N 次 timeout 视为不健康
    bool    perSessionReconnect    = true; // 多 session 独立重连总开关

    // ── Bypass ───────────────────────────────────────────────────
    bool    enableBypass = true;  // 路由绕行防止回环
    bool    bypassStrict = false; // 严格模式：探测失败时返回错误而非降级

    // ── 拥塞控制 ─────────────────────────────────────────────────
    // 留空或 "cubic" 使用默认 CUBIC，"bbr2" 使用 BBRv2（推荐，对抗运营商 QoS）
    QString congestionAlgo = "bbr2";

    // ── BBRv2 子参数（仅 congestionAlgo == "bbr2" 时生效）─────────
    // 留空 / 0 = 走内核默认值（不写入 JSON）
    double  bbr2LossThreshold       = 0.0;   // 0 = 内核默认 0.015 (1.5%)
    double  bbr2Beta                = 0.0;   // 0 = 内核默认 0.3
    int     bbr2StartupFullBwRounds = 0;     // 0 = 内核默认 3
    int     bbr2ProbeRTTPeriodSec   = 0;     // 0 = 内核默认 10s
    int     bbr2ProbeRTTDurationMs  = 0;     // 0 = 内核默认 200ms
    QString bbr2BwLoReduction;               // 留空 = 内核默认 "default"
    bool    bbr2Aggressive          = false; // 激进模式：初始窗口更大

    // ── 管理 / 调试 API（admin_listen / admin_token / pprof）─────
    // adminListen 留空时由 CoreProcess 自动分配 127.0.0.1:0；
    // 非 loopback 地址必须填 adminToken，否则内核拒绝启动。
    QString adminListen;
    QString adminToken;
    bool    enablePprof = false;

    // ── 调试 ─────────────────────────────────────────────────────
    QString keyLogPath;  // TLS key log 路径（Wireshark 调试用，留空不启用）

    // ── 测试结果（GUI 专用，不序列化到内核 config）────────────────
    int latency = 0; // ms，-1=失败，0=未测试

    // ────────────────────────────────────────────────────────────
    // 生成内核 config.json（ClientConfig）
    // ────────────────────────────────────────────────────────────
    QJsonObject toClientConfig(const QString &adminListenFallback = "127.0.0.1:0") const {
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
        // 注意：bypass.server_addr 期望纯 host（不含 port），因此从 serverAddr 拆出 host 部分
        QJsonObject bypass;
        bypass["enable"]      = enableBypass;
        bypass["server_addr"] = host;
        if (bypassStrict) bypass["strict"] = true;

        // TLS（含 mTLS 客户端证书，唯一鉴权方式）
        // 注意：内核已永久移除 tls.insecure_skip_verify，GUI 永远不写出该字段。
        QJsonObject tls;
        tls["server_name"]          = sni;
        tls["enable_pqc"]           = enablePQC;
        tls["use_system_cas"]       = useSystemCAs;
        tls["use_mozilla_ca"]       = useMozillaCA;
        // 信任锚优先级：server_ca_file > use_mozilla_ca > use_system_cas
        if (!serverCAFile.isEmpty()) tls["server_ca_file"] = serverCAFile;
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
        // 地址族偏好与 Happy Eyeballs
        if (!preferAddressFamily.isEmpty())
            tls["prefer_address_family"] = preferAddressFamily;
        if (happyEyeballsDelayMs > 0)
            tls["happy_eyeballs_delay"] = QString("%1ms").arg(happyEyeballsDelayMs);

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
        // UDP socket buffer & GSO（性能调优）
        if (udpRecvBuffer > 0) http3["udp_recv_buffer"] = udpRecvBuffer;
        if (udpSendBuffer > 0) http3["udp_send_buffer"] = udpSendBuffer;
        http3["enable_gso"]             = enableGSO;
        // 拥塞控制：bbr2 推荐用于运营商 QoS 场景
        if (!congestionAlgo.isEmpty() && congestionAlgo != "cubic") {
            QJsonObject congestion;
            congestion["algorithm"] = congestionAlgo;
            // BBRv2 子参数：仅 bbr2 模式下生效，全部默认 0/空 = 不写出，由内核取默认
            if (congestionAlgo == "bbr2") {
                QJsonObject bbr2;
                if (bbr2LossThreshold > 0)        bbr2["loss_threshold"]          = bbr2LossThreshold;
                if (bbr2Beta > 0)                  bbr2["beta"]                    = bbr2Beta;
                if (bbr2StartupFullBwRounds > 0)   bbr2["startup_full_bw_rounds"]  = bbr2StartupFullBwRounds;
                if (bbr2ProbeRTTPeriodSec > 0)
                    bbr2["probe_rtt_period"]   = QString("%1s").arg(bbr2ProbeRTTPeriodSec);
                if (bbr2ProbeRTTDurationMs > 0)
                    bbr2["probe_rtt_duration"] = QString("%1ms").arg(bbr2ProbeRTTDurationMs);
                if (!bbr2BwLoReduction.isEmpty()) bbr2["bw_lo_reduction"] = bbr2BwLoReduction;
                if (bbr2Aggressive)                bbr2["aggressive"]      = true;
                if (!bbr2.isEmpty()) congestion["bbr2"] = bbr2;
            }
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
        // 应用层心跳（CONNECT-IP capsule ping/pong）
        // app_keepalive_period=0 表示禁用
        connectip["app_keepalive_period"]    = QString("%1s").arg(appKeepalivePeriodSec);
        if (appKeepaliveTimeoutSec > 0)
            connectip["app_keepalive_timeout"] = QString("%1s").arg(appKeepaliveTimeoutSec);
        if (unhealthyThreshold > 0)
            connectip["unhealthy_threshold"]   = unhealthyThreshold;
        connectip["per_session_reconnect"]   = perSessionReconnect;

        // ClientConfig
        QJsonObject client;
        client["tun"]          = tun;
        client["bypass"]       = bypass;
        client["tls"]          = tls;
        client["http3"]        = http3;
        client["connect_ip"]   = connectip;
        // admin_listen：节点显式配置 > 调用方 fallback（CoreProcess 默认 127.0.0.1:0）
        client["admin_listen"] = adminListen.isEmpty() ? adminListenFallback : adminListen;
        // admin_token：非 loopback 地址必填，否则内核拒启动；GUI 让用户自行决定
        if (!adminToken.isEmpty()) client["admin_token"] = adminToken;
        if (enablePprof)            client["enable_pprof"] = true;

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
        // 注：insecureSkipVerify 字段已永久移除（内核拒绝该字段），不再持久化
        o["enablePQC"]         = enablePQC;
        o["useMozillaCA"]      = useMozillaCA;
        o["useSystemCAs"]      = useSystemCAs;
        o["serverCAFile"]      = serverCAFile;
        o["enableECH"]         = enableECH;
        o["echDomain"]         = echDomain;
        o["echDohServer"]      = echDohServer;
        o["echConfigListB64"]  = echConfigListB64;
        o["enableSessionCache"] = enableSessionCache;
        o["sessionCacheSize"]  = sessionCacheSize;
        o["preferAddressFamily"]  = preferAddressFamily;
        o["happyEyeballsDelayMs"] = happyEyeballsDelayMs;
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
        o["udpRecvBuffer"]     = udpRecvBuffer;
        o["udpSendBuffer"]     = udpSendBuffer;
        o["enableGSO"]         = enableGSO;
        o["obfsType"]          = obfsType;
        o["obfsPassword"]      = obfsPassword;
        o["enableReconnect"]   = enableReconnect;
        o["numSessions"]       = numSessions;
        o["maxIdleTimeoutSec"]  = maxIdleTimeoutSec;
        o["keepAlivePeriodSec"] = keepAlivePeriodSec;
        o["addressAssignTimeoutSec"] = addressAssignTimeoutSec;
        o["maxReconnectDelaySec"]    = maxReconnectDelaySec;
        o["appKeepalivePeriodSec"]   = appKeepalivePeriodSec;
        o["appKeepaliveTimeoutSec"]  = appKeepaliveTimeoutSec;
        o["unhealthyThreshold"]      = unhealthyThreshold;
        o["perSessionReconnect"]     = perSessionReconnect;
        o["enableBypass"]      = enableBypass;
        o["bypassStrict"]      = bypassStrict;
        o["congestionAlgo"]    = congestionAlgo;
        // BBRv2 子参数
        o["bbr2LossThreshold"]       = bbr2LossThreshold;
        o["bbr2Beta"]                = bbr2Beta;
        o["bbr2StartupFullBwRounds"] = bbr2StartupFullBwRounds;
        o["bbr2ProbeRTTPeriodSec"]   = bbr2ProbeRTTPeriodSec;
        o["bbr2ProbeRTTDurationMs"]  = bbr2ProbeRTTDurationMs;
        o["bbr2BwLoReduction"]       = bbr2BwLoReduction;
        o["bbr2Aggressive"]          = bbr2Aggressive;
        // 管理 / 调试 API
        o["adminListen"]       = adminListen;
        o["adminToken"]        = adminToken;
        o["enablePprof"]       = enablePprof;
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
        // insecureSkipVerify 字段已永久移除：旧节点 JSON 中残留时静默忽略
        n.enablePQC          = o["enablePQC"].toBool(false);
        n.useMozillaCA       = o["useMozillaCA"].toBool(true);
        n.useSystemCAs       = o["useSystemCAs"].toBool(false);
        n.serverCAFile       = o["serverCAFile"].toString();
        n.enableECH          = o["enableECH"].toBool(true);
        n.echDomain          = o["echDomain"].toString();
        n.echDohServer       = o["echDohServer"].toString("https://cloudflare-dns.com/dns-query");
        n.echConfigListB64   = o["echConfigListB64"].toString();
        n.enableSessionCache = o["enableSessionCache"].toBool(true);
        n.sessionCacheSize   = o["sessionCacheSize"].toInt(128);
        n.preferAddressFamily  = o["preferAddressFamily"].toString("auto");
        n.happyEyeballsDelayMs = o["happyEyeballsDelayMs"].toInt(50);
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
        n.udpRecvBuffer      = o["udpRecvBuffer"].toDouble(16777216);
        n.udpSendBuffer      = o["udpSendBuffer"].toDouble(16777216);
        n.enableGSO          = o["enableGSO"].toBool(true);
        n.obfsType           = o["obfsType"].toString();
        n.obfsPassword       = o["obfsPassword"].toString();
        n.enableReconnect    = o["enableReconnect"].toBool(true);
        n.numSessions        = o["numSessions"].toInt(4);
        // 默认值与内核 (option/validate.go) 一致：maxIdleTimeout=45s, keepAlive=15s
        n.maxIdleTimeoutSec  = o["maxIdleTimeoutSec"].toInt(45);
        n.keepAlivePeriodSec = o["keepAlivePeriodSec"].toInt(15);
        n.addressAssignTimeoutSec = o["addressAssignTimeoutSec"].toInt(30);
        n.maxReconnectDelaySec    = o["maxReconnectDelaySec"].toInt(30);
        n.appKeepalivePeriodSec   = o["appKeepalivePeriodSec"].toInt(25);
        n.appKeepaliveTimeoutSec  = o["appKeepaliveTimeoutSec"].toInt(30);
        n.unhealthyThreshold      = o["unhealthyThreshold"].toInt(3);
        n.perSessionReconnect     = o["perSessionReconnect"].toBool(true);
        n.enableBypass       = o["enableBypass"].toBool(true);
        n.bypassStrict       = o["bypassStrict"].toBool(false);
        n.congestionAlgo     = o["congestionAlgo"].toString("bbr2");
        // BBRv2 子参数
        n.bbr2LossThreshold       = o["bbr2LossThreshold"].toDouble(0.0);
        n.bbr2Beta                = o["bbr2Beta"].toDouble(0.0);
        n.bbr2StartupFullBwRounds = o["bbr2StartupFullBwRounds"].toInt(0);
        n.bbr2ProbeRTTPeriodSec   = o["bbr2ProbeRTTPeriodSec"].toInt(0);
        n.bbr2ProbeRTTDurationMs  = o["bbr2ProbeRTTDurationMs"].toInt(0);
        n.bbr2BwLoReduction       = o["bbr2BwLoReduction"].toString();
        n.bbr2Aggressive          = o["bbr2Aggressive"].toBool(false);
        // 管理 / 调试 API
        n.adminListen        = o["adminListen"].toString();
        n.adminToken         = o["adminToken"].toString();
        n.enablePprof        = o["enablePprof"].toBool(false);
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
