<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import DisplayCanvas from '../components/DisplayCanvas.vue'
import { StreamClient, type StreamStats } from '../services/StreamClient'
import { AudioPlayer } from '../services/AudioPlayer'

/* ── State ─────────────────────────────────────────────── */
const jpegFrame = ref<Uint8Array | null>(null)
const frameWidth = ref(0)
const frameHeight = ref(0)
const connected = ref(false)
const stats = ref<StreamStats>({
  connected: false, framesReceived: 0,
  audioChunksReceived: 0, bytesReceived: 0, fps: 0,
})
const audioMuted = ref(false)
const audioStarted = ref(false)

/* ── Stream + Audio instances ──────────────────────────── */
const stream = new StreamClient()
const audio = new AudioPlayer()

stream.onVideoFrame = (jpeg, meta) => {
  jpegFrame.value = jpeg
  frameWidth.value = meta.width
  frameHeight.value = meta.height
  /* Auto-start audio on first frame (page navigation counts as user gesture) */
  if (!audioStarted.value) {
    audio.start(44100, 2)
    audio.muted = audioMuted.value
    audio.resume()
    audioStarted.value = true
  }
}

stream.onAudioChunk = (pcm, meta) => {
  audio.feed(pcm, meta.channels)
}

stream.onConnected = () => { connected.value = true }
stream.onDisconnected = () => { connected.value = false }

/* Stats poll */
let statsTimer: ReturnType<typeof setInterval> | null = null

function toggleAudio() {
  if (!audioStarted.value) {
    audio.start(44100, 2)
    audioStarted.value = true
    audioMuted.value = false
  } else {
    audioMuted.value = !audioMuted.value
  }
  audio.muted = audioMuted.value
  audio.resume()
}

onMounted(() => {
  stream.connect()
  statsTimer = setInterval(() => {
    stats.value = stream.stats
  }, 1000)
})

onUnmounted(() => {
  stream.disconnect()
  audio.stop()
  if (statsTimer) clearInterval(statsTimer)
})
</script>

<template>
  <div class="emulator-page">
    <div class="emu-header">
      <h1 class="page-title">Emulator</h1>
      <div class="emu-controls">
        <button class="btn primary" disabled title="Phase 4: Start emulation">
          ▶ Start
        </button>
        <button class="btn" disabled>⏸ Pause</button>
        <button class="btn" disabled>🔄 Reset</button>
        <button class="btn danger" disabled>⏹ Stop</button>
        <button
          class="btn"
          :class="{ active: !audioMuted && audioStarted }"
          @click="toggleAudio"
          :title="audioMuted ? 'Unmute audio' : 'Mute audio'"
        >
          {{ audioMuted || !audioStarted ? '🔇' : '🔊' }} Audio
        </button>
      </div>
    </div>

    <DisplayCanvas
      :jpegFrame="jpegFrame"
      :frameWidth="frameWidth"
      :frameHeight="frameHeight"
      :connected="connected"
    />

    <div class="emu-info card">
      <h3 class="section-title">Stream Status</h3>
      <table class="info-table">
        <tr>
          <td class="label">WebSocket</td>
          <td>
            <span class="badge" :class="connected ? 'ok' : 'warn'">
              {{ connected ? 'Connected' : 'Disconnected' }}
            </span>
          </td>
        </tr>
        <tr>
          <td class="label">Video FPS</td>
          <td>{{ stats.fps }}</td>
        </tr>
        <tr>
          <td class="label">Frames</td>
          <td>{{ stats.framesReceived.toLocaleString() }}</td>
        </tr>
        <tr>
          <td class="label">Audio Chunks</td>
          <td>{{ stats.audioChunksReceived.toLocaleString() }}</td>
        </tr>
        <tr>
          <td class="label">Data Received</td>
          <td>{{ (stats.bytesReceived / 1024).toFixed(1) }} KB</td>
        </tr>
        <tr>
          <td class="label">Audio</td>
          <td>{{ audioStarted ? (audioMuted ? 'Muted' : 'Playing') : 'Click 🔇 to start' }}</td>
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
