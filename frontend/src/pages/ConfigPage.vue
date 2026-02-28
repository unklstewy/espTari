<script setup lang="ts">
import { ref, onMounted } from 'vue'
import MachineSelector from '../components/MachineSelector.vue'
import { api, type MachineProfile, type EmulatorConfig } from '../services/api'

const config = ref<EmulatorConfig>({
  machine: 'st',
  display: { resolution: 'low', crt_effects: false },
  audio: { sample_rate: 44100, volume: 80 },
  memory: { ram_kb: 1024 },
})
const saving = ref(false)
const saveMsg = ref('')
const loadError = ref('')

async function loadConfig() {
  try {
    config.value = await api.getConfig()
    loadError.value = ''
  } catch (e: unknown) {
    loadError.value = (e as Error).message || 'Failed to load config'
  }
}

async function onMachineSelect(m: MachineProfile) {
  try {
    await api.setActiveMachine(m.model)
    config.value.machine = m.model
    saveMsg.value = `Machine set to ${m.name}`
    setTimeout(() => saveMsg.value = '', 3000)
  } catch (e: unknown) {
    saveMsg.value = `Error: ${(e as Error).message}`
  }
}

async function saveConfig() {
  saving.value = true
  saveMsg.value = ''
  try {
    await api.setConfig(config.value)
    saveMsg.value = 'Configuration saved'
    setTimeout(() => saveMsg.value = '', 3000)
  } catch (e: unknown) {
    saveMsg.value = `Error: ${(e as Error).message}`
  } finally {
    saving.value = false
  }
}

onMounted(loadConfig)
</script>

<template>
  <div class="config-page">
    <h1 class="page-title">Configuration</h1>

    <div v-if="loadError" class="card error-card">
      <span class="badge error">Error</span> {{ loadError }}
    </div>

    <MachineSelector @select="onMachineSelect" />

    <div class="card config-section">
      <h3 class="section-title">Display</h3>
      <div class="config-row">
        <label>Resolution</label>
        <select class="config-select" v-model="config.display.resolution">
          <option value="low">Low (320×200, 16 colors)</option>
          <option value="medium">Medium (640×200, 4 colors)</option>
          <option value="high">High (640×400, monochrome)</option>
        </select>
      </div>
      <div class="config-row">
        <label>CRT Effects</label>
        <label class="toggle">
          <input type="checkbox" v-model="config.display.crt_effects" />
          <span class="toggle-slider"></span>
          Scanlines + curvature
        </label>
      </div>
    </div>

    <div class="card config-section">
      <h3 class="section-title">Audio</h3>
      <div class="config-row">
        <label>Sample Rate</label>
        <select class="config-select" v-model.number="config.audio.sample_rate">
          <option :value="44100">44100 Hz</option>
          <option :value="48000">48000 Hz</option>
        </select>
      </div>
      <div class="config-row">
        <label>Volume</label>
        <input type="range" min="0" max="100" v-model.number="config.audio.volume" class="config-range" />
        <span class="config-value">{{ config.audio.volume }}%</span>
      </div>
    </div>

    <div class="card config-section">
      <h3 class="section-title">Memory</h3>
      <div class="config-row">
        <label>RAM Size</label>
        <select class="config-select" v-model.number="config.memory.ram_kb">
          <option :value="512">512 KB</option>
          <option :value="1024">1024 KB (1 MB)</option>
          <option :value="2048">2048 KB (2 MB)</option>
          <option :value="4096">4096 KB (4 MB)</option>
        </select>
      </div>
    </div>

    <div class="config-actions">
      <button class="btn primary" @click="saveConfig" :disabled="saving">
        {{ saving ? 'Saving…' : '💾 Save Configuration' }}
      </button>
      <span v-if="saveMsg" class="save-msg" :class="{ error: saveMsg.startsWith('Error') }">{{ saveMsg }}</span>
    </div>

    <div class="card config-section">
      <h3 class="section-title">Firmware</h3>
      <div class="config-row">
        <label>Current Version</label>
        <span class="config-value">0.1.0</span>
      </div>
      <div class="config-row">
        <router-link to="/update" class="btn">📦 OTA Update</router-link>
      </div>
    </div>
  </div>
</template>

<style scoped>
.config-page {
  max-width: 700px;
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
  margin-bottom: 0.75rem;
}
.config-section {
  display: flex;
  flex-direction: column;
  gap: 0.6rem;
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
.config-select {
  background: var(--bg-primary);
  color: var(--text-primary);
  border: 1px solid var(--border-light);
  border-radius: var(--radius-sm);
  padding: 0.3rem 0.5rem;
  font-family: inherit;
  font-size: 0.85rem;
}
.config-range {
  flex: 1;
  max-width: 200px;
  accent-color: var(--accent);
}
.config-value {
  color: var(--text-primary);
  font-weight: 500;
}
.config-actions {
  display: flex;
  align-items: center;
  gap: 1rem;
}
.save-msg {
  font-size: 0.85rem;
  color: var(--accent);
}
.save-msg.error {
  color: var(--error, #e53e3e);
}
.error-card {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}
.hint {
  font-size: 0.75rem;
  color: var(--text-muted);
}
.toggle {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  cursor: pointer;
}
</style>
