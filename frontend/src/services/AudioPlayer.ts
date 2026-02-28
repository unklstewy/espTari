/**
 * AudioPlayer — Web Audio API PCM playback for espTari stream
 *
 * Receives raw PCM16 LE samples from the WebSocket stream and plays
 * them back through a ScriptProcessorNode (simple, works everywhere).
 * Buffers ~100 ms of audio to absorb jitter.
 */

const BUFFER_SIZE = 4096       /* ScriptProcessor buffer size (samples) */
const MAX_QUEUE = 8            /* max queued chunks (~160 ms at 4096/44100) */

export class AudioPlayer {
  private ctx: AudioContext | null = null
  private processor: ScriptProcessorNode | null = null
  private gain: GainNode | null = null

  /* Ring of Float32 stereo chunks ready for playback */
  private queue: { left: Float32Array; right: Float32Array }[] = []
  private playOffset = 0       /* sample offset into queue[0] */

  private _channels = 2
  private _sampleRate = 44100
  private _muted = false

  /** Current channels (read by caller if needed) */
  get channels() { return this._channels }
  /** Current sample rate (read by caller if needed) */
  get sampleRate() { return this._sampleRate }

  get muted() { return this._muted }
  set muted(v: boolean) {
    this._muted = v
    if (this.gain) this.gain.gain.value = v ? 0 : 1
  }

  /**
   * Start audio playback context.
   * Must be called from a user gesture the first time.
   */
  start(sampleRate = 44100, channels = 2) {
    if (this.ctx) return
    this._sampleRate = sampleRate
    this._channels = channels

    this.ctx = new AudioContext({ sampleRate })
    this.gain = this.ctx.createGain()
    this.gain.gain.value = this._muted ? 0 : 1
    this.gain.connect(this.ctx.destination)

    this.processor = this.ctx.createScriptProcessor(BUFFER_SIZE, 0, 2)
    this.processor.onaudioprocess = (e) => this._process(e)
    this.processor.connect(this.gain)
  }

  /** Stop playback and release audio context */
  stop() {
    if (this.processor) {
      this.processor.disconnect()
      this.processor = null
    }
    if (this.gain) {
      this.gain.disconnect()
      this.gain = null
    }
    if (this.ctx) {
      this.ctx.close()
      this.ctx = null
    }
    this.queue = []
    this.playOffset = 0
  }

  /**
   * Feed raw PCM data from the stream.
   * @param pcm  ArrayBuffer of PCM16 LE samples (interleaved if stereo)
   * @param channels  1 (mono) or 2 (stereo)
   */
  feed(pcm: ArrayBuffer, channels: number) {
    if (!this.ctx) return

    const samples = new Int16Array(pcm)
    const nFrames = channels >= 2 ? samples.length / 2 : samples.length
    const left = new Float32Array(nFrames)
    const right = new Float32Array(nFrames)

    if (channels >= 2) {
      for (let i = 0; i < nFrames; i++) {
        left[i] = (samples[i * 2] ?? 0) / 32768
        right[i] = (samples[i * 2 + 1] ?? 0) / 32768
      }
    } else {
      for (let i = 0; i < nFrames; i++) {
        left[i] = right[i] = (samples[i] ?? 0) / 32768
      }
    }

    /* Drop oldest if too far behind */
    if (this.queue.length >= MAX_QUEUE) {
      this.queue.shift()
      this.playOffset = 0
    }
    this.queue.push({ left, right })
  }

  /** Resume suspended AudioContext (call from user gesture) */
  async resume() {
    if (this.ctx?.state === 'suspended') {
      await this.ctx.resume()
    }
  }

  /* ── internals ──────────────────────────────────────── */

  private _process(e: AudioProcessingEvent) {
    const outL = e.outputBuffer.getChannelData(0)
    const outR = e.outputBuffer.getChannelData(1)
    let written = 0

    while (written < outL.length && this.queue.length > 0) {
      const chunk = this.queue[0]!
      const avail = chunk.left.length - this.playOffset
      const need = outL.length - written
      const n = Math.min(avail, need)

      outL.set(chunk.left.subarray(this.playOffset, this.playOffset + n), written)
      outR.set(chunk.right.subarray(this.playOffset, this.playOffset + n), written)

      written += n
      this.playOffset += n

      if (this.playOffset >= chunk.left.length) {
        this.queue.shift()
        this.playOffset = 0
      }
    }

    /* Fill remainder with silence */
    if (written < outL.length) {
      outL.fill(0, written)
      outR.fill(0, written)
    }
  }
}
