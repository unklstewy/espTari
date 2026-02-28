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
  audio: { chip?: string; chips?: string[] }
  peripherals: string[]
  notes?: string
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

export interface DiskInfo {
  name: string
  type: string           // floppy | hard
  size: number
}

export interface DiskMountRequest {
  drive: number          // 0 = A, 1 = B
  path: string           // empty string to eject
}

export interface DiskMountResponse {
  ok: boolean
  drive: number
  path: string
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

export interface NetworkConfig {
  wifi_enabled: boolean
  eth_enabled: boolean
  hostname: string
  mdns_enabled: boolean
  wifi_ssid: string
  wifi_dhcp: boolean
  wifi_ip: string
  wifi_netmask: string
  wifi_gateway: string
  eth_dhcp: boolean
  eth_ip: string
  eth_netmask: string
  eth_gateway: string
  failover_enabled: boolean
}

export interface NetworkScanResult {
  supported: boolean
  note: string
  networks: Array<{ ssid: string; rssi: number; auth: string }>
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

export interface EmulatorConfig {
  machine: string
  tos_rom?: string
  memory_kb: number
  auto_start?: boolean
  web_port?: number
  stream_port?: number
  display: { resolution: string; palette?: string; crt_effects?: boolean }
  audio: { enabled?: boolean; sample_rate: number; volume?: number }
  input?: { mouse_speed?: number; joystick_port?: number }
}

export interface OtaStatus {
  version: string
  idf_version: string
  compile_date: string
  compile_time: string
  running_partition: string
  next_update_partition: string
  secure_version: number
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

async function put<T>(path: string, body: unknown): Promise<T> {
  const res = await fetch(`${BASE}${path}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  })
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`)
  return res.json()
}

export const api = {
  /** Live system status (heap, PSRAM, uptime) */
  getStatus: () => get<SystemStatus>('/api/status'),

  /** Current emulator state */
  getSystem: () => get<EmulatorState>('/api/system'),

  /** Control emulator (action: start | stop | reset | pause | resume) */
  controlSystem: (action: string) => post<EmulatorState>('/api/system', { action }),

  /** List available machine profiles */
  getMachines: () => get<MachineProfile[]>('/api/machines'),

  /** List available ROM files */
  getRoms: () => get<RomInfo[]>('/api/roms'),

  /** List available disk images */
  getDisks: () => get<DiskInfo[]>('/api/disks'),

  /** Mount/eject a disk image */
  mountDisk: (req: DiskMountRequest) => post<DiskMountResponse>('/api/disks/mount', req),

  /** Get emulator configuration */
  getConfig: () => get<EmulatorConfig>('/api/config'),

  /** Save emulator configuration */
  setConfig: (cfg: EmulatorConfig) => put<{ ok: boolean }>('/api/config', cfg),

  /** Get active machine */
  getActiveMachine: () => get<{ machine: string }>('/api/config/machine'),

  /** Set active machine */
  setActiveMachine: (machine: string) => put<{ machine: string }>('/api/config/machine', { machine }),

  /** Network interface status */
  getNetworkStatus: () => get<NetworkStatus>('/api/network/status'),

  /** Network configuration */
  getNetworkConfig: () => get<NetworkConfig>('/api/network/config'),

  /** Update network configuration */
  setNetworkConfig: (cfg: NetworkConfig) => put<{ ok: boolean; note: string }>('/api/network/config', cfg),

  /** Scan for WiFi networks */
  scanNetworks: () => get<NetworkScanResult>('/api/network/scan'),

  /** Streaming subsystem stats */
  getStreamStats: () => get<StreamStatsResponse>('/api/stream/stats'),

  /** OTA firmware status */
  getOtaStatus: () => get<OtaStatus>('/api/ota/status'),

  /** Upload OTA firmware binary */
  uploadFirmware: async (file: File, onProgress?: (pct: number) => void): Promise<{ ok: boolean; message: string }> => {
    return new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest()
      xhr.open('POST', `${BASE}/api/ota/upload`)
      xhr.setRequestHeader('Content-Type', 'application/octet-stream')
      if (onProgress) {
        xhr.upload.onprogress = (e) => {
          if (e.lengthComputable) onProgress(Math.round((e.loaded / e.total) * 100))
        }
      }
      xhr.onload = () => {
        if (xhr.status >= 200 && xhr.status < 300) {
          resolve(JSON.parse(xhr.responseText))
        } else {
          reject(new Error(`${xhr.status} ${xhr.statusText}`))
        }
      }
      xhr.onerror = () => reject(new Error('Network error'))
      xhr.send(file)
    })
  },

  /** Rollback to previous firmware */
  rollbackFirmware: () => post<{ ok: boolean }>('/api/ota/rollback'),
}
