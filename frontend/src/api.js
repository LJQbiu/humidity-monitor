import axios from 'axios'
import { getServerUrl } from './plugins/capacitor.js'

// 动态获取baseURL：浏览器开发时用'/api'(vite proxy)，APK里用完整服务器地址
let _baseUrl = '/api'  // 默认值（浏览器+vite proxy可用）

const api = axios.create({
  baseURL: _baseUrl,
  timeout: 10000,
  headers: { 'Content-Type': 'application/json' }
})

/** 初始化时调用，从Preferences/localStorage读取服务器地址并更新baseURL */
export async function initApiBaseUrl() {
  const url = await getServerUrl()
  if (url && url !== '/api') {
    api.defaults.baseURL = url
    _baseUrl = url
  }
  console.log('[API] baseURL set to:', api.defaults.baseURL)
  return api.defaults.baseURL
}

/** 修改服务器地址（配置界面用） */
export async function updateApiBaseUrl(newUrl) {
  api.defaults.baseURL = newUrl
  _baseUrl = newUrl
  console.log('[API] baseURL updated to:', newUrl)
}

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
