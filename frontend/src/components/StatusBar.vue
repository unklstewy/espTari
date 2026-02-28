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

function formatUptime(ms: number): string {
  const s = Math.floor(ms / 1000)
  const m = Math.floor(s / 60)
  const h = Math.floor(m / 60)
  if (h > 0) return `${h}h ${m % 60}m`
  if (m > 0) return `${m}m ${s % 60}s`
  return `${s}s`
}

function formatBytes(bytes: number): string {
  if (bytes >= 1048576) return `${(bytes / 1048576).toFixed(1)} MB`
  return `${(bytes / 1024).toFixed(0)} KB`
}

onMounted(() => {
  poll()
  timer = setInterval(poll, 3000)
})
onUnmounted(() => clearInterval(timer))
</script>

<template>
  <footer class="status-bar">
    <template v-if="status && !error">
      <div class="status-item">
        <span class="dot online"></span>
        <span>{{ status.state }}</span>
      </div>
      <div class="status-item">
        Heap: <strong>{{ formatBytes(status.free_heap) }}</strong>
      </div>
      <div class="status-item">
        PSRAM: <strong>{{ formatBytes(status.free_psram) }}</strong>
      </div>
      <div class="status-item">
        Up: <strong>{{ formatUptime(status.uptime_ms) }}</strong>
      </div>
    </template>
    <template v-else-if="error">
      <div class="status-item">
        <span class="dot offline"></span>
        <span>Disconnected</span>
      </div>
    </template>
    <template v-else>
      <div class="status-item">Connecting…</div>
    </template>
  </footer>
</template>

<style scoped>
.status-bar {
  display: flex;
  align-items: center;
  gap: 1.5rem;
  height: 28px;
  padding: 0 1rem;
  background: var(--bg-secondary);
  border-top: 1px solid var(--border);
  font-size: 0.75rem;
  color: var(--text-muted);
  flex-shrink: 0;
}
.status-item {
  display: flex;
  align-items: center;
  gap: 0.35rem;
}
.status-item strong {
  color: var(--text-secondary);
}
.dot {
  width: 7px;
  height: 7px;
  border-radius: 50%;
}
.dot.online {
  background: var(--accent);
  box-shadow: 0 0 6px var(--accent);
}
.dot.offline {
  background: var(--error);
  box-shadow: 0 0 6px var(--error);
}
</style>
