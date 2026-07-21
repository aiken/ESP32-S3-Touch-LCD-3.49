# kids-calendar 云端服务

美熹和壮壮两个孩子的课程日历后端：FastAPI + JSON 文件存储，Web 编辑页 + 设备 API。

## 功能

- Web 编辑页（`/`）：简单密码登录，按孩子（美熹/壮壮）分 tab 管理每周课程（增删改、颜色、提醒分钟）
- 设备 API（`X-Device-Key` 头鉴权）：
  - `GET /api/today?kid=meixi|zhuangzhuang` — 今日课程（服务端日期，按开始时间排序）
  - `GET /api/sync?kid=...` — 全部课程
  - `GET /api/health`
- 管理 API（会话 cookie）：`POST /api/login`、`GET/POST/DELETE /api/courses`
- API 文档：`/docs`（FastAPI 自动生成）

## 本机开发

```bash
cd calendar-server
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python -m uvicorn app.main:app --host 0.0.0.0 --port 3001 --reload
# 打开 http://localhost:3001/ （默认密码 admin123）
```

环境变量：`ADMIN_PASSWORD`（默认 admin123）、`DEVICE_KEY`（默认 devkey-change-me）、`DATA_DIR`（默认 ./data）、`PORT`（默认 3000）。

## NAS 部署（Docker）

```bash
# 把 calendar-server 目录拷到 NAS，然后：
cd calendar-server
ADMIN_PASSWORD=你的密码 DEVICE_KEY=你的设备密钥 docker compose up -d --build
# 服务在 NAS:3000，数据持久化在 ./data/courses.json
```

注意本机 3000 端口若被占用，改 `docker-compose.yml` 的 `ports`（如 `"3001:3000"`），设备端 `API_LAN_URL` 同步改。

## Cloudflare Tunnel（外网 HTTPS）

在 NAS 上跑 cloudflared（docker 或原生均可）：

```bash
# 一次性：登录并创建 tunnel（在 Cloudflare Zero Trust 控制台拿 token 更简单）
docker run -d --name cloudflared --restart unless-stopped \
  cloudflare/cloudflared:latest tunnel --no-autoupdate run --token <你的TUNNEL_TOKEN>
```

然后在 CF 控制台把域名（如 `calendar.yourdomain.com`）Public Hostname 指到 `http://host.docker.internal:3000`（或 NAS IP:3000）。

设备端配置（`idf.py menuconfig` → Calendar App Configuration）：

- `API_LAN_URL` = `http://<NAS-IP>:3000`（局域网优先）
- `API_CLOUD_URL` = `https://calendar.yourdomain.com`（兜底，可留空关闭）
- `DEVICE_KEY` = 与服务端一致
- `KID_PROFILE` = meixi 或 zhuangzhuang

设备先请求局域网地址，失败（超时/拒连/非 200）再走 Cloudflare；两边都失败则用 NVS 里的上次缓存。
