<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { api } from '../services/api'
import type { OtaStatus } from '../services/api'

const status = ref<OtaStatus | null>(null)
const file = ref<File | null>(null)
const uploading = ref(false)
const progress = ref(0)
const message = ref('')
const error = ref('')

onMounted(loadStatus)

async function loadStatus() {
  try {
    status.value = await api.getOtaStatus()
  } catch (e: unknown) {
    error.value = 'Failed to load firmware info'
    console.error(e)
  }
}

function onFileSelect(e: Event) {
  const input = e.target as HTMLInputElement
  file.value = input.files?.[0] ?? null
  message.value = ''
  error.value = ''
}

async function doUpload() {
  if (!file.value) return
  uploading.value = true
  progress.value = 0
  message.value = ''
  error.value = ''
  try {
    await api.uploadFirmware(file.value, (pct) => { progress.value = pct })
    message.value = 'Upload complete — device is rebooting…'
  } catch (e: unknown) {
    error.value = `Upload failed: ${e instanceof Error ? e.message : e}`
  } finally {
    uploading.value = false
  }
}

async function doRollback() {
  if (!confirm('Roll back to previous firmware? Device will reboot.')) return
  try {
    await api.rollbackFirmware()
    message.value = 'Rollback initiated — device is rebooting…'
  } catch (e: unknown) {
    error.value = `Rollback failed: ${e instanceof Error ? e.message : e}`
  }
}
</script>

<template>
  <div class="page">
    <h2>Firmware Update</h2>

    <div v-if="status" class="info-card">
      <div class="info-row"><span class="label">Version</span><span>{{ status.version }}</span></div>
      <div class="info-row"><span class="label">IDF Version</span><span>{{ status.idf_version }}</span></div>
      <div class="info-row"><span class="label">Compile Date</span><span>{{ status.compile_date }} {{ status.compile_time }}</span></div>
      <div class="info-row"><span class="label">Running Partition</span><span>{{ status.running_partition }}</span></div>
      <div class="info-row"><span class="label">Next Update</span><span>{{ status.next_update_partition }}</span></div>
    </div>

    <div class="upload-section">
      <h3>Upload New Firmware</h3>
      <input type="file" accept=".bin" @change="onFileSelect" :disabled="uploading" />

      <div v-if="file" class="file-info">
        Selected: {{ file.name }} ({{ (file.size / 1024).toFixed(0) }} KB)
      </div>

      <div v-if="uploading" class="progress-bar">
        <div class="progress-fill" :style="{ width: progress + '%' }"></div>
        <span class="progress-text">{{ progress }}%</span>
      </div>

      <button @click="doUpload" :disabled="!file || uploading" class="btn-primary">
        {{ uploading ? 'Uploading…' : 'Upload & Flash' }}
      </button>
    </div>

    <div class="rollback-section">
      <h3>Rollback</h3>
      <p>Revert to the previously-flashed firmware partition.</p>
      <button @click="doRollback" class="btn-danger">Rollback Firmware</button>
    </div>

    <div v-if="message" class="success-msg">{{ message }}</div>
    <div v-if="error" class="error-msg">{{ error }}</div>
  </div>
</template>

<style scoped>
.page { padding: 1rem; color: #eee; }
h2 { margin-bottom: 1rem; }
h3 { margin: 1.2rem 0 0.6rem; }

.info-card {
  background: #1a1a2e;
  border-radius: 8px;
  padding: 0.8rem 1rem;
  margin-bottom: 1rem;
}
.info-row {
  display: flex;
  justify-content: space-between;
  padding: 0.25rem 0;
  border-bottom: 1px solid #2a2a3e;
}
.info-row:last-child { border-bottom: none; }
.label { color: #888; }

.upload-section, .rollback-section {
  background: #1a1a2e;
  border-radius: 8px;
  padding: 1rem;
  margin-bottom: 1rem;
}

.file-info { margin: 0.5rem 0; color: #aaa; font-size: 0.9em; }

.progress-bar {
  position: relative;
  height: 24px;
  background: #333;
  border-radius: 4px;
  margin: 0.5rem 0;
  overflow: hidden;
}
.progress-fill {
  height: 100%;
  background: linear-gradient(90deg, #3a7bd5, #00d2ff);
  transition: width 0.2s;
}
.progress-text {
  position: absolute;
  top: 0; left: 0; right: 0;
  text-align: center;
  line-height: 24px;
  font-size: 0.85em;
  color: #fff;
}

.btn-primary {
  background: #3a7bd5;
  color: #fff;
  border: none;
  border-radius: 6px;
  padding: 0.6rem 1.4rem;
  cursor: pointer;
  margin-top: 0.5rem;
}
.btn-primary:disabled { opacity: 0.4; cursor: not-allowed; }

.btn-danger {
  background: #c0392b;
  color: #fff;
  border: none;
  border-radius: 6px;
  padding: 0.6rem 1.4rem;
  cursor: pointer;
}

.success-msg {
  margin-top: 1rem;
  padding: 0.6rem 1rem;
  background: #1a3a1a;
  border-left: 3px solid #27ae60;
  border-radius: 4px;
}
.error-msg {
  margin-top: 1rem;
  padding: 0.6rem 1rem;
  background: #3a1a1a;
  border-left: 3px solid #e74c3c;
  border-radius: 4px;
}
</style>
