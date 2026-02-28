/**
 * espTari API service — typed fetch wrapper for all backend endpoints
 */

const BASE = ''  // same origin when served from ESP32

export interface SystemStatus {
  status: string
  free_heap: number
  free_psram: number
  uptime_ms: number
  version: string
}

export interface MachineProfile {
  name: string
  model: string
  cpu: { core: string; clock_hz: number }
  mmu: string
  memory: { ram_kb: number; rom: string }
  video: { chip: string; default_resolution: string }
  audio: { chip: string }
  peripherals: string[]
}

export interface EmulatorState {
  state: string          // stopped | running | paused | error
  free_heap: number
  total_heap: number
  free_psram: number
  total_psram: number
  min_free_heap: number
  uptime_ms: number
}

export interface RomInfo {
  name: string
  category: string       // tos | cartridge | bios
  size: number
}

export interface NetworkStatus {
  connected: boolean
  hostname: string
  wifi: {
    enabled: boolean
    status: string       // down | started | connected | got_ip
    ip: string
    netmask: string
    gateway: string
    mac: string
  }
  ethernet: {
    enabled: boolean
    status: string
    ip: string
    netmask: string
    gateway: string
    mac: string
  }
  mdns_enabled: boolean
}

export interface StreamStatsResponse {
  frames_sent: number
  audio_chunks_sent: number
  bytes_sent: number
  fps: number
  clients: number
  dropped_frames: number
  encode_time_us: number
  jpeg_quality: number
}

async function get<T>(path: string): Promise<T> {
  const res = await fetch(`${BASE}${path}`)
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`)
  return res.json()
}

async function post<T>(path: string, body?: unknown): Promise<T> {
  const res = await fetch(`${BASE}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: body ? JSON.stringify(body) : undefined,
  })
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`)
  return res.json()
}

export const api = {
  /** Live system status (heap, PSRAM, uptime) */
  getStatus: () => get<SystemStatus>('/api/status'),

  /** Current emulator state */
  getSystem: () => get<EmulatorState>('/api/system'),

  /** Control emulator (action: start | stop | reset | pause) */
  controlSystem: (action: string) => post<EmulatorState>('/api/system', { action }),

  /** List available machine profiles */
  getMachines: () => get<MachineProfile[]>('/api/machines'),

  /** List available ROM files */
  getRoms: () => get<RomInfo[]>('/api/roms'),

  /** Network interface status */
  getNetworkStatus: () => get<NetworkStatus>('/api/network/status'),

  /** Streaming subsystem stats */
  getStreamStats: () => get<StreamStatsResponse>('/api/stream/stats'),
}
