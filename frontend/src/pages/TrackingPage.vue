<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'

type Overview = {
  tasks_by_status: { status: string; count: number }[]
  acceptance_count: number
  kanban_card_count: number
}

type TaskRow = {
  task_id: string
  epic: string
  objective: string
  priority: string
  size: string
  sprint: string
  status: string
  dependencies_raw: string
}

type AcceptanceRow = {
  id: number
  decision_date: string
  sprint: string
  task_ref: string
  decision: string
  notes: string
  evidence_link: string
}

type KanbanData = Record<string, { card_text: string; task_id: string | null }[]>

const apiBase = '/tracking-api'

const loading = ref(false)
const error = ref('')
const overview = ref<Overview | null>(null)
const tasks = ref<TaskRow[]>([])
const acceptance = ref<AcceptanceRow[]>([])
const kanban = ref<KanbanData>({})

const statusFilter = ref('')
const taskSearch = ref('')
const acceptanceTask = ref('')
const fullTextSearch = ref('')
const searchResults = ref<AcceptanceRow[]>([])

const columns = computed(() => Object.keys(kanban.value))

async function fetchJson<T>(path: string): Promise<T> {
  const response = await fetch(`${apiBase}${path}`)
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`)
  }
  const payload = await response.json()
  if (!payload.ok) {
    throw new Error(payload?.error?.code ?? 'REQUEST_FAILED')
  }
  return payload.data as T
}

async function loadOverview() {
  overview.value = await fetchJson<Overview>('/api/overview')
}

async function loadTasks() {
  const qs = new URLSearchParams()
  if (statusFilter.value) qs.set('status', statusFilter.value)
  if (taskSearch.value) qs.set('q', taskSearch.value)
  qs.set('limit', '120')
  tasks.value = await fetchJson<TaskRow[]>(`/api/tasks?${qs.toString()}`)
}

async function loadAcceptance() {
  const qs = new URLSearchParams()
  if (acceptanceTask.value) qs.set('task', acceptanceTask.value)
  qs.set('limit', '80')
  acceptance.value = await fetchJson<AcceptanceRow[]>(`/api/acceptance?${qs.toString()}`)
}

async function loadKanban() {
  kanban.value = await fetchJson<KanbanData>('/api/kanban')
}

async function runSearch() {
  if (!fullTextSearch.value.trim()) {
    searchResults.value = []
    return
  }
  const qs = new URLSearchParams({ q: fullTextSearch.value.trim() })
  searchResults.value = await fetchJson<AcceptanceRow[]>(`/api/search?${qs.toString()}`)
}

async function loadAll() {
  loading.value = true
  error.value = ''
  try {
    await Promise.all([loadOverview(), loadTasks(), loadAcceptance(), loadKanban()])
  } catch (err) {
    error.value = err instanceof Error ? err.message : 'Failed to load tracking data'
  } finally {
    loading.value = false
  }
}

onMounted(loadAll)
</script>

<template>
  <div class="tracking-page">
    <h1 class="page-title">Tracking Database</h1>
    <p class="page-subtitle">Read-only SQLite-backed view of backlog, kanban, and acceptance logs.</p>

    <div class="toolbar card">
      <div class="toolbar-actions">
        <button class="btn" @click="loadAll">Refresh</button>
      </div>
      <div class="toolbar-status" v-if="loading">Loading…</div>
      <div class="toolbar-status error" v-else-if="error">{{ error }}</div>
    </div>

    <section class="card" v-if="overview">
      <h3 class="section-title">Overview</h3>
      <div class="overview-grid">
        <div class="metric">
          <div class="metric-label">Acceptance rows</div>
          <div class="metric-value">{{ overview.acceptance_count }}</div>
        </div>
        <div class="metric">
          <div class="metric-label">Kanban cards</div>
          <div class="metric-value">{{ overview.kanban_card_count }}</div>
        </div>
      </div>
      <div class="status-chips">
        <span v-for="row in overview.tasks_by_status" :key="row.status" class="badge info">
          {{ row.status }}: {{ row.count }}
        </span>
      </div>
    </section>

    <section class="card">
      <h3 class="section-title">Tasks</h3>
      <div class="filters">
        <input v-model="statusFilter" class="input" placeholder="Status filter (e.g. Done)" />
        <input v-model="taskSearch" class="input" placeholder="Search task id/objective" />
        <button class="btn" @click="loadTasks">Apply</button>
      </div>
      <div class="table-wrap">
        <table class="table">
          <thead>
            <tr>
              <th>Task</th>
              <th>Status</th>
              <th>Epic</th>
              <th>Objective</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="row in tasks" :key="row.task_id">
              <td>{{ row.task_id }}</td>
              <td>{{ row.status }}</td>
              <td>{{ row.epic }}</td>
              <td>{{ row.objective }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </section>

    <section class="card">
      <h3 class="section-title">Acceptance</h3>
      <div class="filters">
        <input v-model="acceptanceTask" class="input" placeholder="Filter by task reference" />
        <button class="btn" @click="loadAcceptance">Apply</button>
      </div>
      <div class="table-wrap">
        <table class="table">
          <thead>
            <tr>
              <th>Date</th>
              <th>Task</th>
              <th>Decision</th>
              <th>Notes</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="row in acceptance" :key="row.id">
              <td>{{ row.decision_date }}</td>
              <td>{{ row.task_ref }}</td>
              <td>{{ row.decision }}</td>
              <td>{{ row.notes }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </section>

    <section class="card">
      <h3 class="section-title">Full-text Search (Acceptance)</h3>
      <div class="filters">
        <input v-model="fullTextSearch" class="input" placeholder="Search notes/evidence (FTS5)" />
        <button class="btn" @click="runSearch">Search</button>
      </div>
      <ul class="search-list">
        <li v-for="row in searchResults" :key="row.id">
          <strong>{{ row.task_ref }}</strong> · {{ row.decision_date }} · {{ row.decision }}
          <div class="muted">{{ row.notes }}</div>
        </li>
      </ul>
    </section>

    <section class="card">
      <h3 class="section-title">Kanban</h3>
      <div class="kanban-grid">
        <div class="kanban-col" v-for="column in columns" :key="column">
          <h4>{{ column }}</h4>
          <ul>
            <li v-for="card in kanban[column]" :key="card.card_text">
              {{ card.card_text }}
            </li>
          </ul>
        </div>
      </div>
    </section>
  </div>
</template>

<style scoped>
.tracking-page {
  max-width: 1200px;
  display: grid;
  gap: 1rem;
}
.page-title {
  font-size: 1.2rem;
  font-weight: 700;
}
.page-subtitle {
  color: var(--text-secondary);
  font-size: 0.9rem;
}
.toolbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.toolbar-status.error {
  color: var(--error);
}
.overview-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 0.75rem;
  margin-bottom: 0.75rem;
}
.metric-label {
  color: var(--text-muted);
  font-size: 0.75rem;
}
.metric-value {
  font-size: 1.1rem;
  font-weight: 700;
}
.status-chips {
  display: flex;
  gap: 0.4rem;
  flex-wrap: wrap;
}
.section-title {
  margin-bottom: 0.75rem;
  text-transform: uppercase;
  font-size: 0.78rem;
  letter-spacing: 0.08em;
  color: var(--text-muted);
}
.filters {
  display: flex;
  gap: 0.5rem;
  margin-bottom: 0.75rem;
}
.input {
  flex: 1;
  min-width: 200px;
  background: var(--bg-hover);
  border: 1px solid var(--border-light);
  color: var(--text-primary);
  border-radius: var(--radius-sm);
  padding: 0.45rem 0.55rem;
}
.table-wrap {
  overflow: auto;
  max-height: 300px;
}
.table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.82rem;
}
.table th,
.table td {
  border-bottom: 1px solid var(--border);
  text-align: left;
  padding: 0.4rem 0.5rem;
  vertical-align: top;
}
.table th {
  color: var(--text-muted);
  position: sticky;
  top: 0;
  background: var(--bg-card);
}
.search-list {
  display: grid;
  gap: 0.5rem;
  list-style: none;
}
.search-list .muted {
  color: var(--text-secondary);
  font-size: 0.82rem;
}
.kanban-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
  gap: 0.75rem;
}
.kanban-col {
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  padding: 0.5rem;
  background: var(--bg-hover);
}
.kanban-col h4 {
  margin-bottom: 0.4rem;
  font-size: 0.85rem;
}
.kanban-col ul {
  list-style: none;
  display: grid;
  gap: 0.35rem;
}
.kanban-col li {
  font-size: 0.8rem;
  color: var(--text-secondary);
}
</style>
