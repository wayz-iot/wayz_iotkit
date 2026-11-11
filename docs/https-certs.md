# 传输协议与证书（HTTP / HTTPS）

> **当前结论：本组件默认且实际使用 `http://`。** 不引入 TLS 组件、不需要设备内置任何证书。
> 本文件保留 HTTPS 的事实与做法，供后续确有需要时参考。

## 一、现状

| 项 | 状态 |
| ---- | ---- |
| **HTTP 调通** | ✅ **已达成并验证**：组件默认 `http://`，无需 TLS 组件与证书；已在 RT-Thread 模拟器内端到端验证（真实请求 → 真实响应 → 解析 → 打印） |
| HTTPS | ⏸ 未启用。RT-Thread 的 mbedTLS 移植默认强制校验证书且信任链编译进固件，直接切 https 会握手失败（详见第三节） |

组件的协议选择：

```c
/* inc/wayz_pos.h */
#if defined(PKG_WAYZ_IOTKIT_USE_HTTPS)
#  define WAYZ_USE_HTTPS     1
#else
#  define WAYZ_USE_HTTPS     0      /* 默认: http */
#endif
```

在 BSP 的 `rtconfig.h` 里定义 `PKG_WAYZ_IOTKIT_USE_HTTPS` 才会切到 https。

> **注意**：云端接口规定 `access_key` 放在 URL 的 query 参数里，即使走 https，密钥也仍会出现在 URL 中
> （只是传输过程被加密）；走 http 则密钥与定位数据在链路上均为明文。
> 组件不会把带密钥的完整 URL 打印到日志里。

## 二、HTTP 的安全前提

默认 http 的前提下，请确保链路本身受控，例如：

- 设备走内网 / 专用 AP；
- 或在网络层用 VPN、模组加密隧道、专用网关做保护；
- 或对 `access_key` 做定期轮换（它等价于设备身份凭据）。

## 三、若将来要启用 HTTPS

### 为什么不能"直接开 https"

RT-Thread 的两条 TLS 路径（webclient 的 https、SAL 的 https）**共用** mbedtls 软件包的
`ports/src/tls_client.c`：

```c
ret = mbedtls_x509_crt_parse(&session->cacert,
                (const unsigned char *)mbedtls_root_certificate,
                mbedtls_root_certificate_len);      /* 信任链 = 编译进固件的根证书 */

mbedtls_ssl_conf_authmode(&session->conf, MBEDTLS_SSL_VERIFY_REQUIRED);  /* 写死强制校验 */
mbedtls_ssl_conf_ca_chain(&session->conf, &session->cacert, NULL);
```

mbedtls 包默认只编进一张 **DigiCert High Assurance EV Root CA**，
不在 `api.newayz.com` 的证书链里 → 不改第三方包必然握手失败。

服务端证书链实测（`openssl s_client`）：

```
CN=*.newayz.com                                        ← 叶子(RSA-2048/SHA256)
  ← C=CN, O=WoTrus CA Limited, CN=WoTrus RSA DV SSL CA 2
    ← C=GB, O=Sectigo Limited, CN=Sectigo Public Server Authentication Root R46
      ← C=US, O=The USERTRUST Network, CN=USERTrust RSA Certification Authority  ← 自签名根
```

叶子证书有效期 **2026-06-24 ~ 2027-01-08**。

### 三条可选路径

| 路径 | 做法 | 代价 |
| ---- | ---- | ---- |
| **A. 内置根证书（推荐，约 1.5KB）** | 把 `certs/USERTrust_RSA_Certification_Authority.pem` 放进 `romfs/certs/`（mbedtls 的 "certs from FS"），或追加进 mbedtls 包的 `ports/src/tls_certificate.c` 的 `mbedtls_root_certificate[]` | 需要维护证书；根证书换发时要更新固件 |
| B. 证书固定 | 只信任 `certs/WoTrus_RSA_DV_SSL_CA_2.pem`（中间证书）+ 服务端提供的 Sectigo | 仍要内置一张证书；链更短 |
| C. 关闭证书校验 | 给 mbedtls 包打一行 `MBEDTLS_SSL_VERIFY_NONE` 补丁 | **等于加密但不认证**，中间人可拿到 `access_key`；每个使用者都要维护 fork。仅适合实验室 |

本目录已备好并用 openssl 验签通过的证书：

- `certs/USERTrust_RSA_Certification_Authority.pem` —— 根证书（信任锚）
- `certs/WoTrus_RSA_DV_SSL_CA_2.pem` —— 中间证书

验证命令（服务端链 `chain.pem` 由 `openssl s_client -showcerts` 得到）：

```sh
openssl verify -CAfile certs/USERTrust_RSA_Certification_Authority.pem \
                -untrusted <服务端中间证书> <叶子证书>       # → OK
```

> 另注：mbedtls 的 `ports/inc/tls_config.h` 里 `MBEDTLS_HAVE_TIME` 打开但 `MBEDTLS_HAVE_TIME_DATE`
> 被注释 → 默认**不校验证书有效期**，设备没有 RTC/NTP 也能握手（代价是证书过期不会被发现）。

