<script setup lang="ts">
/**
 * DisplayCanvas — renders JPEG video frames received from the
 * WebSocket stream via createImageBitmap → canvas.drawImage.
 * Falls back to a retro "no signal" screen when not streaming.
 */
import { ref, onMounted, onUnmounted, watch } from 'vue'

const props = defineProps<{
  /** Raw JPEG bytes from the stream (reactive) */
  jpegFrame?: Uint8Array | null
  /** Frame width reported by backend */
  frameWidth?: number
  /** Frame height reported by backend */
  frameHeight?: number
  /** Whether the stream is connected */
  connected?: boolean
}>()

const canvas = ref<HTMLCanvasElement | null>(null)
const label = ref('640 × 400 · No Signal')

const ST_WIDTH = 640
const ST_HEIGHT = 400

/* ── No-signal pattern ─────────────────────────────────── */
function drawNoSignal(ctx: CanvasRenderingContext2D) {
  ctx.fillStyle = '#0a0c10'
  ctx.fillRect(0, 0, ST_WIDTH, ST_HEIGHT)

  ctx.fillStyle = 'rgba(255,255,255,0.015)'
  for (let y = 0; y < ST_HEIGHT; y += 2) ctx.fillRect(0, y, ST_WIDTH, 1)

  const grad = ctx.createRadialGradient(
    ST_WIDTH / 2, ST_HEIGHT / 2, ST_HEIGHT * 0.3,
    ST_WIDTH / 2, ST_HEIGHT / 2, ST_HEIGHT * 0.7,
  )
  grad.addColorStop(0, 'rgba(0,0,0,0)')
  grad.addColorStop(1, 'rgba(0,0,0,0.5)')
  ctx.fillStyle = grad
  ctx.fillRect(0, 0, ST_WIDTH, ST_HEIGHT)

  ctx.font = '16px "JetBrains Mono", monospace'
  ctx.textAlign = 'center'
  ctx.fillStyle = '#333a4a'
  ctx.fillText('NO SIGNAL', ST_WIDTH / 2, ST_HEIGHT / 2 - 10)
  ctx.font = '11px "JetBrains Mono", monospace'
  ctx.fillStyle = '#252a38'
  ctx.fillText('Waiting for WebSocket stream…', ST_WIDTH / 2, ST_HEIGHT / 2 + 12)
}

/* ── JPEG → canvas ─────────────────────────────────────── */
let rendering = false

async function renderJpeg(jpeg: Uint8Array) {
  if (rendering || !canvas.value) return
  rendering = true
  try {
    const blob = new Blob([jpeg as BlobPart], { type: 'image/jpeg' })
    const bmp = await createImageBitmap(blob)
    const ctx = canvas.value.getContext('2d')!
    ctx.drawImage(bmp, 0, 0, ST_WIDTH, ST_HEIGHT)
    bmp.close()
  } catch {
    /* corrupt frame — skip */
  }
  rendering = false
}

/* ── Reactive watchers ─────────────────────────────────── */
watch(() => props.jpegFrame, (jpeg) => {
  if (jpeg && jpeg.byteLength > 0) renderJpeg(jpeg)
})

watch(() => props.connected, (c) => {
  if (!c) {
    const ctx = canvas.value?.getContext('2d')
    if (ctx) drawNoSignal(ctx)
  }
})

watch(
  () => [props.frameWidth, props.frameHeight, props.connected] as const,
  ([w, h, c]) => {
    if (c && w && h) {
      label.value = `${w} × ${h} · WebSocket Stream`
    } else {
      label.value = `${ST_WIDTH} × ${ST_HEIGHT} · No Signal`
    }
  },
)

/* ── Lifecycle ─────────────────────────────────────────── */
onMounted(() => {
  const ctx = canvas.value?.getContext('2d')
  if (ctx) drawNoSignal(ctx)
})

onUnmounted(() => { /* nothing to clean up */ })
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
      {{ label }}
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
