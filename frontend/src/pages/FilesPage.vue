<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { api, type RomInfo, type DiskInfo } from '../services/api'

const roms = ref<RomInfo[]>([])
const disks = ref<DiskInfo[]>([])
const loading = ref(true)
const error = ref('')
const mountedA = ref('')
const mountedB = ref('')
const mountMsg = ref('')

async function load() {
  try {
    const [r, d] = await Promise.all([api.getRoms(), api.getDisks()])
    roms.value = r
    disks.value = d
  } catch (e: unknown) {
    error.value = (e as Error).message || 'Failed to load'
  } finally {
    loading.value = false
  }
}

function fmtSize(bytes: number): string {
  if (bytes >= 1048576) return `${(bytes / 1048576).toFixed(1)} MB`
  return `${(bytes / 1024).toFixed(0)} KB`
}

async function mountDisk(name: string, drive: number) {
  try {
    const path = `/sdcard/disks/floppy/${name}`
    const res = await api.mountDisk({ drive, path })
    if (drive === 0) mountedA.value = res.path
    else mountedB.value = res.path
    mountMsg.value = `Drive ${drive === 0 ? 'A' : 'B'}: ${name}`
    setTimeout(() => mountMsg.value = '', 3000)
  } catch (e: unknown) {
    mountMsg.value = `Error: ${(e as Error).message}`
  }
}

async function ejectDisk(drive: number) {
  try {
    await api.mountDisk({ drive, path: '' })
    if (drive === 0) mountedA.value = ''
    else mountedB.value = ''
    mountMsg.value = `Drive ${drive === 0 ? 'A' : 'B'}: ejected`
    setTimeout(() => mountMsg.value = '', 3000)
  } catch (e: unknown) {
    mountMsg.value = `Error: ${(e as Error).message}`
  }
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
      <div class="drive-status">
        <span class="drive-label">
          A: <strong>{{ mountedA || '(empty)' }}</strong>
          <button v-if="mountedA" class="btn-sm" @click="ejectDisk(0)">⏏</button>
        </span>
        <span class="drive-label">
          B: <strong>{{ mountedB || '(empty)' }}</strong>
          <button v-if="mountedB" class="btn-sm" @click="ejectDisk(1)">⏏</button>
        </span>
        <span v-if="mountMsg" class="mount-msg">{{ mountMsg }}</span>
      </div>
      <div v-if="loading" class="placeholder">Loading…</div>
      <div v-else-if="disks.length === 0" class="placeholder">
        No disk images found.<br>
        <span class="hint">Place .st / .msa / .stx files in /sdcard/disks/floppy/</span>
      </div>
      <table v-else class="file-table">
        <thead>
          <tr>
            <th>Name</th>
            <th>Size</th>
            <th>Type</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="disk in disks" :key="disk.name">
            <td class="fname">{{ disk.name }}</td>
            <td>{{ fmtSize(disk.size) }}</td>
            <td><span class="badge info">{{ disk.type }}</span></td>
            <td class="disk-actions">
              <button class="btn-sm" @click="mountDisk(disk.name, 0)" title="Insert in drive A:">A:</button>
              <button class="btn-sm" @click="mountDisk(disk.name, 1)" title="Insert in drive B:">B:</button>
            </td>
          </tr>
        </tbody>
      </table>
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
.drive-status {
  display: flex;
  gap: 1rem;
  flex-wrap: wrap;
  align-items: center;
  margin-bottom: 0.5rem;
  font-size: 0.85rem;
}
.drive-label {
  color: var(--text-secondary);
}
.mount-msg {
  font-size: 0.8rem;
  color: var(--accent);
}
.disk-actions {
  display: flex;
  gap: 0.3rem;
}
.btn-sm {
  background: var(--bg-hover);
  color: var(--text-primary);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm, 4px);
  padding: 0.15rem 0.4rem;
  font-size: 0.75rem;
  cursor: pointer;
}
.btn-sm:hover {
  border-color: var(--accent);
}
</style>
