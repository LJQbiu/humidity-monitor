# Humidity Monitor

湿度传感器监控系统 - 前后端 monorepo

## 结构

```
humidity-monitor/
  ├── backend/        # Flask + SQLite (Agent-A)
  ├── frontend/       # Capacitor APK (Agent-B)
  ├── contracts/      # API契约 (共享)
  └── README.md
```

## 契约

API契约定义在 `contracts/api_v1.yaml`

## 接口

| 接口 | 方法 | 说明 |
|------|------|------|
| /api/data | POST | 插入传感器数据 |
| /api/history | GET | 查询历史数据 |
| /api/stats | GET | 统计摘要 |
| /api/alerts/config | PUT/GET | 告警配置读写 |
| /api/push/register | POST | FCM令牌注册 |
| /api/push/send | POST | 推送通知发送 |

## 快速启动

```bash
cd backend
pip install -r requirements.txt
python app.py
```

后端默认运行在 `http://0.0.0.0:5000`
