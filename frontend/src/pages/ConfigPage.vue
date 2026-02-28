<script setup lang="ts">
import MachineSelector from '../components/MachineSelector.vue'
import type { MachineProfile } from '../services/api'

function onMachineSelect(m: MachineProfile) {
  // TODO: POST /api/config/machine to load this profile
  console.log('Selected machine:', m.model)
}
</script>

<template>
  <div class="config-page">
    <h1 class="page-title">Configuration</h1>

    <MachineSelector @select="onMachineSelect" />

    <div class="card config-section">
      <h3 class="section-title">Display</h3>
      <div class="config-row">
        <label>Resolution</label>
        <select class="config-select" disabled>
          <option>Low (320×200, 16 colors)</option>
          <option>Medium (640×200, 4 colors)</option>
          <option>High (640×400, monochrome)</option>
        </select>
      </div>
      <div class="config-row">
        <label>CRT Effects</label>
        <label class="toggle">
          <input type="checkbox" disabled />
          <span class="toggle-slider"></span>
          Scanlines + curvature
        </label>
      </div>
    </div>

    <div class="card config-section">
      <h3 class="section-title">Audio</h3>
      <div class="config-row">
        <label>Sample Rate</label>
        <select class="config-select" disabled>
          <option>44100 Hz</option>
          <option>48000 Hz</option>
        </select>
      </div>
      <div class="config-row">
        <label>Volume</label>
        <input type="range" min="0" max="100" value="80" disabled class="config-range" />
      </div>
    </div>

    <div class="card config-section">
      <h3 class="section-title">Memory</h3>
      <div class="config-row">
        <label>RAM Size</label>
        <select class="config-select" disabled>
          <option>512 KB</option>
          <option selected>1024 KB (1 MB)</option>
          <option>2048 KB (2 MB)</option>
          <option>4096 KB (4 MB)</option>
        </select>
      </div>
    </div>

    <div class="card config-section">
      <h3 class="section-title">Firmware</h3>
      <div class="config-row">
        <label>Current Version</label>
        <span class="config-value">0.1.0</span>
      </div>
      <div class="config-row">
        <button class="btn" disabled>📦 OTA Update</button>
        <span class="hint">Upload firmware via browser (Phase 5)</span>
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
