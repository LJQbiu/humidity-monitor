"""湿度传感器 Flask 后端 - ToolMan 实现
按 collab/contracts/api_v1.yaml 契约实现6+个API接口
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
    db.execute("""
        CREATE TABLE IF NOT EXISTS alert_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            type TEXT NOT NULL CHECK(type IN ('low','high')),
            moisture REAL NOT NULL,
            threshold REAL NOT NULL,
            timestamp TEXT NOT NULL,
            notified INTEGER DEFAULT 0
        )
    """)
    db.execute("""
        CREATE TABLE IF NOT EXISTS watering_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            time TEXT NOT NULL,
            moisture_before REAL,
            moisture_after REAL
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

    # 自动告警检测: 检查是否超出阈值
    _check_and_trigger_alert(db, data['moisture'])

    return jsonify({"status": "ok"}), 200

def _check_and_trigger_alert(db, moisture):
    """检查湿度是否超出阈值，触发告警记录"""
    row = db.execute("SELECT enabled, min_threshold, max_threshold FROM alert_config WHERE id=1").fetchone()
    if not row or not row[0]:
        return
    min_t, max_t = row[1], row[2]
    alert_type = None
    if moisture < min_t:
        alert_type = 'low'
    elif moisture > max_t:
        alert_type = 'high'
    if alert_type:
        threshold = min_t if alert_type == 'low' else max_t
        ts = datetime.utcnow().isoformat() + 'Z'
        db.execute(
            "INSERT INTO alert_history (type, moisture, threshold, timestamp) VALUES (?, ?, ?, ?)",
            (alert_type, moisture, threshold, ts)
        )
        db.commit()

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
    
    # 获取最新一条数据
    latest = db.execute(
        "SELECT moisture, voltage, adc, timestamp FROM readings WHERE timestamp >= ? ORDER BY timestamp DESC LIMIT 1",
        (since,)
    ).fetchone()
    
    return jsonify({
        "avg_moisture": round(row[0], 2) if row[0] else 0,
        "min_moisture": row[1] if row[1] else 0,
        "max_moisture": row[2] if row[2] else 0,
        "readings_count": row[3],
        "period_hours": hours,
        "latest_moisture": latest[0] if latest else None,
        "latest_voltage": latest[1] if latest else None,
        "latest_adc": latest[2] if latest else None,
        "latest_time": latest[3] if latest else None
    }), 200

# ─── API 4: GET/PUT /api/alerts/config ───
@app.route('/api/alerts/config', methods=['GET'])
def get_alert_config():
    """获取告警配置（返回前端使用的字段名 low_threshold/high_threshold）"""
    db = get_db()
    row = db.execute("SELECT enabled, min_threshold, max_threshold, webhook_url, push_enabled FROM alert_config WHERE id=1").fetchone()
    return jsonify({
        "enabled": bool(row[0]),
        "low_threshold": row[1],
        "high_threshold": row[2],
        "min_threshold": row[1],
        "max_threshold": row[2],
        "webhook_url": row[3],
        "push_enabled": bool(row[4])
    }), 200

@app.route('/api/alerts/config', methods=['PUT'])
def update_alert_config():
    """更新告警配置（同时支持前端字段名 low/high 和后端字段名 min/max）"""
    data = request.get_json(force=True)
    db = get_db()
    
    # 兼容前端字段名和后端字段名
    min_val = data.get('min_threshold') or data.get('low_threshold')
    max_val = data.get('max_threshold') or data.get('high_threshold')
    
    db.execute("""
        UPDATE alert_config SET
            enabled = ?, min_threshold = ?, max_threshold = ?, webhook_url = ?, push_enabled = ?
        WHERE id = 1
    """, (
        int(data.get('enabled', True)),
        min_val if min_val is not None else 30.0,
        max_val if max_val is not None else 80.0,
        data.get('webhook_url', ''),
        int(data.get('push_enabled', False))
    ))
    db.commit()
    # 返回时用前端字段名
    row = db.execute("SELECT enabled, min_threshold, max_threshold, webhook_url, push_enabled FROM alert_config WHERE id=1").fetchone()
    return jsonify({
        "enabled": bool(row[0]),
        "low_threshold": row[1],
        "high_threshold": row[2],
        "min_threshold": row[1],
        "max_threshold": row[2],
        "webhook_url": row[3],
        "push_enabled": bool(row[4])
    }), 200

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

# ═══════════════════════════════════════════════
# 新增端点 (Agent-B请求)
# ═══════════════════════════════════════════════

# ─── API 7: GET /api/alerts (告警历史) ───
@app.route('/api/alerts', methods=['GET'])
def get_alerts():
    """获取告警历史列表"""
    limit = request.args.get('limit', 20, type=int)
    db = get_db()
    rows = db.execute(
        "SELECT id, type, moisture, threshold, timestamp, notified FROM alert_history ORDER BY timestamp DESC LIMIT ?",
        (limit,)
    ).fetchall()
    result = [{
        "id": r[0],
        "type": r[1],
        "moisture": r[2],
        "threshold": r[3],
        "timestamp": r[4],
        "notified": bool(r[5])
    } for r in rows]
    return jsonify(result), 200

# ─── API 8: POST/GET/DELETE /api/watering (浇水记录) ───
@app.route('/api/watering', methods=['POST'])
def watering_create():
    """记录浇水事件"""
    data = request.get_json(force=True)
    if not data:
        return jsonify({"status": "error", "message": "request body required"}), 400
    
    ts = data.get('time', datetime.utcnow().isoformat() + 'Z')
    db = get_db()
    cursor = db.execute(
        "INSERT INTO watering_records (time, moisture_before, moisture_after) VALUES (?, ?, ?)",
        (ts, data.get('moisture_before'), data.get('moisture_after'))
    )
    db.commit()
    return jsonify({"id": cursor.lastrowid, "status": "ok"}), 200

@app.route('/api/watering', methods=['GET'])
def watering_list():
    """获取浇水历史"""
    limit = request.args.get('limit', 50, type=int)
    db = get_db()
    rows = db.execute(
        "SELECT id, time, moisture_before, moisture_after FROM watering_records ORDER BY time DESC LIMIT ?",
        (limit,)
    ).fetchall()
    result = [{
        "id": r[0],
        "time": r[1],
        "moisture_before": r[2],
        "moisture_after": r[3]
    } for r in rows]
    return jsonify(result), 200

@app.route('/api/watering/<int:record_id>', methods=['DELETE'])
@app.route('/api/watering', methods=['DELETE'])
def watering_delete(record_id=None):
    """删除浇水记录（支持路径参数和查询参数两种格式）"""
    if record_id is None:
        record_id = request.args.get('id', type=int)
    if not record_id:
        return jsonify({"status": "error", "message": "id required (path /api/watering/{id} or ?id=)"}), 400
    
    db = get_db()
    db.execute("DELETE FROM watering_records WHERE id=?", (record_id,))
    db.commit()
    return jsonify({"status": "ok"}), 200

# ─── 健康检查 ───
@app.route('/api/health', methods=['GET'])
def health():
    return jsonify({"status": "ok", "time": datetime.utcnow().isoformat() + 'Z'}), 200

# ─── 启动 ───
if __name__ == '__main__':
    init_db()
    port = int(os.environ.get('HUMIDITY_PORT', 9876))
    print(f"🌱 Humidity Sensor Backend starting on 0.0.0.0:{port}")
    app.run(host='0.0.0.0', port=port, debug=True)
