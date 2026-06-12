<template>
  <div class="app" :class="{ dark: isDark }">
    <header class="app-header">
      <h1>🌱 湿度监控</h1>
      <div class="header-actions">
        <button class="icon-btn" @click="toggleDark" title="深色模式">
          {{ isDark ? '☀️' : '🌙' }}
        </button>
        <button class="icon-btn" @click="fetchData" :disabled="loading" title="刷新">
          🔄
        </button>
      </div>
    </header>

    <main class="app-body">
      <!-- 当前湿度卡片 -->
      <section class="card current-card" v-if="stats">
        <div class="moisture-ring" :style="ringStyle">
          <span class="moisture-value">{{ stats.latest_moisture ?? '--' }}</span>
          <span class="moisture-unit">%</span>
        </div>
        <div class="meta-info">
          <span>电压: {{ stats.latest_voltage ?? '--' }}V</span>
          <span>ADC: {{ stats.latest_adc ?? '--' }}</span>
          <span>{{ formatTime(stats.latest_time) }}</span>
        </div>
        <div class="refresh-badge" v-if="lastRefresh">
          更新于 {{ lastRefreshStr }}
        </div>
      </section>

      <!-- 统计摘要 -->
      <section class="card stats-card" v-if="stats">
        <h2>📊 统计 ({{ stats.period_hours }}h)</h2>
        <div class="stats-grid">
          <div>平均: {{ stats.avg_moisture?.toFixed(1) }}%</div>
          <div>最低: {{ stats.min_moisture?.toFixed(1) }}%</div>
          <div>最高: {{ stats.max_moisture?.toFixed(1) }}%</div>
          <div>读数: {{ stats.readings_count }}</div>
        </div>
      </section>

      <!-- 历史图表 -->
      <section class="card history-card">
        <h2>📈 历史趋势</h2>
        <div class="range-selector">
          <button :class="{ active: historyRange === 24 }" @click="changeRange(24)">24h</button>
          <button :class="{ active: historyRange === 168 }" @click="changeRange(168)">7d</button>
          <button :class="{ active: historyRange === 720 }" @click="changeRange(720)">30d</button>
        </div>
        <div class="chart-container" v-if="historyData.length">
          <div class="chart-y-axis">
            <span>{{ chartMax }}%</span>
            <span>{{ chartMin }}%</span>
          </div>
          <div class="chart-bars">
            <div class="chart-bar-group" v-for="(pt, i) in chartPoints" :key="i">
              <div class="chart-bar"
                :style="{ height: pt.height + '%', bottom: pt.bottom + '%', background: pt.color }"
                :title="`${formatTimeShort(pt.time)}: ${pt.value}%`">
              </div>
              <span class="chart-label" v-if="i % labelStep === 0">{{ formatTimeShort(pt.time) }}</span>
            </div>
          </div>
        </div>
        <div class="empty-msg" v-else>暂无历史数据</div>
      </section>

      <!-- 浇水按钮 -->
      <section class="card action-card">
        <h2>💧 浇水记录</h2>
        <button class="water-btn" @click="recordWatering" :disabled="loading">
          {{ loading ? '提交中...' : '记录浇水' }}
        </button>
        <div class="watering-list" v-if="waterings.length">
          <div class="watering-item" v-for="w in waterings" :key="w.id">
            <span>{{ formatTime(w.time) }} — {{ w.moisture_before }}%</span>
            <button class="del-btn" @click="deleteWatering(w.id)">✕</button>
          </div>
        </div>
      </section>

      <!-- 告警配置 -->
      <section class="card alert-card" v-if="alertConfig">
        <h2>🔔 告警设置</h2>
        <div class="alert-form">
          <label>
            <input type="checkbox" v-model="alertConfig.enabled" /> 启用告警
          </label>
          <label>
            低阈值: <input type="number" v-model.number="alertConfig.low_threshold" min="0" max="100" />
          </label>
          <label>
            高阈值: <input type="number" v-model.number="alertConfig.high_threshold" min="0" max="100" />
          </label>
          <label>
            <input type="checkbox" v-model="alertConfig.push_enabled" /> 推送通知
          </label>
          <button class="save-btn" @click="saveAlertConfig" :disabled="loading">保存</button>
        </div>
      </section>

      <!-- 告警历史 -->
      <section class="card alerts-card" v-if="alerts.length">
        <h2>⚠️ 告警历史 <button class="clear-btn" @click="clearAlerts" title="清除已读">🗑️</button></h2>
        <div class="alert-item" v-for="a in alerts" :key="a.id">
          <span :class="['alert-type', a.type]">{{ a.type === 'low' ? '偏低' : '偏高' }}</span>
          <span>{{ a.moisture }}% / 阈值{{ a.threshold }}%</span>
          <span>{{ formatTime(a.timestamp) }}</span>
        </div>
      </section>

      <!-- 加载/错误状态 -->
      <div class="status-msg" v-if="errorMsg">
        ❌ {{ errorMsg }}
      </div>
    </main>

    <!-- 离线提示 -->
    <div class="offline-bar" v-if="!isOnline">
      📡 网络断开 — 数据可能过时
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { getStats, getWatering, getAlerts, getAlertConfig, getHistory, postWatering, deleteWatering as apiDeleteWatering, updateAlertConfig } from './api'
import { initCapacitorPlugins, getServerUrl, setServerUrl, getNetworkStatus } from './plugins/capacitor'

// ========== 状态 ==========
const stats = ref(null)
const waterings = ref([])
const alerts = ref([])
const alertConfig = ref(null)
const historyData = ref([])
const loading = ref(false)
const errorMsg = ref('')
const isOnline = ref(true)
const isDark = ref(false)
const historyRange = ref(24)
const lastRefresh = ref(null)
const serverUrl = ref('')
let refreshTimer = null

// ========== 计算属性 ==========
const lastRefreshStr = computed(() => {
  if (!lastRefresh.value) return ''
  return new Date(lastRefresh.value).toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
})

const ringStyle = computed(() => {
  const m = stats.value?.latest_moisture
  if (m == null) return {}
  let color = '#4CAF50'
  if (m < 30) color = '#F44336'
  else if (m < 50) color = '#FF9800'
  return { borderColor: color }
})

// 历史图表计算
const chartPoints = computed(() => {
  if (!historyData.value.length) return []
  const vals = historyData.value.map(d => d.moisture ?? 0)
  const mn = Math.min(...vals)
  const mx = Math.max(...vals)
  const range = mx - mn || 1
  const maxBars = 60
  const step = Math.max(1, Math.ceil(historyData.value.length / maxBars))
  const points = []
  for (let i = 0; i < historyData.value.length; i += step) {
    const d = historyData.value[i]
    const v = d.moisture ?? 0
    const norm = (v - mn) / range * 80 + 10  // 10-90% height range
    let color = '#4CAF50'
    if (v < 30) color = '#F44336'
    else if (v < 50) color = '#FF9800'
    points.push({
      value: v,
      time: d.timestamp,
      height: norm,
      bottom: 5,
      color
    })
  }
  return points
})

const chartMax = computed(() => {
  if (!historyData.value.length) return 100
  return Math.max(...historyData.value.map(d => d.moisture ?? 0)).toFixed(0)
})

const chartMin = computed(() => {
  if (!historyData.value.length) return 0
  return Math.min(...historyData.value.map(d => d.moisture ?? 0)).toFixed(0)
})

const labelStep = computed(() => {
  const len = chartPoints.value.length
  if (len <= 10) return 1
  if (len <= 20) return 3
  return Math.ceil(len / 8)
})

// ========== 数据获取 ==========
async function fetchData() {
  try {
    errorMsg.value = ''
    const [s, w, a, ac] = await Promise.all([
      getStats(),
      getWatering(),
      getAlerts(),
      getAlertConfig()
    ])
    stats.value = s
    waterings.value = w
    alerts.value = a
    alertConfig.value = ac
    lastRefresh.value = Date.now()
  } catch (e) {
    errorMsg.value = e.message || '连接失败'
  }
}

async function fetchHistory() {
  try {
    const data = await getHistory(historyRange.value, 200)
    historyData.value = data
  } catch (e) {
    console.warn('历史数据获取失败:', e.message)
  }
}

function changeRange(hours) {
  historyRange.value = hours
  fetchHistory()
}

// ========== 操作 ==========
async function recordWatering() {
  loading.value = true
  try {
    const moisture = stats.value?.latest_moisture ?? 0
    await postWatering({ moisture_before: moisture })
    await fetchData()
  } catch (e) {
    errorMsg.value = '记录失败: ' + e.message
  }
  loading.value = false
}

async function deleteWatering(id) {
  try {
    await apiDeleteWatering(id)
    await fetchData()
  } catch (e) {
    errorMsg.value = '删除失败'
  }
}

async function saveAlertConfig() {
  loading.value = true
  try {
    await updateAlertConfig(alertConfig.value)
    await fetchData()
  } catch (e) {
    errorMsg.value = '保存失败'
  }
  loading.value = false
}

function clearAlerts() {
  alerts.value = []
}

function toggleDark() {
  isDark.value = !isDark.value
  try { localStorage.setItem('darkMode', isDark.value) } catch {}
}

// ========== 格式化 ==========
function formatTime(ts) {
  if (!ts) return '--'
  const d = new Date(ts)
  return d.toLocaleString('zh-CN', { month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit' })
}

function formatTimeShort(ts) {
  if (!ts) return ''
  const d = new Date(ts)
  if (historyRange.value <= 24) return d.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' })
  return d.toLocaleDateString('zh-CN', { month: '2-digit', day: '2-digit' })
}

// ========== 生命周期 ==========
onMounted(async () => {
  // 深色模式恢复
  try {
    const saved = localStorage.getItem('darkMode')
    if (saved === 'true') isDark.value = true
  } catch {}

  // Capacitor插件初始化
  try {
    serverUrl.value = await getServerUrl()
    const { networkStatus: ns } = await initCapacitorPlugins({
      onNetworkChange: (status) => { isOnline.value = status.connected },
      onPushNotification: () => { fetchData() },
      onBackButton: () => { errorMsg.value = '再按一次退出' }
    })
    isOnline.value = ns.connected
  } catch (e) {
    console.warn('Capacitor plugins init skipped (web env):', e.message)
  }

  fetchData()
  fetchHistory()
  refreshTimer = setInterval(() => {
    fetchData()
    fetchHistory()
  }, 30000)
})

onUnmounted(() => {
  if (refreshTimer) clearInterval(refreshTimer)
})
</script>

<style>
:root {
  --green: #4CAF50;
  --green-dark: #388E3C;
  --red: #F44336;
  --orange: #FF9800;
  --bg: #f5f5f5;
  --card-bg: #fff;
  --text: #333;
  --text2: #888;
  --shadow: rgba(0,0,0,0.1);
}

.dark {
  --green: #66BB6A;
  --green-dark: #81C784;
  --red: #EF5350;
  --orange: #FFA726;
  --bg: #1a1a2e;
  --card-bg: #16213e;
  --text: #e0e0e0;
  --text2: #9e9e9e;
  --shadow: rgba(0,0,0,0.3);
}

* { margin: 0; padding: 0; box-sizing: border-box; }

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
  background: var(--bg);
  color: var(--text);
}

.app {
  max-width: 480px;
  margin: 0 auto;
  padding: 16px;
  min-height: 100vh;
}

/* Header */
.app-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 0;
  color: var(--green-dark);
}

.app-header h1 { font-size: 1.4rem; }

.header-actions { display: flex; gap: 8px; }

.icon-btn {
  background: none; border: none; font-size: 1.2rem;
  cursor: pointer; padding: 4px 8px; border-radius: 8px;
  color: var(--text);
}

.icon-btn:hover { background: var(--shadow); }
.icon-btn:disabled { opacity: 0.5; }

/* Cards */
.card {
  background: var(--card-bg);
  border-radius: 12px;
  padding: 16px;
  margin-bottom: 12px;
  box-shadow: 0 2px 4px var(--shadow);
}

.card h2 { font-size: 1rem; margin-bottom: 12px; color: var(--text2); }

/* 湿度环 */
.current-card { text-align: center; }

.moisture-ring {
  display: inline-flex;
  align-items: baseline;
  justify-content: center;
  width: 120px;
  height: 120px;
  border-radius: 50%;
  border: 6px solid var(--green);
  margin: 8px auto;
}

.moisture-value { font-size: 2.4rem; font-weight: bold; color: var(--green-dark); }
.moisture-unit { font-size: 1rem; color: var(--text2); margin-left: 2px; }

.meta-info {
  display: flex;
  gap: 12px;
  justify-content: center;
  font-size: 0.85rem;
  color: var(--text2);
}

.refresh-badge {
  font-size: 0.75rem;
  color: var(--text2);
  margin-top: 8px;
}

/* 统计网格 */
.stats-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
  font-size: 0.9rem;
}

/* 历史图表 */
.range-selector {
  display: flex;
  gap: 8px;
  margin-bottom: 12px;
}

.range-selector button {
  padding: 4px 12px;
  border: 1px solid var(--green);
  border-radius: 6px;
  background: transparent;
  color: var(--text);
  cursor: pointer;
  font-size: 0.85rem;
}

.range-selector button.active {
  background: var(--green);
  color: white;
}

.chart-container {
  display: flex;
  gap: 4px;
  height: 160px;
}

.chart-y-axis {
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  font-size: 0.7rem;
  color: var(--text2);
  width: 30px;
  text-align: right;
}

.chart-bars {
  flex: 1;
  display: flex;
  gap: 2px;
  align-items: stretch;
  position: relative;
}

.chart-bar-group {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  position: relative;
  min-width: 0;
}

.chart-bar {
  width: 100%;
  min-height: 3px;
  border-radius: 2px 2px 0 0;
  position: absolute;
  transition: height 0.3s;
}

.chart-label {
  font-size: 0.65rem;
  color: var(--text2);
  position: absolute;
  bottom: -14px;
  white-space: nowrap;
}

.empty-msg {
  text-align: center;
  color: var(--text2);
  font-size: 0.85rem;
  padding: 16px;
}

/* 浇水 */
.water-btn {
  width: 100%;
  padding: 12px;
  background: var(--green);
  color: white;
  border: none;
  border-radius: 8px;
  font-size: 1rem;
  cursor: pointer;
  margin-bottom: 8px;
}

.water-btn:disabled { opacity: 0.6; }

.watering-item {
  display: flex;
  justify-content: space-between;
  padding: 6px 0;
  font-size: 0.85rem;
}

.del-btn {
  background: none;
  border: none;
  color: var(--red);
  cursor: pointer;
  font-size: 1rem;
}

/* 告警 */
.alert-form {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.alert-form label { font-size: 0.9rem; display: flex; align-items: center; gap: 8px; }

.alert-form input[type="number"] {
  width: 60px;
  padding: 4px;
  border: 1px solid #ddd;
  border-radius: 4px;
  background: var(--card-bg);
  color: var(--text);
}

.save-btn {
  padding: 8px;
  background: var(--green);
  color: white;
  border: none;
  border-radius: 6px;
  cursor: pointer;
}

.alert-type { font-weight: bold; }
.alert-type.low { color: var(--orange); }
.alert-type.high { color: var(--red); }

.alert-item {
  padding: 4px 0;
  font-size: 0.85rem;
  display: flex;
  gap: 8px;
}

.alerts-card h2 {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.clear-btn {
  background: none; border: none;
  font-size: 0.9rem; cursor: pointer;
  color: var(--text2);
}

.status-msg {
  padding: 8px;
  background: #ffebee;
  border-radius: 6px;
  color: var(--red);
  font-size: 0.85rem;
  text-align: center;
}

.dark .status-msg { background: #2a1a1a; }

.offline-bar {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  padding: 8px;
  background: #ff9800;
  color: white;
  text-align: center;
  font-size: 0.85rem;
  z-index: 100;
}

/* 响应式 */
@media (max-width: 360px) {
  .app { padding: 8px; }
  .moisture-ring { width: 96px; height: 96px; border-width: 4px; }
  .moisture-value { font-size: 1.8rem; }
  .card { padding: 12px; border-radius: 8px; }
  .chart-container { height: 120px; }
  .chart-label { font-size: 0.55rem; }
}

@media (min-width: 768px) {
  .app { max-width: 600px; }
  .moisture-ring { width: 140px; height: 140px; }
  .moisture-value { font-size: 2.8rem; }
}
</style>
