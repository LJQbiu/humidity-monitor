<template>
  <div class="app" :class="{ dark: isDark }">
    <!-- Header -->
    <header class="app-header">
      <div class="header-brand">
        <FlowerFairy :moisture="stats?.latest_moisture ?? null" :size="38" />
        <h1>花语湿度</h1>
      </div>
      <div class="header-actions">
        <button class="icon-btn" @click="toggleDark" title="深色模式">
          {{ isDark ? '☀️' : '🌙' }}
        </button>
        <button class="icon-btn" @click="fetchData" :disabled="loading" title="刷新">
          🔄
        </button>
        <button class="icon-btn" @click="showServerConfig = true" title="服务器设置">
          ⚙️
        </button>
      </div>
    </header>

    <main class="app-body">
      <!-- Hero: Fairy + Moisture Ring -->
      <section class="card hero-card" v-if="stats">
        <div class="hero-scene">
          <FlowerFairy :moisture="stats.latest_moisture ?? null" :size="120" />
          <div class="moisture-ring" :style="ringStyle">
            <span class="moisture-value">{{ stats.latest_moisture ?? '--' }}</span>
            <span class="moisture-unit">%</span>
          </div>
        </div>
        <p class="fairy-says">{{ fairyMessage }}</p>
        <div class="meta-row">
          <span class="meta-item">⚡ {{ stats.latest_voltage ?? '--' }}V</span>
          <span class="meta-item">📊 ADC {{ stats.latest_adc ?? '--' }}</span>
          <span class="meta-item">🕐 {{ formatTime(stats.latest_time) }}</span>
        </div>
        <div class="refresh-badge" v-if="lastRefresh">
          更新于 {{ lastRefreshStr }}
        </div>
      </section>

      <!-- Stats Grid -->
      <section class="card stats-card" v-if="stats">
        <h2>🌿 数据概览</h2>
        <div class="stats-grid">
          <div class="stat-cell">
            <div class="stat-num">{{ stats.avg_moisture?.toFixed(1) }}</div>
            <div class="stat-label">平均湿度%</div>
          </div>
          <div class="stat-cell">
            <div class="stat-num low">{{ stats.min_moisture?.toFixed(1) }}</div>
            <div class="stat-label">最低%</div>
          </div>
          <div class="stat-cell">
            <div class="stat-num high">{{ stats.max_moisture?.toFixed(1) }}</div>
            <div class="stat-label">最高%</div>
          </div>
          <div class="stat-cell">
            <div class="stat-num">{{ stats.readings_count }}</div>
            <div class="stat-label">读数次数</div>
          </div>
        </div>
      </section>

      <!-- History Chart -->
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

      <!-- Watering -->
      <section class="card action-card">
        <h2>💧 浇水记录</h2>
        <button class="water-btn" @click="recordWatering" :disabled="loading">
          {{ loading ? '提交中...' : '记录浇水' }}
        </button>
        <div class="watering-list" v-if="waterings.length">
          <div class="watering-item" v-for="w in waterings" :key="w.id">
            <span>{{ formatTime(w.time) }} — {{ w.moisture_before ?? '未知' }}{{ w.moisture_before != null ? '%' : '' }}{{ w.moisture_after != null ? ' → '+w.moisture_after+'%' : '' }}</span>
            <button class="del-btn" @click="deleteWatering(w.id)">✕</button>
          </div>
        </div>
      </section>

      <!-- Alert Config -->
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

      <!-- Alert History -->
      <section class="card alerts-card" v-if="alerts.length">
        <h2>⚠️ 告警历史 <button class="clear-btn" @click="clearAlerts" title="清除已读">🗑️</button></h2>
        <div class="alert-item" v-for="a in alerts" :key="a.id">
          <span :class="['alert-type', a.type]">{{ a.type === 'low' ? '偏低' : '偏高' }}</span>
          <span>{{ a.moisture }}% / 阈值{{ a.threshold }}%</span>
          <span>{{ formatTime(a.timestamp) }}</span>
        </div>
      </section>

      <div class="status-msg" v-if="errorMsg">
        ❌ {{ errorMsg }}
      </div>
    </main>

    <div class="offline-bar" v-if="!isOnline">
      📡 网络断开 — 数据可能过时
    </div>

    <!-- Server Config Modal -->
    <div class="modal-overlay" v-if="showServerConfig" @click.self="showServerConfig = false">
      <div class="modal-card">
        <h2>⚙️ 服务器设置</h2>
        <p class="modal-hint">当前地址：{{ currentServerUrl }}</p>
        <input class="modal-input" v-model="serverUrlInput" placeholder="http://192.168.x.x:5000/api" />
        <div class="modal-actions">
          <button class="modal-btn save" @click="saveServerConfig">保存并连接</button>
          <button class="modal-btn cancel" @click="showServerConfig = false">取消</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { getStats, getWatering, getAlerts, getAlertConfig, getHistory, postWatering, deleteWatering as apiDeleteWatering, updateAlertConfig, initApiBaseUrl, updateApiBaseUrl } from './api'
import { initCapacitorPlugins, getServerUrl, setServerUrl, getNetworkStatus } from './plugins/capacitor'
import FlowerFairy from './components/FlowerFairy.vue'

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
const currentServerUrl = ref('')
const showServerConfig = ref(false)
const serverUrlInput = ref('')
let refreshTimer = null

// ========== 计算属性 ==========
const lastRefreshStr = computed(() => {
  if (!lastRefresh.value) return ''
  return new Date(lastRefresh.value).toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
})

const fairyMessage = computed(() => {
  const m = stats.value?.latest_moisture
  if (m == null) return '等待数据...'
  if (m >= 60) return '土壤很湿润，花朵们很开心~ 🌸'
  if (m >= 40) return '湿度适中，我正在努力生长~ 🌿'
  if (m >= 25) return '有点干了，渴渴的...给我水~ 🥀'
  return '好干啊！快浇水救救小花！😭'
})

const ringStyle = computed(() => {
  const m = stats.value?.latest_moisture
  if (m == null) return { borderColor: '#666' }
  let color = '#4CAF50'
  if (m < 25) color = '#F44336'
  else if (m < 40) color = '#FF5722'
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
    const norm = (v - mn) / range * 80 + 10
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

async function saveServerConfig() {
  const newUrl = serverUrlInput.value.trim()
  if (!newUrl) return
  loading.value = true
  try {
    await setServerUrl(newUrl)
    await updateApiBaseUrl(newUrl)
    currentServerUrl.value = newUrl
    showServerConfig.value = false
    await fetchData()
    await fetchHistory()
  } catch (e) {
    errorMsg.value = '连接失败: ' + e.message
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
  try {
    const saved = localStorage.getItem('darkMode')
    if (saved === 'true') isDark.value = true
  } catch {}

  try {
    currentServerUrl.value = await getServerUrl()
    serverUrlInput.value = currentServerUrl.value
    const { networkStatus: ns } = await initCapacitorPlugins({
      onNetworkChange: (status) => { isOnline.value = status.connected },
      onPushNotification: () => { fetchData() },
      onBackButton: () => { errorMsg.value = '再按一次退出' }
    })
    isOnline.value = ns.connected
  } catch (e) {
    console.warn('Capacitor plugins init skipped (web env):', e.message)
  }

  try {
    await initApiBaseUrl()
  } catch (e) {
    console.warn('initApiBaseUrl failed:', e.message)
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
  --green-light: #81C784;
  --green-dark: #388E3C;
  --red: #F44336;
  --orange: #FF9800;
  --blue: #42A5F5;
  --bg: #f8faf5;
  --card-bg: rgba(255,255,255,0.92);
  --card-border: rgba(76,175,80,0.08);
  --text: #2d3436;
  --text2: #7f8c8d;
  --shadow: rgba(45,52,54,0.08);
  --fairy-glow: rgba(129,199,132,0.4);
}

.dark {
  --green: #66BB6A;
  --green-light: #A5D6A7;
  --green-dark: #81C784;
  --red: #EF5350;
  --orange: #FFA726;
  --blue: #42A5F5;
  --bg: #0a1628;
  --card-bg: rgba(16,28,56,0.88);
  --card-border: rgba(129,199,132,0.1);
  --text: #ecf0f1;
  --text2: #8e9aaf;
  --shadow: rgba(0,0,0,0.3);
  --fairy-glow: rgba(129,199,132,0.25);
}

* { margin: 0; padding: 0; box-sizing: border-box; }

body {
  font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, sans-serif;
  background: var(--bg);
  color: var(--text);
  -webkit-font-smoothing: antialiased;
  min-height: 100vh;
}

.app {
  max-width: 480px;
  margin: 0 auto;
  padding: 12px 16px;
  min-height: 100vh;
  background-image:
    radial-gradient(ellipse at 30% 0%, rgba(129,199,132,0.08) 0%, transparent 50%),
    radial-gradient(ellipse at 70% 100%, rgba(66,165,245,0.05) 0%, transparent 50%);
}

/* ====== Header ====== */
.app-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 0 8px;
}

.header-brand {
  display: flex;
  align-items: center;
  gap: 10px;
}

.header-brand h1 {
  font-size: 1.4rem;
  font-weight: 700;
  color: var(--green);
  letter-spacing: 1px;
}

.header-actions { display: flex; gap: 6px; }

.icon-btn {
  background: rgba(129,199,132,0.08);
  border: 1px solid rgba(129,199,132,0.12);
  font-size: 1.1rem;
  cursor: pointer;
  padding: 5px 8px;
  border-radius: 10px;
  color: var(--text);
  transition: all 0.2s;
}
.icon-btn:hover { background: rgba(129,199,132,0.18); transform: scale(1.08); }
.icon-btn:active { transform: scale(0.94); }
.icon-btn:disabled { opacity: 0.4; }

/* ====== Cards ====== */
.card {
  background: var(--card-bg);
  border: 1px solid var(--card-border);
  border-radius: 18px;
  padding: 18px;
  margin-bottom: 12px;
  box-shadow: 0 4px 16px var(--shadow);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
  transition: transform 0.25s, box-shadow 0.25s;
}
.card:hover { transform: translateY(-2px); box-shadow: 0 8px 24px var(--shadow); }
.card h2 {
  font-size: 0.95rem;
  margin-bottom: 14px;
  color: var(--text2);
  font-weight: 600;
  letter-spacing: 0.5px;
}

/* ====== Hero Card ====== */
.hero-card { text-align: center; padding: 20px 16px 16px; }

.hero-scene {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  position: relative;
}

.fairy-says {
  font-size: 0.9rem;
  color: var(--green-light);
  margin: 10px 0 6px;
  font-weight: 500;
  animation: fadeFloat 3s ease-in-out infinite;
  min-height: 1.4em;
}

@keyframes fadeFloat {
  0%, 100% { opacity: 0.85; transform: translateY(0); }
  50% { opacity: 1; transform: translateY(-3px); }
}

/* Moisture Ring */
.moisture-ring {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 120px;
  height: 120px;
  border-radius: 50%;
  border: 5px solid var(--green);
  margin: 4px auto;
  background: radial-gradient(circle, rgba(129,199,132,0.06) 0%, transparent 70%);
  box-shadow: 0 0 24px var(--fairy-glow);
  animation: ringPulse 3s ease-in-out infinite;
  transition: border-color 0.5s, box-shadow 0.5s;
}

@keyframes ringPulse {
  0%, 100% { box-shadow: 0 0 24px var(--fairy-glow); }
  50% { box-shadow: 0 0 36px var(--fairy-glow); }
}

.moisture-value {
  font-size: 2.4rem;
  font-weight: 800;
  color: var(--green-light);
  line-height: 1;
}
.moisture-unit {
  font-size: 0.8rem;
  color: var(--text2);
  margin-left: 2px;
}

/* Meta row */
.meta-row {
  display: flex;
  justify-content: center;
  gap: 12px;
  margin-top: 10px;
  font-size: 0.78rem;
  color: var(--text2);
}
.meta-item {
  background: rgba(129,199,132,0.06);
  padding: 3px 8px;
  border-radius: 8px;
}

.refresh-badge {
  font-size: 0.72rem;
  color: var(--text2);
  margin-top: 6px;
  opacity: 0.7;
}

/* ====== Stats Grid ====== */
.stats-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
}

.stat-cell {
  background: rgba(129,199,132,0.04);
  border-radius: 12px;
  padding: 12px 10px;
  text-align: center;
}

.stat-num {
  font-size: 1.5rem;
  font-weight: 700;
  color: var(--green-light);
}
.stat-num.low { color: var(--red); }
.stat-num.high { color: var(--orange); }
.stat-label {
  font-size: 0.72rem;
  color: var(--text2);
  margin-top: 4px;
}

/* ====== Chart ====== */
.range-selector {
  display: flex;
  gap: 8px;
  margin-bottom: 12px;
}
.range-selector button {
  padding: 5px 14px;
  border: 1px solid var(--card-border);
  background: transparent;
  color: var(--text2);
  border-radius: 10px;
  cursor: pointer;
  font-size: 0.85rem;
  transition: all 0.2s;
}
.range-selector button.active {
  background: var(--green);
  color: #fff;
  border-color: var(--green);
}

.chart-container {
  display: flex;
  gap: 8px;
  height: 160px;
}
.chart-y-axis {
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  font-size: 0.65rem;
  color: var(--text2);
  width: 30px;
  text-align: right;
}
.chart-bars {
  flex: 1;
  display: flex;
  align-items: flex-end;
  gap: 2px;
  overflow-x: auto;
}
.chart-bar-group {
  flex: 1;
  min-width: 6px;
  display: flex;
  flex-direction: column;
  justify-content: flex-end;
  position: relative;
  height: 100%;
}
.chart-bar {
  border-radius: 3px 3px 0 0;
  min-height: 4px;
  transition: height 0.3s;
}
.chart-label {
  font-size: 0.55rem;
  color: var(--text2);
  position: absolute;
  bottom: -16px;
  left: 50%;
  transform: translateX(-50%);
  white-space: nowrap;
}
.empty-msg {
  color: var(--text2);
  font-size: 0.85rem;
  text-align: center;
  padding: 20px 0;
}

/* ====== Watering ====== */
.water-btn {
  width: 100%;
  padding: 10px;
  background: linear-gradient(135deg, var(--green) 0%, var(--green-dark) 100%);
  color: #fff;
  border: none;
  border-radius: 12px;
  font-size: 0.95rem;
  cursor: pointer;
  transition: all 0.2s;
  margin-bottom: 10px;
}
.water-btn:hover { transform: translateY(-1px); box-shadow: 0 4px 12px rgba(76,175,80,0.3); }
.water-btn:active { transform: translateY(0); }
.water-btn:disabled { opacity: 0.5; }

.watering-list { max-height: 150px; overflow-y: auto; }
.watering-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 6px 0;
  font-size: 0.82rem;
  color: var(--text2);
  border-bottom: 1px solid rgba(129,199,132,0.06);
}
.del-btn {
  background: none; border: none; color: var(--red);
  cursor: pointer; font-size: 0.85rem; opacity: 0.6;
  transition: opacity 0.2s;
}
.del-btn:hover { opacity: 1; }

/* ====== Alerts ====== */
.alert-form {
  display: flex;
  flex-direction: column;
  gap: 8px;
  font-size: 0.85rem;
}
.alert-form label {
  display: flex;
  align-items: center;
  gap: 6px;
}
.alert-form input[type="number"] {
  width: 60px;
  padding: 4px;
  border: 1px solid var(--card-border);
  border-radius: 8px;
  background: rgba(129,199,132,0.04);
  color: var(--text);
  font-size: 0.85rem;
}
.alert-form input[type="checkbox"] {
  accent-color: var(--green);
}
.save-btn {
  padding: 8px 16px;
  background: var(--green);
  color: #fff;
  border: none;
  border-radius: 10px;
  cursor: pointer;
  font-size: 0.85rem;
  transition: all 0.2s;
}
.save-btn:hover { background: var(--green-dark); }
.save-btn:disabled { opacity: 0.5; }

.alert-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 6px 0;
  font-size: 0.82rem;
  color: var(--text2);
}
.alert-type {
  padding: 2px 8px;
  border-radius: 6px;
  font-size: 0.75rem;
  font-weight: 600;
}
.alert-type.low { background: rgba(244,67,54,0.1); color: var(--red); }
.alert-type.high { background: rgba(255,152,0,0.1); color: var(--orange); }

.clear-btn {
  background: none; border: none; font-size: 0.85rem;
  cursor: pointer; opacity: 0.6; transition: opacity 0.2s;
}
.clear-btn:hover { opacity: 1; }

.status-msg {
  color: var(--red);
  font-size: 0.85rem;
  text-align: center;
  padding: 10px;
}

.offline-bar {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  background: rgba(244,67,54,0.9);
  color: #fff;
  text-align: center;
  padding: 8px;
  font-size: 0.85rem;
}

/* ====== Modal ====== */
.modal-overlay {
  position: fixed;
  top: 0; left: 0; right: 0; bottom: 0;
  background: rgba(0,0,0,0.5);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 100;
}
.modal-card {
  background: var(--card-bg);
  border-radius: 18px;
  padding: 24px;
  width: 90%;
  max-width: 360px;
  backdrop-filter: blur(12px);
}
.modal-card h2 { margin-bottom: 12px; }
.modal-hint { font-size: 0.82rem; color: var(--text2); margin-bottom: 10px; }
.modal-input {
  width: 100%;
  padding: 10px;
  border: 1px solid var(--card-border);
  border-radius: 12px;
  background: rgba(129,199,132,0.04);
  color: var(--text);
  font-size: 0.9rem;
  margin-bottom: 12px;
}
.modal-actions { display: flex; gap: 8px; }
.modal-btn {
  padding: 10px 20px;
  border: none;
  border-radius: 10px;
  cursor: pointer;
  font-size: 0.85rem;
  transition: all 0.2s;
}
.modal-btn.save { background: var(--green); color: #fff; }
.modal-btn.save:hover { background: var(--green-dark); }
.modal-btn.cancel { background: rgba(0,0,0,0.06); color: var(--text); }
</style>
