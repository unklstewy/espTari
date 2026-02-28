<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { api, type MachineProfile } from '../services/api'

const machines = ref<MachineProfile[]>([])
const selected = ref<string | null>(null)
const loading = ref(true)
const loadError = ref('')

const emit = defineEmits<{
  select: [machine: MachineProfile]
}>()

async function load() {
  try {
    machines.value = await api.getMachines()
    loading.value = false
  } catch (e: any) {
    loadError.value = e.message || 'Failed to load machines'
    loading.value = false
  }
}

function select(m: MachineProfile) {
  selected.value = m.model
  emit('select', m)
}

onMounted(load)
</script>

<template>
  <div class="machine-selector">
    <h3 class="section-title">Machine</h3>
    <div v-if="loading" class="loading">Loading machines…</div>
    <div v-else-if="loadError" class="load-error">{{ loadError }}</div>
    <div v-else class="machine-grid">
      <button
        v-for="m in machines"
        :key="m.model"
        class="machine-card"
        :class="{ active: selected === m.model }"
        @click="select(m)"
      >
        <div class="machine-name">{{ m.name }}</div>
        <div class="machine-specs">
          {{ m.cpu.core.toUpperCase() }} @ {{ (m.cpu.clock_hz / 1e6).toFixed(0) }}MHz
          · {{ m.memory.ram_kb }}KB
        </div>
        <div class="machine-chips">
          {{ [m.video.chip, m.audio.chip, ...m.peripherals].join(' · ') }}
        </div>
      </button>
    </div>
  </div>
</template>

<style scoped>
.section-title {
  font-size: 0.8rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-muted);
  margin-bottom: 0.75rem;
}
.machine-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 0.75rem;
}
.machine-card {
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 0.75rem 1rem;
  text-align: left;
  color: var(--text-primary);
  transition: border-color 200ms, background 200ms;
}
.machine-card:hover {
  border-color: var(--accent-dim);
  background: var(--bg-hover);
}
.machine-card.active {
  border-color: var(--accent);
  background: var(--accent-glow);
}
.machine-name {
  font-weight: 600;
  font-size: 0.95rem;
  margin-bottom: 0.25rem;
}
.machine-specs {
  font-size: 0.78rem;
  color: var(--text-secondary);
}
.machine-chips {
  font-size: 0.7rem;
  color: var(--text-muted);
  margin-top: 0.35rem;
}
.loading, .load-error {
  color: var(--text-muted);
  font-size: 0.85rem;
}
.load-error { color: var(--error); }
</style>
