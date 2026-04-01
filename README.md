# Connect-IP Tunnel GUI

基于 Qt 6 的 [connect-ip-tunnel](../connect-ip-tunnel/) 图形化客户端，提供直观的节点管理、一键连接和实时状态监控。

## 功能特性

- **节点管理** — 添加、编辑、删除多个隧道节点，配置持久化到本地 JSON 文件
- **一键连接** — 选中节点后一键启动内核进程，自动生成临时配置文件
- **实时统计** — 通过内核 Admin API 轮询，显示连接状态、分配 IP、上下行流量、丢包等
- **系统托盘** — 关闭窗口最小化到托盘，支持托盘菜单快速连接/断开/退出
- **完整配置** — 覆盖内核客户端的所有关键配置项：
  - 基本：服务端地址、URI、Authority
  - 鉴权：mTLS 客户端证书 + 私钥（唯一鉴权方式）
  - TLS/ECH：SNI、PQC、ECH、Mozilla CA / 系统 CA、Session Cache
  - TUN：设备名、MTU、DNS
  - HTTP3：Datagram、0-RTT、空闲超时、Keep-Alive
  - 高级：自动重连、重连延迟、并行 Session、地址分配超时
- **跨平台** — 支持 Windows / Linux / macOS

## 截图

> TODO: 添加截图

## 构建依赖

| 依赖 | 最低版本 |
|------|---------|
| Qt   | 6.2+    |
| CMake | 3.16+  |
| C++ 编译器 | C++17 |

## 构建步骤

```bash
# 1. 创建构建目录
mkdir build && cd build

# 2. 配置（确保 Qt 6 在 CMAKE_PREFIX_PATH 中）
cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt6

# 3. 编译
cmake --build . --parallel

# 4. 运行
./connect-ip-tunnel-gui     # Linux / macOS
connect-ip-tunnel-gui.exe   # Windows
```

## 使用说明

### 1. 放置内核可执行文件

将 `connect-ip-tunnel`（或 `connect-ip-tunnel.exe`）放在 GUI 可执行文件同目录下，或确保其在系统 `PATH` 中。

### 2. 添加节点

点击「添加」按钮，在弹出的编辑对话框中配置：

- **基本** 标签页：填写名称、服务端地址（`host:port`）、URI
- **TLS / ECH** 标签页：配置 SNI、ECH、证书验证等 TLS 选项
- **TUN** 标签页：配置 TUN 设备名、MTU、DNS（IP 由服务端自动分配）
- **高级** 标签页：配置 0-RTT、自动重连、Session 缓存、超时参数等

### 3. 连接

选中节点后点击「连接」，GUI 会：
1. 根据节点配置生成临时 JSON 配置文件
2. 以 `-c <config.json>` 参数启动内核进程
3. 解析内核日志获取 Admin API 端口
4. 通过 `GET /api/v1/stats` 轮询连接状态和流量统计

### 4. 断开

点击「断开」按钮，GUI 会停止统计轮询并终止内核进程。

## 项目结构

```
connect-ip-tunnel-gui/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 本文件
├── ui/
│   ├── MainWindow.ui           # 主窗口布局
│   └── EditNodeDialog.ui       # 节点编辑对话框布局
├── src/
│   ├── main.cpp                # 入口
│   ├── MainWindow.h/cpp        # 主窗口逻辑
│   ├── EditNodeDialog.h/cpp    # 节点编辑对话框
│   ├── TunnelNode.h            # 节点数据结构 + 配置序列化
│   ├── NodeManager.h/cpp       # 节点持久化管理
│   ├── CoreProcess.h/cpp       # 内核进程管理
│   ├── StatsPoller.h/cpp       # Admin API 统计轮询
│   ├── SystemTray.h/cpp        # 系统托盘
│   └── TunnelStats.h           # 统计数据结构
└── resources/
    └── icons/                  # 应用图标
```

## 与内核的关系

GUI 不直接实现隧道功能，而是作为 `connect-ip-tunnel` 内核的前端：

- GUI 将用户配置转换为内核的 JSON 配置格式（`option.Config`）
- 通过子进程方式启动内核（`connect-ip-tunnel -c <config.json>`）
- 通过内核的 HTTP Admin API（`/api/v1/stats`）获取实时状态
- 内核负责所有底层工作：QUIC/HTTP3 连接、CONNECT-IP 协议、TUN 设备管理、路由配置等

## 配置字段对照表

以下是 GUI 配置项与内核 JSON 配置的对应关系：

| GUI 字段 | 内核 JSON 路径 | 说明 |
|----------|---------------|------|
| 服务端地址 | `client.connect_ip.addr` | QUIC 连接目标 |
| URI | `client.connect_ip.uri` | CONNECT-IP 请求 URI |
| Authority | `client.connect_ip.authority` | HTTP :authority |
| SNI | `client.tls.server_name` | TLS SNI |
| 启用 ECH | `client.tls.enable_ech` | Encrypted Client Hello |
| 客户端证书 | `client.tls.client_cert_file` | mTLS 证书 |
| 客户端私钥 | `client.tls.client_key_file` | mTLS 私钥 |
| TUN 设备名 | `client.tun.name` | TUN 网卡名 |
| MTU | `client.tun.mtu` | TUN MTU |
| 允许 0-RTT | `client.http3.allow_0rtt` | QUIC 0-RTT |
| 空闲超时 | `client.http3.max_idle_timeout` | 连接空闲超时 |
| Keep-Alive | `client.http3.keep_alive_period` | Keep-alive 周期 |
| 自动重连 | `client.connect_ip.enable_reconnect` | 断线重连 |
| 最大重连延迟 | `client.connect_ip.max_reconnect_delay` | 重连退避上限 |
| 地址分配超时 | `client.connect_ip.address_assign_timeout` | ADDRESS_ASSIGN 超时 |
| Session 缓存 | `client.tls.enable_session_cache` | TLS Session 复用 |
| 并行 Session | `client.connect_ip.num_sessions` | 多路 QUIC Session |

## 许可证

与 connect-ip-tunnel 内核使用相同的许可证。
