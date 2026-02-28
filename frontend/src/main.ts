import { createApp } from 'vue'
import { createRouter, createWebHistory } from 'vue-router'
import './style.css'
import App from './App.vue'

import DashboardPage from './pages/DashboardPage.vue'
import EmulatorPage from './pages/EmulatorPage.vue'
import ConfigPage from './pages/ConfigPage.vue'
import FilesPage from './pages/FilesPage.vue'
import NetworkPage from './pages/NetworkPage.vue'
import UpdatePage from './pages/UpdatePage.vue'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/', name: 'dashboard', component: DashboardPage },
    { path: '/emulator', name: 'emulator', component: EmulatorPage },
    { path: '/config', name: 'config', component: ConfigPage },
    { path: '/files', name: 'files', component: FilesPage },
    { path: '/network', name: 'network', component: NetworkPage },
    { path: '/update', name: 'update', component: UpdatePage },
  ],
})

createApp(App).use(router).mount('#app')
