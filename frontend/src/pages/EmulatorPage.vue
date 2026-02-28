<script setup lang="ts">
import DisplayCanvas from '../components/DisplayCanvas.vue'

// TODO: Wire up to /api/system for emulator state control
// TODO: Connect to /ws/stream for video + /ws/input for keyboard/mouse
</script>

<template>
  <div class="emulator-page">
    <div class="emu-header">
      <h1 class="page-title">Emulator</h1>
      <div class="emu-controls">
        <button class="btn primary" disabled title="Phase 3: Start emulation">
          ▶ Start
        </button>
        <button class="btn" disabled>⏸ Pause</button>
        <button class="btn" disabled>🔄 Reset</button>
        <button class="btn danger" disabled>⏹ Stop</button>
      </div>
    </div>

    <DisplayCanvas />

    <div class="emu-info card">
      <h3 class="section-title">Emulator Status</h3>
      <table class="info-table">
        <tr>
          <td class="label">State</td>
          <td><span class="badge info">Idle</span></td>
        </tr>
        <tr>
          <td class="label">Machine</td>
          <td>—</td>
        </tr>
        <tr>
          <td class="label">Video</td>
          <td>Waiting for Phase 3 (WebSocket streaming)</td>
        </tr>
        <tr>
          <td class="label">Audio</td>
          <td>Waiting for Phase 3 (Web Audio API)</td>
        </tr>
        <tr>
          <td class="label">Input</td>
          <td>Waiting for Phase 6 (Keyboard/Mouse/Joystick)</td>
        </tr>
      </table>
    </div>
  </div>
</template>

<style scoped>
.emulator-page {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  max-width: 720px;
}
.emu-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: 0.5rem;
}
.page-title {
  font-size: 1.3rem;
  font-weight: 600;
}
.emu-controls {
  display: flex;
  gap: 0.4rem;
}
.section-title {
  font-size: 0.8rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-muted);
  margin-bottom: 0.5rem;
}
.info-table {
  width: 100%;
  border-collapse: collapse;
}
.info-table td {
  padding: 0.3rem 0.5rem;
  border-bottom: 1px solid var(--border);
  font-size: 0.85rem;
}
.info-table .label {
  color: var(--text-muted);
  width: 100px;
}
</style>
