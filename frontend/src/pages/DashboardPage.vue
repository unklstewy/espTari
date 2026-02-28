<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import { api, type EmulatorState } from '../services/api'

const status = ref<EmulatorState | null>(null)
const error = ref(false)
let timer: ReturnType<typeof setInterval>

async function poll() {
  try {
    status.value = await api.getSystem()
    error.value = false
  } catch {
    error.value = true
  }
}

function fmtBytes(b: number): string {
  if (b >= 1048576) return `${(b / 1048576).toFixed(1)} MB`
  return `${(b / 1024).toFixed(0)} KB`
}

function fmtUptime(ms: number): string {
  const s = Math.floor(ms / 1000)
  const m = Math.floor(s / 60)
  const h = Math.floor(m / 60)
  const d = Math.floor(h / 24)
  if (d > 0) return `${d}d ${h % 24}h ${m % 60}m`
  if (h > 0) return `${h}h ${m % 60}m ${s % 60}s`
  if (m > 0) return `${m}m ${s % 60}s`
  return `${s}s`
}

function heapPercent(s: EmulatorState): number {
  // Rough estimate: ESP32-P4 has ~450KB internal SRAM
  const total = 450 * 1024
  return Math.round((s.free_heap / total) * 100)
}

function psramPercent(s: EmulatorState): number {
  const total = 32 * 1024 * 1024 // 32MB PSRAM
  return Math.round((s.free_psram / total) * 100)
}

onMounted(() => { poll(); timer = setInterval(poll, 3000) })
onUnmounted(() => clearInterval(timer))
</script>

<template>
  <div class="dashboard">
    <h1 class="page-title">Dashboard</h1>

    <div v-if="error" class="card error-card">
      <span class="badge error">Offline</span>
      <span>Cannot reach espTari device</span>
    </div>

    <div v-else-if="status" class="stats-grid">
      <!-- Status -->
      <div class="card stat-card">
        <div class="stat-label">Status</div>
        <div class="stat-value">
          <span class="badge ok">Online</span>
        </div>
        <div class="stat-detail">{{ status.state }}</div>
      </div>

      <!-- Uptime -->
      <div class="card stat-card">
        <div class="stat-label">Uptime</div>
        <div class="stat-value">{{ fmtUptime(status.uptime_ms) }}</div>
      </div>

      <!-- Heap -->
      <div class="card stat-card">
        <div class="stat-label">Internal Heap</div>
        <div class="stat-value">{{ fmtBytes(status.free_heap) }}</div>
        <div class="progress-bar">
          <div class="progress-fill" :style="{ width: heapPercent(status) + '%' }"></div>
        </div>
        <div class="stat-detail">{{ heapPercent(status) }}% free</div>
      </div>

      <!-- PSRAM -->
      <div class="card stat-card">
        <div class="stat-label">PSRAM</div>
        <div class="stat-value">{{ fmtBytes(status.free_psram) }}</div>
        <div class="progress-bar">
          <div class="progress-fill psram" :style="{ width: psramPercent(status) + '%' }"></div>
        </div>
        <div class="stat-detail">{{ psramPercent(status) }}% free of 32 MB</div>
      </div>
    </div>

    <div v-else class="loading-state">
      Connecting to espTari…
    </div>

    <!-- Quick actions -->
    <div class="card quick-actions" v-if="status">
      <h3 class="section-title">Quick Actions</h3>
      <div class="action-row">
        <router-link to="/emulator" class="btn primary">🕹️ Open Emulator</router-link>
        <router-link to="/config" class="btn">⚙️ Configuration</router-link>
        <router-link to="/files" class="btn">📁 File Manager</router-link>
        <router-link to="/network" class="btn">📡 Network</router-link>
      </div>
    </div>
  </div>
</template>

<style scoped>
.dashboard {
  max-width: 800px;
}
.page-title {
  font-size: 1.3rem;
  font-weight: 600;
  margin-bottom: 1.25rem;
  color: var(--text-primary);
}
.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
  gap: 0.75rem;
  margin-bottom: 1.25rem;
}
.stat-card {
  display: flex;
  flex-direction: column;
  gap: 0.3rem;
}
.stat-label {
  font-size: 0.75rem;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: var(--text-muted);
}
.stat-value {
  font-size: 1.15rem;
  font-weight: 600;
  color: var(--text-primary);
}
.stat-detail {
  font-size: 0.72rem;
  color: var(--text-muted);
}
.progress-bar {
  height: 4px;
  background: var(--bg-primary);
  border-radius: 2px;
  overflow: hidden;
}
.progress-fill {
  height: 100%;
  background: var(--accent);
  border-radius: 2px;
  transition: width 500ms ease;
}
.progress-fill.psram {
  background: var(--info);
}
.error-card {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  color: var(--error);
}
.loading-state {
  color: var(--text-muted);
  padding: 2rem 0;
}
.section-title {
  font-size: 0.8rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-muted);
  margin-bottom: 0.75rem;
}
.quick-actions {
  margin-top: 1rem;
}
.action-row {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
}
.action-row .btn {
  text-decoration: none;
}
</style>
