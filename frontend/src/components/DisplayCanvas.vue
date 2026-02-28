<script setup lang="ts">
/**
 * DisplayCanvas — placeholder for Phase 3 video streaming.
 * Shows a retro-styled "no signal" screen until the emulator
 * is running and streaming frames via WebSocket.
 */
import { ref, onMounted, onUnmounted } from 'vue'

const canvas = ref<HTMLCanvasElement | null>(null)
let animId = 0

const ST_WIDTH = 640
const ST_HEIGHT = 400

function drawNoSignal(ctx: CanvasRenderingContext2D) {
  ctx.fillStyle = '#0a0c10'
  ctx.fillRect(0, 0, ST_WIDTH, ST_HEIGHT)

  // Subtle scanlines
  ctx.fillStyle = 'rgba(255,255,255,0.015)'
  for (let y = 0; y < ST_HEIGHT; y += 2) {
    ctx.fillRect(0, y, ST_WIDTH, 1)
  }

  // CRT vignette
  const grad = ctx.createRadialGradient(
    ST_WIDTH / 2, ST_HEIGHT / 2, ST_HEIGHT * 0.3,
    ST_WIDTH / 2, ST_HEIGHT / 2, ST_HEIGHT * 0.7
  )
  grad.addColorStop(0, 'rgba(0,0,0,0)')
  grad.addColorStop(1, 'rgba(0,0,0,0.5)')
  ctx.fillStyle = grad
  ctx.fillRect(0, 0, ST_WIDTH, ST_HEIGHT)

  // "No Signal" text
  ctx.font = '16px "JetBrains Mono", monospace'
  ctx.textAlign = 'center'
  ctx.fillStyle = '#333a4a'
  ctx.fillText('NO SIGNAL', ST_WIDTH / 2, ST_HEIGHT / 2 - 10)
  ctx.font = '11px "JetBrains Mono", monospace'
  ctx.fillStyle = '#252a38'
  ctx.fillText('Start emulation to begin streaming', ST_WIDTH / 2, ST_HEIGHT / 2 + 12)
}

onMounted(() => {
  const ctx = canvas.value?.getContext('2d')
  if (ctx) drawNoSignal(ctx)
})

onUnmounted(() => {
  if (animId) cancelAnimationFrame(animId)
})
</script>

<template>
  <div class="display-wrapper">
    <div class="display-bezel">
      <canvas
        ref="canvas"
        :width="ST_WIDTH"
        :height="ST_HEIGHT"
        class="display-canvas"
      />
    </div>
    <div class="display-label">
      640 × 400 · ST Low Resolution · WebSocket
    </div>
  </div>
</template>

<style scoped>
.display-wrapper {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.5rem;
}
.display-bezel {
  background: #080a0e;
  border: 2px solid var(--border);
  border-radius: 6px;
  padding: 4px;
  box-shadow: 0 0 30px rgba(0, 0, 0, 0.6), inset 0 0 20px rgba(0, 0, 0, 0.4);
}
.display-canvas {
  display: block;
  width: 640px;
  height: 400px;
  image-rendering: pixelated;
}
.display-label {
  font-size: 0.7rem;
  color: var(--text-muted);
  letter-spacing: 0.05em;
}

@media (max-width: 720px) {
  .display-canvas {
    width: 100%;
    height: auto;
  }
}
</style>
