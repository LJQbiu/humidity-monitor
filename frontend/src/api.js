import axios from 'axios'

const api = axios.create({
  baseURL: '/api',
  timeout: 10000,
  headers: { 'Content-Type': 'application/json' }
})

// ========== 传感器数据 ==========

/** 提交传感器数据 */
export function postData(sensorReading) {
  return api.post('/data', sensorReading)
}

/** 获取历史数据 */
export function getHistory(hours = 24, limit = 100) {
  return api.get('/history', { params: { hours, limit } })
}

// ========== 统计 ==========

/** 获取统计数据 */
export function getStats() {
  return api.get('/stats')
}

// ========== 告警 ==========

/** 获取告警列表 */
export function getAlerts(limit = 20) {
  return api.get('/alerts', { params: { limit } })
}

/** 获取告警配置 */
export function getAlertConfig() {
  return api.get('/alerts/config')
}

/** 更新告警配置 */
export function updateAlertConfig(config) {
  return api.put('/alerts/config', config)
}

// ========== 浇水 ==========

/** 记录浇水事件 */
export function postWatering(record) {
  return api.post('/watering', record)
}

/** 获取浇水历史 */
export function getWatering(limit = 50) {
  return api.get('/watering', { params: { limit } })
}

/** 删除浇水记录 */
export function deleteWatering(id) {
  return api.delete(`/watering/${id}`)
}

// ========== 推送 ==========

/** 注册设备推送令牌 */
export function registerPushToken(token, platform = 'android') {
  return api.post('/push/register', { token, platform })
}

// ========== 健康检查 ==========

/** 健康检查 */
export function getHealth() {
  return api.get('/health')
}

// ========== 错误处理 ==========

api.interceptors.response.use(
  res => res.data,
  err => {
    const msg = err.response?.data?.error || err.message || '网络错误'
    console.error('[API]', msg)
    throw new Error(msg)
  }
)

export default api
