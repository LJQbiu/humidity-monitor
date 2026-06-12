<template>
  <div class="app">
    <header class="app-header">
      <h1>🌱 湿度监控</h1>
    </header>
    <main class="app-body">
      <!-- 当前湿度卡片 -->
      <section class="card current-card" v-if="stats">
        <div class="moisture-ring">
          <span class="moisture-value">{{ stats.latest_moisture ?? '--' }}</span>
          <span class="moisture-unit">%</span>
        </div>
        <div class="meta-info">
          <span>电压: {{ stats.latest_voltage ?? '--' }}V</span>
          <span>ADC: {{ stats.latest_adc ?? '--' }}</span>
          <span>{{ formatTime(stats.latest_time) }}</span>
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
          <button @click="saveAlertConfig" :disabled="loading">保存</button>
        </div>
      </section>

      <!-- 告警历史 -->
      <section class="card alerts-card" v-if="alerts.length">
        <h2>⚠️ 告警历史</h2>
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
import { ref, onMounted, onUnmounted } from 'vue'
import api from './api'
import { initCapacitorPlugins, getServerUrl, setServerUrl, getNetworkStatus } from './plugins/capacitor'

const stats = ref(null)
const waterings = ref([])
const alerts = ref([])
const alertConfig = ref(null)
const loading = ref(false)
const errorMsg = ref('')
const isOnline = ref(true)
const serverUrl = ref('/api')
let refreshTimer = null

async function fetchData() {
  try {
    errorMsg.value = ''
    const [s, w, a, ac] = await Promise.all([
      api.get('/stats'),
      api.get('/watering'),
      api.get('/alerts'),
      api.get('/alerts/config')
    ])
    stats.value = s.data
    waterings.value = w.data
    alerts.value = a.data
    alertConfig.value = ac.data
  } catch (e) {
    errorMsg.value = e.response?.data?.error || e.message || '连接失败'
  }
}

async function recordWatering() {
  loading.value = true
  try {
    const moisture = stats.value?.latest_moisture ?? 0
    await api.post('/watering', { moisture_before: moisture })
    await fetchData()
  } catch (e) {
    errorMsg.value = '记录失败: ' + (e.response?.data?.error || e.message)
  }
  loading.value = false
}

async function deleteWatering(id) {
  try {
    await api.delete(`/watering/${id}`)
    await fetchData()
  } catch (e) {
    errorMsg.value = '删除失败'
  }
}

async function saveAlertConfig() {
  loading.value = true
  try {
    await api.put('/alerts/config', alertConfig.value)
    await fetchData()
  } catch (e) {
    errorMsg.value = '保存失败'
  }
  loading.value = false
}

function formatTime(ts) {
  if (!ts) return '--'
  const d = new Date(ts)
  return d.toLocaleString('zh-CN', { month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit' })
}

onMounted(async () => {
  // 初始化Capacitor插件
  try {
    serverUrl.value = await getServerUrl()
    const { networkStatus: ns } = await initCapacitorPlugins({
      onNetworkChange: (status) => { isOnline.value = status.connected },
      onPushNotification: (notification) => {
        // 推送到达时刷新数据
        fetchData()
      },
      onBackButton: () => {
        // Android返回键：不做退出，仅提示
        errorMsg.value = '再按一次退出'
      }
    })
    isOnline.value = ns.connected
  } catch (e) {
    console.warn('Capacitor plugins init skipped (web env):', e.message)
  }
  fetchData()
  refreshTimer = setInterval(fetchData, 30000)
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
}

* { margin: 0; padding: 0; box-sizing: border-box; }

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
  background: var(--bg);
  color: #333;
}

.app {
  max-width: 480px;
  margin: 0 auto;
  padding: 16px;
}

.app-header {
  text-align: center;
  padding: 12px 0;
  color: var(--green-dark);
}

.app-header h1 { font-size: 1.4rem; }

.card {
  background: var(--card-bg);
  border-radius: 12px;
  padding: 16px;
  margin-bottom: 12px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.card h2 { font-size: 1rem; margin-bottom: 12px; color: #555; }

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
.moisture-unit { font-size: 1rem; color: #888; margin-left: 2px; }

.meta-info {
  display: flex;
  gap: 12px;
  justify-content: center;
  font-size: 0.85rem;
  color: #888;
}

/* 统计网格 */
.stats-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
  font-size: 0.9rem;
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
}

.alert-form button {
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

.status-msg {
  padding: 8px;
  background: #ffebee;
  border-radius: 6px;
  color: var(--red);
  font-size: 0.85rem;
  text-align: center;
}

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
</style>
