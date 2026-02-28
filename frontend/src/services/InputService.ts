/**
 * InputService — captures browser keyboard, mouse, and gamepad events
 * and sends them to the ESP32 via WebSocket /ws/input.
 *
 * Binary protocol:
 *   Key:    [0x01][scancode:1][pressed:1]
 *   Mouse:  [0x02][dx:2LE][dy:2LE][buttons:1]
 *   Joy:    [0x03][port:1][state:1]
 */

/** Browser keyCode → Atari ST IKBD scan code mapping */
const KEY_MAP: Record<string, number> = {
  Escape: 0x01,
  Digit1: 0x02, Digit2: 0x03, Digit3: 0x04, Digit4: 0x05,
  Digit5: 0x06, Digit6: 0x07, Digit7: 0x08, Digit8: 0x09,
  Digit9: 0x0A, Digit0: 0x0B, Minus: 0x0C, Equal: 0x0D,
  Backspace: 0x0E, Tab: 0x0F,
  KeyQ: 0x10, KeyW: 0x11, KeyE: 0x12, KeyR: 0x13,
  KeyT: 0x14, KeyY: 0x15, KeyU: 0x16, KeyI: 0x17,
  KeyO: 0x18, KeyP: 0x19, BracketLeft: 0x1A, BracketRight: 0x1B,
  Enter: 0x1C, ControlLeft: 0x1D,
  KeyA: 0x1E, KeyS: 0x1F, KeyD: 0x20, KeyF: 0x21,
  KeyG: 0x22, KeyH: 0x23, KeyJ: 0x24, KeyK: 0x25,
  KeyL: 0x26, Semicolon: 0x27, Quote: 0x28, Backquote: 0x29,
  ShiftLeft: 0x2A, Backslash: 0x2B,
  KeyZ: 0x2C, KeyX: 0x2D, KeyC: 0x2E, KeyV: 0x2F,
  KeyB: 0x30, KeyN: 0x31, KeyM: 0x32,
  Comma: 0x33, Period: 0x34, Slash: 0x35, ShiftRight: 0x36,
  // Keypad
  NumpadMultiply: 0x66, AltLeft: 0x38, Space: 0x39,
  CapsLock: 0x3A,
  F1: 0x3B, F2: 0x3C, F3: 0x3D, F4: 0x3E, F5: 0x3F,
  F6: 0x40, F7: 0x41, F8: 0x42, F9: 0x43, F10: 0x44,
  // Numpad
  Numpad7: 0x67, Numpad8: 0x68, Numpad9: 0x69, NumpadSubtract: 0x4A,
  Numpad4: 0x6A, Numpad5: 0x6B, Numpad6: 0x6C, NumpadAdd: 0x4E,
  Numpad1: 0x6D, Numpad2: 0x6E, Numpad3: 0x6F, Numpad0: 0x70,
  NumpadDecimal: 0x71, NumpadEnter: 0x72,
  // Arrow keys
  ArrowUp: 0x48, ArrowDown: 0x50, ArrowLeft: 0x4B, ArrowRight: 0x4D,
  // Extra
  Insert: 0x52, Home: 0x47, Delete: 0x53,
  Help: 0x62, Undo: 0x61,
}

export class InputService {
  private ws: WebSocket | null = null
  private _enabled = false
  private _mouseCaptured = false
  private pointerLockElement: HTMLElement | null = null

  // Gamepad polling
  private gamepadTimer: ReturnType<typeof setInterval> | null = null
  private lastGamepadState: number[] = [0, 0]

  get enabled() { return this._enabled }
  get mouseCaptured() { return this._mouseCaptured }

  connect() {
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:'
    this.ws = new WebSocket(`${proto}//${location.host}/ws/input`)
    this.ws.binaryType = 'arraybuffer'
    this.ws.onopen = () => console.log('[Input] WS connected')
    this.ws.onclose = () => { this.ws = null; console.log('[Input] WS disconnected') }
  }

  disconnect() {
    this.disable()
    this.ws?.close()
    this.ws = null
  }

  enable(captureElement?: HTMLElement) {
    if (this._enabled) return
    this._enabled = true
    window.addEventListener('keydown', this.onKeyDown)
    window.addEventListener('keyup', this.onKeyUp)
    if (captureElement) {
      this.pointerLockElement = captureElement
      captureElement.addEventListener('click', this.requestPointerLock)
      document.addEventListener('pointerlockchange', this.onPointerLockChange)
      document.addEventListener('mousemove', this.onMouseMove)
      document.addEventListener('mousedown', this.onMouseDown)
      document.addEventListener('mouseup', this.onMouseUp)
    }
    this.startGamepadPolling()
  }

  disable() {
    this._enabled = false
    window.removeEventListener('keydown', this.onKeyDown)
    window.removeEventListener('keyup', this.onKeyUp)
    if (this.pointerLockElement) {
      this.pointerLockElement.removeEventListener('click', this.requestPointerLock)
      document.removeEventListener('pointerlockchange', this.onPointerLockChange)
      document.removeEventListener('mousemove', this.onMouseMove)
      document.removeEventListener('mousedown', this.onMouseDown)
      document.removeEventListener('mouseup', this.onMouseUp)
    }
    if (document.pointerLockElement) document.exitPointerLock()
    this._mouseCaptured = false
    this.stopGamepadPolling()
  }

  /* ── Key events ──────────────────────────────────────── */
  private onKeyDown = (e: KeyboardEvent) => {
    const sc = KEY_MAP[e.code]
    if (sc !== undefined) {
      e.preventDefault()
      this.sendKey(sc, true)
    }
  }

  private onKeyUp = (e: KeyboardEvent) => {
    const sc = KEY_MAP[e.code]
    if (sc !== undefined) {
      e.preventDefault()
      this.sendKey(sc, false)
    }
  }

  private sendKey(scancode: number, pressed: boolean) {
    const buf = new Uint8Array(3)
    buf[0] = 0x01
    buf[1] = scancode
    buf[2] = pressed ? 1 : 0
    this.send(buf)
  }

  /* ── Mouse events ────────────────────────────────────── */
  private requestPointerLock = () => {
    this.pointerLockElement?.requestPointerLock()
  }

  private onPointerLockChange = () => {
    this._mouseCaptured = document.pointerLockElement === this.pointerLockElement
  }

  private mouseButtons = 0

  private onMouseMove = (e: MouseEvent) => {
    if (!this._mouseCaptured) return
    this.sendMouse(e.movementX, e.movementY, this.mouseButtons)
  }

  private onMouseDown = (e: MouseEvent) => {
    if (!this._mouseCaptured) return
    if (e.button === 0) this.mouseButtons |= 1
    if (e.button === 2) this.mouseButtons |= 2
    this.sendMouse(0, 0, this.mouseButtons)
  }

  private onMouseUp = (e: MouseEvent) => {
    if (!this._mouseCaptured) return
    if (e.button === 0) this.mouseButtons &= ~1
    if (e.button === 2) this.mouseButtons &= ~2
    this.sendMouse(0, 0, this.mouseButtons)
  }

  private sendMouse(dx: number, dy: number, buttons: number) {
    const buf = new ArrayBuffer(6)
    const view = new DataView(buf)
    view.setUint8(0, 0x02)
    view.setInt16(1, dx, true)
    view.setInt16(3, dy, true)
    view.setUint8(5, buttons)
    this.send(new Uint8Array(buf))
  }

  /* ── Gamepad polling ─────────────────────────────────── */
  private startGamepadPolling() {
    this.gamepadTimer = setInterval(() => this.pollGamepads(), 16) // ~60Hz
  }

  private stopGamepadPolling() {
    if (this.gamepadTimer) { clearInterval(this.gamepadTimer); this.gamepadTimer = null }
  }

  private pollGamepads() {
    const gamepads = navigator.getGamepads()
    for (let port = 0; port < 2; port++) {
      const gp = gamepads[port]
      if (!gp) continue
      let state = 0
      // D-pad (axes or buttons)
      const ax = gp.axes[0] ?? 0
      const ay = gp.axes[1] ?? 0
      if (ay < -0.5 || gp.buttons[12]?.pressed) state |= 0x01 // up
      if (ay > 0.5  || gp.buttons[13]?.pressed) state |= 0x02 // down
      if (ax < -0.5 || gp.buttons[14]?.pressed) state |= 0x04 // left
      if (ax > 0.5  || gp.buttons[15]?.pressed) state |= 0x08 // right
      if (gp.buttons[0]?.pressed || gp.buttons[1]?.pressed) state |= 0x10 // fire

      if (state !== this.lastGamepadState[port]) {
        this.lastGamepadState[port] = state
        this.sendJoystick(port, state)
      }
    }
  }

  private sendJoystick(port: number, state: number) {
    const buf = new Uint8Array(3)
    buf[0] = 0x03
    buf[1] = port
    buf[2] = state
    this.send(buf)
  }

  /* ── WebSocket send ──────────────────────────────────── */
  private send(data: Uint8Array) {
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(data.buffer)
    }
  }
}
