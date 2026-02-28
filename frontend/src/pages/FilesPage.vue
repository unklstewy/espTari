<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { api, type RomInfo } from '../services/api'

const roms = ref<RomInfo[]>([])
const loading = ref(true)
const error = ref('')

async function load() {
  try {
    roms.value = await api.getRoms()
  } catch (e: any) {
    error.value = e.message || 'Failed to load'
  } finally {
    loading.value = false
  }
}

function fmtSize(bytes: number): string {
  if (bytes >= 1048576) return `${(bytes / 1048576).toFixed(1)} MB`
  return `${(bytes / 1024).toFixed(0)} KB`
}

onMounted(load)
</script>

<template>
  <div class="files-page">
    <h1 class="page-title">File Manager</h1>

    <!-- ROMs -->
    <div class="card file-section">
      <h3 class="section-title">TOS ROMs</h3>
      <div v-if="loading" class="placeholder">Loading…</div>
      <div v-else-if="error" class="placeholder error">{{ error }}</div>
      <div v-else-if="roms.length === 0" class="placeholder">
        No ROM files found.<br>
        <span class="hint">Place .img files in /sdcard/roms/tos/</span>
      </div>
      <table v-else class="file-table">
        <thead>
          <tr>
            <th>Name</th>
            <th>Size</th>
            <th>Category</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="rom in roms" :key="rom.name">
            <td class="fname">{{ rom.name }}</td>
            <td>{{ fmtSize(rom.size) }}</td>
            <td><span class="badge info">{{ rom.category }}</span></td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- Disk Images -->
    <div class="card file-section">
      <h3 class="section-title">Disk Images</h3>
      <div class="placeholder">
        Disk management coming in Phase 5.<br>
        <span class="hint">Place .st / .msa / .stx files in /sdcard/disks/floppy/</span>
      </div>
    </div>

    <!-- SD Card Info -->
    <div class="card file-section">
      <h3 class="section-title">SD Card</h3>
      <table class="info-table">
        <tr>
          <td class="label">Mount</td>
          <td>/sdcard</td>
        </tr>
        <tr>
          <td class="label">Web Root</td>
          <td>/sdcard/www/</td>
        </tr>
        <tr>
          <td class="label">Upload</td>
          <td>
            <button class="btn" disabled>📤 Upload File</button>
            <span class="hint">Coming in Phase 5</span>
          </td>
        </tr>
      </table>
    </div>
  </div>
</template>

<style scoped>
.files-page {
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
.file-section {
  display: flex;
  flex-direction: column;
}
.file-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.85rem;
}
.file-table th {
  text-align: left;
  font-size: 0.75rem;
  text-transform: uppercase;
  color: var(--text-muted);
  letter-spacing: 0.05em;
  padding: 0.3rem 0.5rem;
  border-bottom: 1px solid var(--border);
}
.file-table td {
  padding: 0.4rem 0.5rem;
  border-bottom: 1px solid var(--border);
}
.fname {
  font-weight: 500;
}
.placeholder {
  color: var(--text-muted);
  font-size: 0.85rem;
  padding: 0.5rem 0;
}
.placeholder.error { color: var(--error); }
.hint {
  font-size: 0.75rem;
  color: var(--text-muted);
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
</style>
