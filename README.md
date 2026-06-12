# humidity-monitor

湿度监控系统 — 土壤湿度传感器 + 后端API + 移动端App

## 结构

```
humidity-monitor/
  ├── backend/        Flask API (Python)
  ├── frontend/       Capacitor App (Android APK)
  ├── contracts/      API契约 (api_v1.yaml)
  └── README.md
```

## 协作

- ToolMan (Linux): 后端 Flask API
- Forge (Windows): 前端 Capacitor App
- 通信: 通过 GenericAgent 的 collab.py

## 启动

```bash
cd backend
pip install -r requirements.txt
python app.py
```
