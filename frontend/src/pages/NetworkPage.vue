<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import { api, type NetworkStatus, type NetworkConfig } from '../services/api'

const net = ref<NetworkStatus | null>(null)
const config = ref<NetworkConfig | null>(null)
const editing = ref(false)
const loading = ref(true)
const error = ref('')
const saveMsg = ref('')
const scanning = ref(false)
const scanNote = ref('')
let timer: ReturnType<typeof setInterval>

async function poll() {
  try {
    net.value = await api.getNetworkStatus()
    error.value = ''
  } catch (e: unknown) {
    error.value = (e as Error).message || 'Failed to load'
  } finally {
    loading.value = false
  }
}

async function loadConfig() {
  try {
    config.value = await api.getNetworkConfig()
    editing.value = true
  } catch (e: unknown) {
    saveMsg.value = `Error: ${(e as Error).message}`
  }
}

async function saveConfig() {
  if (!config.value) return
  try {
    const res = await api.setNetworkConfig(config.value)
    saveMsg.value = res.note || 'Saved'
    setTimeout(() => saveMsg.value = '', 5000)
  } catch (e: unknown) {
    saveMsg.value = `Error: ${(e as Error).message}`
  }
}

async function doScan() {
  scanning.value = true
  scanNote.value = ''
  try {
    const res = await api.scanNetworks()
    if (!res.supported) {
      scanNote.value = res.note
    } else {
      scanNote.value = `Found ${res.networks.length} networks`
    }
  } catch (e: unknown) {
    scanNote.value = `Error: ${(e as Error).message}`
  } finally {
    scanning.value = false
  }
}

onMounted(() => { poll(); timer = setInterval(poll, 5000) })
onUnmounted(() => clearInterval(timer))
</script>

<template>
  <div class="network-page">
    <h1 class="page-title">Network</h1>

    <div v-if="loading" class="placeholder">Loading network info…</div>
    <div v-else-if="error" class="card error-card">
      <span class="badge error">Error</span> {{ error }}
    </div>

    <template v-else-if="net">
      <!-- WiFi -->
      <div class="card net-section">
        <h3 class="section-title">📶 WiFi</h3>
        <table class="info-table">
          <tr>
            <td class="label">Enabled</td>
            <td><span class="badge" :class="net.wifi.enabled ? 'ok' : 'info'">{{ net.wifi.enabled ? 'Yes' : 'No' }}</span></td>
          </tr>
          <tr>
            <td class="label">Status</td>
            <td>
              <span class="badge" :class="net.wifi.status === 'got_ip' ? 'ok' : 'error'">
                {{ net.wifi.status === 'got_ip' ? 'Connected' : net.wifi.status }}
              </span>
            </td>
          </tr>
          <tr v-if="net.wifi.status === 'got_ip'">
            <td class="label">IP Address</td>
            <td class="mono">{{ net.wifi.ip }}</td>
          </tr>
          <tr v-if="net.wifi.status === 'got_ip'">
            <td class="label">MAC</td>
            <td class="mono">{{ net.wifi.mac }}</td>
          </tr>
        </table>
        <div class="scan-row">
          <button class="btn" @click="doScan" :disabled="scanning">
            {{ scanning ? 'Scanning…' : '📡 Scan WiFi' }}
          </button>
          <span v-if="scanNote" class="hint">{{ scanNote }}</span>
        </div>
      </div>

      <!-- Ethernet -->
      <div class="card net-section">
        <h3 class="section-title">🔌 Ethernet</h3>
        <table class="info-table">
          <tr>
            <td class="label">Enabled</td>
            <td><span class="badge" :class="net.ethernet.enabled ? 'ok' : 'info'">{{ net.ethernet.enabled ? 'Yes' : 'No' }}</span></td>
          </tr>
          <tr>
            <td class="label">Status</td>
            <td>
              <span class="badge" :class="net.ethernet.status === 'got_ip' ? 'ok' : 'info'">
                {{ net.ethernet.status === 'got_ip' ? 'Connected' : net.ethernet.status }}
              </span>
            </td>
          </tr>
          <tr v-if="net.ethernet.status === 'got_ip'">
            <td class="label">IP Address</td>
            <td class="mono">{{ net.ethernet.ip }}</td>
          </tr>
          <tr v-if="net.ethernet.status === 'got_ip'">
            <td class="label">MAC</td>
            <td class="mono">{{ net.ethernet.mac }}</td>
          </tr>
        </table>
      </div>

      <!-- mDNS -->
      <div class="card net-section">
        <h3 class="section-title">🌐 mDNS</h3>
        <table class="info-table">
          <tr>
            <td class="label">Hostname</td>
            <td class="mono">{{ net.hostname }}.local</td>
          </tr>
          <tr>
            <td class="label">Web UI</td>
            <td>
              <a :href="'http://' + net.hostname + '.local'" target="_blank">
                http://{{ net.hostname }}.local
              </a>
            </td>
          </tr>
        </table>
      </div>

      <!-- Configuration Editor -->
      <div class="card net-section">
        <h3 class="section-title">⚙️ Configuration</h3>
        <div v-if="!editing">
          <p class="hint-text">
            Network settings are stored in <code>/spiffs/network.yaml</code>.
          </p>
          <button class="btn" @click="loadConfig" style="margin-top:0.5rem">
            ✏️ Edit Configuration
          </button>
        </div>
        <div v-else-if="config" class="config-editor">
          <div class="config-row">
            <label>Hostname</label>
            <input type="text" v-model="config.hostname" class="config-input" />
          </div>
          <div class="config-row">
            <label>WiFi Enabled</label>
            <input type="checkbox" v-model="config.wifi_enabled" />
          </div>
          <div class="config-row">
            <label>WiFi DHCP</label>
            <input type="checkbox" v-model="config.wifi_dhcp" />
          </div>
          <div class="config-row">
            <label>Ethernet Enabled</label>
            <input type="checkbox" v-model="config.eth_enabled" />
          </div>
          <div class="config-row">
            <label>Failover</label>
            <input type="checkbox" v-model="config.failover_enabled" />
          </div>
          <div class="config-row">
            <label>mDNS</label>
            <input type="checkbox" v-model="config.mdns_enabled" />
          </div>
          <div class="config-actions">
            <button class="btn primary" @click="saveConfig">💾 Save</button>
            <button class="btn" @click="editing = false">Cancel</button>
            <span v-if="saveMsg" class="save-msg">{{ saveMsg }}</span>
          </div>
        </div>
      </div>
    </template>
  </div>
</template>

<style scoped>
.network-page {
  max-width: 600px;
  display: flex;
  flex-direction: column;
  gap: 1rem;
}
.page-title {
  font-size: 1.3rem;
  font-weight: 600;
}
.section-title {
  font-size: 0.8rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-muted);
  margin-bottom: 0.5rem;
}
.net-section {
  display: flex;
  flex-direction: column;
}
.info-table {
  width: 100%;
  border-collapse: collapse;
}
.info-table td {
  padding: 0.35rem 0.5rem;
  border-bottom: 1px solid var(--border);
  font-size: 0.85rem;
}
.info-table .label {
  color: var(--text-muted);
  width: 100px;
}
.mono {
  font-family: 'JetBrains Mono', monospace;
}
.error-card {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}
.placeholder {
  color: var(--text-muted);
  font-size: 0.85rem;
}
.hint-text {
  font-size: 0.85rem;
  color: var(--text-secondary);
  line-height: 1.5;
}
.hint-text code {
  background: var(--bg-primary);
  padding: 0.1rem 0.4rem;
  border-radius: 3px;
  font-size: 0.82rem;
}
.hint {
  font-size: 0.75rem;
  color: var(--text-muted);
}
.scan-row {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  margin-top: 0.75rem;
}
.config-editor {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}
.config-row {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  font-size: 0.85rem;
}
.config-row label {
  min-width: 120px;
  color: var(--text-secondary);
}
.config-input {
  background: var(--bg-primary);
  color: var(--text-primary);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm, 4px);
  padding: 0.3rem 0.5rem;
  font-family: 'JetBrains Mono', monospace;
  font-size: 0.85rem;
}
.config-actions {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  margin-top: 0.5rem;
}
.save-msg {
  font-size: 0.85rem;
  color: var(--accent);
}
</style>
