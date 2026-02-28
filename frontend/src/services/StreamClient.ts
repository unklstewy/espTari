/**
 * StreamClient — WebSocket binary A/V stream receiver
 *
 * Connects to the espTari /ws endpoint and parses the binary protocol:
 *   Video: [0x01][frame_num:4LE][ts_ms:4LE][w:2LE][h:2LE][JPEG data…]
 *   Audio: [0x02][ts_ms:4LE][samples:2LE][ch:1][bits:1][PCM data…]
 */

/* ── Packet constants (must match esptari_stream.h) ─────── */
const PKT_VIDEO = 0x01
const PKT_AUDIO = 0x02
const VIDEO_HDR = 13
const AUDIO_HDR = 9

/* ── Types ──────────────────────────────────────────────── */
export interface VideoFrameMeta {
  frameNum: number
  timestampMs: number
  width: number
  height: number
}

export interface AudioChunkMeta {
  timestampMs: number
  sampleCount: number
  channels: number
  bits: number
}

export interface StreamStats {
  connected: boolean
  framesReceived: number
  audioChunksReceived: number
  bytesReceived: number
  fps: number
}

/* ── Helpers ────────────────────────────────────────────── */
function u16(buf: DataView, off: number): number {
  return buf.getUint16(off, true)
}
function u32(buf: DataView, off: number): number {
  return buf.getUint32(off, true)
}

/* ── StreamClient class ─────────────────────────────────── */
export class StreamClient {
  private ws: WebSocket | null = null
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null
  private url = ''
  private _stats: StreamStats = {
    connected: false,
    framesReceived: 0,
    audioChunksReceived: 0,
    bytesReceived: 0,
    fps: 0,
  }
  private fpsFrames = 0
  private fpsTimer: ReturnType<typeof setInterval> | null = null

  /* Callbacks */
  onVideoFrame: ((jpeg: Uint8Array, meta: VideoFrameMeta) => void) | null = null
  onAudioChunk: ((pcm: ArrayBuffer, meta: AudioChunkMeta) => void) | null = null
  onConnected: (() => void) | null = null
  onDisconnected: (() => void) | null = null

  get stats(): StreamStats {
    return { ...this._stats }
  }

  /** Connect to the WebSocket stream endpoint */
  connect(host?: string) {
    if (this.ws) this.disconnect()

    const h = host || window.location.host
    const proto = window.location.protocol === 'https:' ? 'wss' : 'ws'
    this.url = `${proto}://${h}/ws`

    this._open()

    /* FPS counter — 1 Hz */
    this.fpsTimer = setInterval(() => {
      this._stats.fps = this.fpsFrames
      this.fpsFrames = 0
    }, 1000)
  }

  /** Disconnect and stop reconnecting */
  disconnect() {
    this._clearReconnect()
    if (this.fpsTimer) { clearInterval(this.fpsTimer); this.fpsTimer = null }
    if (this.ws) {
      this.ws.onclose = null
      this.ws.close()
      this.ws = null
    }
    this._stats.connected = false
  }

  /* ── internals ──────────────────────────────────────── */

  private _open() {
    const ws = new WebSocket(this.url)
    ws.binaryType = 'arraybuffer'

    ws.onopen = () => {
      this._stats.connected = true
      this.onConnected?.()
    }

    ws.onclose = () => {
      this._stats.connected = false
      this.ws = null
      this.onDisconnected?.()
      this._scheduleReconnect()
    }

    ws.onerror = () => {
      /* onclose will fire after onerror */
    }

    ws.onmessage = (ev: MessageEvent) => {
      if (!(ev.data instanceof ArrayBuffer)) return
      this._handleBinary(ev.data)
    }

    this.ws = ws
  }

  private _handleBinary(data: ArrayBuffer) {
    if (data.byteLength < 1) return
    this._stats.bytesReceived += data.byteLength

    const view = new DataView(data)
    const type = view.getUint8(0)

    if (type === PKT_VIDEO && data.byteLength > VIDEO_HDR) {
      const meta: VideoFrameMeta = {
        frameNum: u32(view, 1),
        timestampMs: u32(view, 5),
        width: u16(view, 9),
        height: u16(view, 11),
      }
      /* slice() creates a NEW ArrayBuffer — required so Blob gets only JPEG bytes */
      const jpeg = new Uint8Array(data.slice(VIDEO_HDR))
      this._stats.framesReceived++
      this.fpsFrames++
      this.onVideoFrame?.(jpeg, meta)
    } else if (type === PKT_AUDIO && data.byteLength > AUDIO_HDR) {
      const meta: AudioChunkMeta = {
        timestampMs: u32(view, 1),
        sampleCount: u16(view, 5),
        channels: view.getUint8(7),
        bits: view.getUint8(8),
      }
      const pcm = data.slice(AUDIO_HDR)
      this._stats.audioChunksReceived++
      this.onAudioChunk?.(pcm, meta)
    }
  }

  private _scheduleReconnect() {
    this._clearReconnect()
    this.reconnectTimer = setTimeout(() => this._open(), 2000)
  }

  private _clearReconnect() {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer)
      this.reconnectTimer = null
    }
  }
}
