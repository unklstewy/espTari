<script setup lang="ts">
import MenuBar from './components/MenuBar.vue'
import StatusBar from './components/StatusBar.vue'
</script>

<template>
  <MenuBar />
  <main class="main-content">
    <router-view />
  </main>
  <StatusBar />
</template>

<style scoped>
.main-content {
  flex: 1;
  overflow-y: auto;
  padding: 1rem 1.5rem;
}
</style>
