<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import DisplayCanvas from '../components/DisplayCanvas.vue'
import { StreamClient, type StreamStats } from '../services/StreamClient'
import { AudioPlayer } from '../services/AudioPlayer'
import { InputService } from '../services/InputService'
import { api } from '../services/api'

/* ── State ─────────────────────────────────────────────── */
const jpegFrame = ref<Uint8Array | null>(null)
const frameWidth = ref(0)
const frameHeight = ref(0)
const connected = ref(false)
const emulatorState = ref<string>('stopped')
const stats = ref<StreamStats>({
  connected: false, framesReceived: 0,
  audioChunksReceived: 0, bytesReceived: 0, fps: 0,
})
const audioMuted = ref(false)
const audioStarted = ref(false)
const inputActive = ref(false)
const crtEnabled = ref(false)
const displayArea = ref<HTMLElement | null>(null)

/* ── Stream + Audio + Input instances ──────────────────── */
const stream = new StreamClient()
const audio = new AudioPlayer()
const input = new InputService()

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

/* Stats + state poll */
let pollTimer: ReturnType<typeof setInterval> | null = null

async function pollState() {
  stats.value = stream.stats
  try {
    const sys = await api.getSystem()
    emulatorState.value = sys.state
  } catch { /* ignore */ }
}

/* ── Emulator controls ─────────────────────────────────── */
async function doAction(action: string) {
  try {
    const sys = await api.controlSystem(action)
    emulatorState.value = sys.state
  } catch (e: unknown) {
    console.error('Control error:', e)
  }
}

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

function toggleInput() {
  if (inputActive.value) {
    input.disable()
    inputActive.value = false
  } else {
    input.connect()
    input.enable(displayArea.value ?? undefined)
    inputActive.value = true
  }
}

function toggleCrt() {
  crtEnabled.value = !crtEnabled.value
}

onMounted(() => {
  stream.connect()
  pollTimer = setInterval(pollState, 1000)
})

onUnmounted(() => {
  stream.disconnect()
  audio.stop()
  input.disconnect()
  if (pollTimer) clearInterval(pollTimer)
})
</script>

<template>
  <div class="emulator-page">
    <div class="emu-header">
      <h1 class="page-title">Emulator</h1>
      <div class="emu-controls">
        <button
          class="btn primary"
          :disabled="emulatorState === 'running'"
          @click="doAction(emulatorState === 'paused' ? 'resume' : 'start')"
        >
          ▶ {{ emulatorState === 'paused' ? 'Resume' : 'Start' }}
        </button>
        <button
          class="btn"
          :disabled="emulatorState !== 'running'"
          @click="doAction('pause')"
        >⏸ Pause</button>
        <button
          class="btn"
          :disabled="emulatorState === 'stopped'"
          @click="doAction('reset')"
        >🔄 Reset</button>
        <button
          class="btn danger"
          :disabled="emulatorState === 'stopped'"
          @click="doAction('stop')"
        >⏹ Stop</button>
        <button
          class="btn"
          :class="{ active: !audioMuted && audioStarted }"
          @click="toggleAudio"
          :title="audioMuted ? 'Unmute audio' : 'Mute audio'"
        >
          {{ audioMuted || !audioStarted ? '🔇' : '🔊' }} Audio
        </button>
        <button
          class="btn"
          :class="{ active: inputActive }"
          @click="toggleInput"
          :title="inputActive ? 'Release keyboard/mouse' : 'Capture keyboard/mouse'"
        >
          {{ inputActive ? '🎮' : '⌨️' }} Input
        </button>
        <button
          class="btn"
          :class="{ active: crtEnabled }"
          @click="toggleCrt"
          title="Toggle CRT effect"
        >
          📺 CRT
        </button>
      </div>
    </div>

    <div class="state-badge">
      <span class="badge" :class="emulatorState === 'running' ? 'ok' : emulatorState === 'paused' ? 'warn' : 'info'">
        {{ emulatorState.toUpperCase() }}
      </span>
    </div>

    <div ref="displayArea" class="display-wrapper" :class="{ 'crt-effect': crtEnabled }">
      <DisplayCanvas
        :jpegFrame="jpegFrame"
        :frameWidth="frameWidth"
        :frameHeight="frameHeight"
        :connected="connected"
      />
      <div v-if="inputActive && !input.mouseCaptured" class="capture-hint">
        Click display to capture mouse
      </div>
    </div>

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
.state-badge {
  display: flex;
  gap: 0.5rem;
  align-items: center;
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

/* ── CRT effect ──────────────────────────────────────── */
.display-wrapper {
  position: relative;
}
.crt-effect {
  border-radius: 12px;
  overflow: hidden;
  box-shadow: 0 0 30px rgba(0, 200, 255, 0.15), inset 0 0 60px rgba(0, 0, 0, 0.3);
}
.crt-effect::after {
  content: '';
  position: absolute;
  inset: 0;
  pointer-events: none;
  background: repeating-linear-gradient(
    0deg,
    transparent,
    transparent 2px,
    rgba(0, 0, 0, 0.15) 2px,
    rgba(0, 0, 0, 0.15) 4px
  );
  z-index: 10;
}
.capture-hint {
  position: absolute;
  bottom: 8px;
  left: 50%;
  transform: translateX(-50%);
  background: rgba(0, 0, 0, 0.7);
  color: #fff;
  padding: 0.3rem 0.8rem;
  border-radius: 4px;
  font-size: 0.8em;
  z-index: 20;
  pointer-events: none;
}
</style>
