import { Network } from '@capacitor/network'
import { Preferences } from '@capacitor/preferences'
import { PushNotifications } from '@capacitor/push-notifications'
import { App } from '@capacitor/app'

// ========== Network 状态监听 ==========
let networkStatus = { connected: true, connectionType: 'unknown' }

export async function initNetwork(onChange) {
  try {
    const status = await Network.getStatus()
    networkStatus = { connected: status.connected, connectionType: status.connectionType }
    if (onChange) onChange(networkStatus)

    Network.addListener('networkStatusChange', (status) => {
      networkStatus = { connected: status.connected, connectionType: status.connectionType }
      if (onChange) onChange(networkStatus)
    })
  } catch (e) {
    // Web环境fallback: 用browser事件
    networkStatus = { connected: navigator.onLine, connectionType: 'wifi' }
    window.addEventListener('online', () => {
      networkStatus.connected = true
      if (onChange) onChange(networkStatus)
    })
    window.addEventListener('offline', () => {
      networkStatus.connected = false
      if (onChange) onChange(networkStatus)
    })
  }
  return networkStatus
}

export function getNetworkStatus() {
  return networkStatus
}

// ========== Preferences 服务器地址配置 ==========
const SERVER_URL_KEY = 'server_url'
const DEFAULT_SERVER_URL = 'http://192.168.199.233:5000/api'

export async function getServerUrl() {
  try {
    const { value } = await Preferences.get({ key: SERVER_URL_KEY })
    return value || DEFAULT_SERVER_URL
  } catch (e) {
    return localStorage.getItem(SERVER_URL_KEY) || DEFAULT_SERVER_URL
  }
}

export async function setServerUrl(url) {
  try {
    await Preferences.set({ key: SERVER_URL_KEY, value: url })
  } catch (e) {
    localStorage.setItem(SERVER_URL_KEY, url)
  }
}

export async function clearServerUrl() {
  try {
    await Preferences.remove({ key: SERVER_URL_KEY })
  } catch (e) {
    localStorage.removeItem(SERVER_URL_KEY)
  }
}

// ========== Push Notifications ==========
export async function initPush(onNotification) {
  // TODO: Firebase未配置(缺google-services.json)，暂跳过Push注册避免原生层崩溃
  // 配置Firebase后再启用: 需google-services.json + com.google.gms.google-services插件
  console.warn('Push notifications skipped: Firebase not configured (missing google-services.json)')
  return
  /* 原始代码，Firebase配置后恢复:
  try {
    const permResult = await PushNotifications.requestPermissions()
    if (permResult.receive !== 'granted') {
      console.warn('Push permission not granted')
      return
    }

    await PushNotifications.register()

    PushNotifications.addListener('registration', (token) => {
      console.log('Push registration token:', token.value)
      // TODO: 将token发送到后端存储
    })

    PushNotifications.addListener('registrationError', (error) => {
      console.error('Push registration error:', error)
    })

    PushNotifications.addListener('pushNotificationReceived', (notification) => {
      console.log('Push received:', notification)
      if (onNotification) onNotification(notification)
    })

    PushNotifications.addListener('pushNotificationActionPerformed', (action) => {
      console.log('Push action:', action)
    })
  } catch (e) {
    console.warn('Push notifications not available (web environment)')
  }
  */
}

// ========== App 生命周期 ==========
export async function initAppLifecycle(onBackButton) {
  try {
    App.addListener('backButton', (data) => {
      if (onBackButton) {
        onBackButton(data)
      } else {
        App.exitApp()
      }
    })
  } catch (e) {
    // Web环境无App插件
  }
}

// ========== 统一初始化 ==========
export async function initCapacitorPlugins({ onNetworkChange, onPushNotification, onBackButton }) {
  const netStatus = await initNetwork(onNetworkChange)
  await initPush(onPushNotification)
  await initAppLifecycle(onBackButton)
  return { networkStatus: netStatus }
}
