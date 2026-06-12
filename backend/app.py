"""湿度传感器 Flask 后端 - Agent-A 实现
按 collab/contracts/api_v1.yaml 契约实现6个API接口
"""
import os
import sqlite3
from datetime import datetime, timedelta
from flask import Flask, request, jsonify, g

app = Flask(__name__)

# 配置
DB_PATH = os.environ.get('HUMIDITY_DB', os.path.join(os.path.dirname(__file__), 'humidity.db'))
DEFAULT_ALERT_CONFIG = {
    "enabled": True,
    "min_threshold": 30.0,
    "max_threshold": 80.0,
    "webhook_url": "",
    "push_enabled": False
}

# ─── 数据库 ───
def get_db():
    if 'db' not in g:
        g.db = sqlite3.connect(DB_PATH)
        g.db.row_factory = sqlite3.Row
        g.db.execute("PRAGMA journal_mode=WAL")
    return g.db

@app.teardown_appcontext
def close_db(exc):
    db = g.pop('db', None)
    if db: db.close()

def init_db():
    """首次运行时创建表"""
    db = sqlite3.connect(DB_PATH)
    db.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            moisture REAL NOT NULL,
            adc INTEGER,
            voltage REAL,
            timestamp TEXT NOT NULL
        )
    """)
    db.execute("""
        CREATE TABLE IF NOT EXISTS alert_config (
            id INTEGER PRIMARY KEY CHECK(id=1),
            enabled INTEGER DEFAULT 1,
            min_threshold REAL DEFAULT 30.0,
            max_threshold REAL DEFAULT 80.0,
            webhook_url TEXT DEFAULT '',
            push_enabled INTEGER DEFAULT 0
        )
    """)
    db.execute("""
        CREATE TABLE IF NOT EXISTS push_tokens (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            token TEXT NOT NULL UNIQUE,
            platform TEXT NOT NULL,
            created_at TEXT NOT NULL
        )
    """)
    # 初始化默认告警配置
    row = db.execute("SELECT COUNT(*) FROM alert_config").fetchone()
    if row[0] == 0:
        db.execute("""
            INSERT INTO alert_config (id, enabled, min_threshold, max_threshold, webhook_url, push_enabled)
            VALUES (1, 1, 30.0, 80.0, '', 0)
        """)
    db.commit()
    db.close()

# ─── API 1: POST /api/data ───
@app.route('/api/data', methods=['POST'])
def post_data():
    """提交传感器数据"""
    data = request.get_json(force=True)
    if not data or 'moisture' not in data:
        return jsonify({"status": "error", "message": "moisture field required"}), 400
    
    ts = data.get('timestamp', datetime.utcnow().isoformat() + 'Z')
    db = get_db()
    db.execute(
        "INSERT INTO readings (moisture, adc, voltage, timestamp) VALUES (?, ?, ?, ?)",
        (data['moisture'], data.get('adc'), data.get('voltage'), ts)
    )
    db.commit()
    return jsonify({"status": "ok"}), 200

# ─── API 2: GET /api/history ───
@app.route('/api/history', methods=['GET'])
def get_history():
    """获取历史数据"""
    hours = request.args.get('hours', 24, type=int)
    limit = request.args.get('limit', 100, type=int)
    since = (datetime.utcnow() - timedelta(hours=hours)).isoformat() + 'Z'
    
    db = get_db()
    rows = db.execute(
        "SELECT moisture, adc, voltage, timestamp FROM readings WHERE timestamp >= ? ORDER BY timestamp DESC LIMIT ?",
        (since, limit)
    ).fetchall()
    result = [{"moisture": r[0], "adc": r[1], "voltage": r[2], "timestamp": r[3]} for r in rows]
    return jsonify(result), 200

# ─── API 3: GET /api/stats ───
@app.route('/api/stats', methods=['GET'])
def get_stats():
    """获取统计数据"""
    hours = request.args.get('hours', 24, type=int)
    since = (datetime.utcnow() - timedelta(hours=hours)).isoformat() + 'Z'
    
    db = get_db()
    row = db.execute("""
        SELECT AVG(moisture), MIN(moisture), MAX(moisture), COUNT(*)
        FROM readings WHERE timestamp >= ?
    """, (since,)).fetchone()
    
    return jsonify({
        "avg_moisture": round(row[0], 2) if row[0] else 0,
        "min_moisture": row[1] if row[1] else 0,
        "max_moisture": row[2] if row[2] else 0,
        "readings_count": row[3],
        "period_hours": hours
    }), 200

# ─── API 4: GET/PUT /api/alerts/config ───
@app.route('/api/alerts/config', methods=['GET'])
def get_alert_config():
    """获取告警配置"""
    db = get_db()
    row = db.execute("SELECT enabled, min_threshold, max_threshold, webhook_url, push_enabled FROM alert_config WHERE id=1").fetchone()
    return jsonify({
        "enabled": bool(row[0]),
        "min_threshold": row[1],
        "max_threshold": row[2],
        "webhook_url": row[3],
        "push_enabled": bool(row[4])
    }), 200

@app.route('/api/alerts/config', methods=['PUT'])
def update_alert_config():
    """更新告警配置"""
    data = request.get_json(force=True)
    db = get_db()
    db.execute("""
        UPDATE alert_config SET
            enabled = ?, min_threshold = ?, max_threshold = ?, webhook_url = ?, push_enabled = ?
        WHERE id = 1
    """, (
        int(data.get('enabled', True)),
        data.get('min_threshold', 30.0),
        data.get('max_threshold', 80.0),
        data.get('webhook_url', ''),
        int(data.get('push_enabled', False))
    ))
    db.commit()
    return jsonify(data), 200

# ─── API 5: POST /api/push/register ───
@app.route('/api/push/register', methods=['POST'])
def push_register():
    """注册设备推送令牌"""
    data = request.get_json(force=True)
    if not data or 'token' not in data:
        return jsonify({"status": "error", "message": "token required"}), 400
    
    db = get_db()
    try:
        db.execute(
            "INSERT INTO push_tokens (token, platform, created_at) VALUES (?, ?, ?)",
            (data['token'], data.get('platform', 'android'), datetime.utcnow().isoformat() + 'Z')
        )
        db.commit()
    except sqlite3.IntegrityError:
        # token已存在，更新platform
        db.execute("UPDATE push_tokens SET platform=? WHERE token=?", (data.get('platform', 'android'), data['token']))
        db.commit()
    
    return jsonify({"status": "ok"}), 200

# ─── API 6: POST /api/push/send ───
@app.route('/api/push/send', methods=['POST'])
def push_send():
    """发送推送通知（告警触发）"""
    data = request.get_json(force=True)
    if not data or 'user_token' not in data or 'title' not in data or 'body' not in data:
        return jsonify({"status": "error", "message": "user_token, title, body required"}), 400
    
    # TODO: 实际FCM推送 - 需要Firebase Admin SDK
    # 当前仅记录推送意图，后续接入Firebase后实现
    app.logger.info(f"PUSH intent: token={data['user_token']} title={data['title']} body={data['body']}")
    
    return jsonify({"status": "ok", "message": "push recorded (FCM integration pending)"}), 200

# ─── 启动 ───
if __name__ == '__main__':
    init_db()
    port = int(os.environ.get('HUMIDITY_PORT', 5000))
    app.run(host='0.0.0.0', port=port, debug=True)
