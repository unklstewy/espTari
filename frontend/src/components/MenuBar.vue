<script setup lang="ts">
import { useRoute } from 'vue-router'

const route = useRoute()

const navItems = [
  { path: '/', label: 'Dashboard', icon: '📊' },
  { path: '/emulator', label: 'Emulator', icon: '🕹️' },
  { path: '/config', label: 'Config', icon: '⚙️' },
  { path: '/files', label: 'Files', icon: '📁' },
  { path: '/network', label: 'Network', icon: '📡' },
  { path: '/update', label: 'Update', icon: '🔄' },
  { path: '/tracking', label: 'Tracking', icon: '🗂️' },
]
</script>

<template>
  <nav class="menu-bar">
    <div class="brand">
      <span class="brand-icon">🎮</span>
      <span class="brand-text">espTari</span>
    </div>
    <div class="nav-items">
      <router-link
        v-for="item in navItems"
        :key="item.path"
        :to="item.path"
        class="nav-item"
        :class="{ active: route.path === item.path }"
      >
        <span class="nav-icon">{{ item.icon }}</span>
        <span class="nav-label">{{ item.label }}</span>
      </router-link>
    </div>
  </nav>
</template>

<style scoped>
.menu-bar {
  display: flex;
  align-items: center;
  height: 48px;
  padding: 0 1rem;
  background: var(--bg-secondary);
  border-bottom: 1px solid var(--border);
  flex-shrink: 0;
}
.brand {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  margin-right: 2rem;
}
.brand-icon { font-size: 1.2rem; }
.brand-text {
  font-size: 1.1rem;
  font-weight: 700;
  color: var(--accent);
  letter-spacing: 0.05em;
}
.nav-items {
  display: flex;
  gap: 0.25rem;
}
.nav-item {
  display: flex;
  align-items: center;
  gap: 0.35rem;
  padding: 0.35rem 0.75rem;
  border-radius: var(--radius-sm);
  color: var(--text-secondary);
  text-decoration: none;
  font-size: 0.85rem;
  transition: background 150ms, color 150ms;
}
.nav-item:hover {
  background: var(--bg-hover);
  color: var(--text-primary);
  text-decoration: none;
}
.nav-item.active {
  background: var(--accent-glow);
  color: var(--accent);
}
.nav-icon { font-size: 1rem; }
</style>
