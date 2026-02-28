import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vite.dev/config/
export default defineConfig({
  plugins: [vue()],
  build: {
    outDir: 'dist',
    // Keep assets small for embedded serving from ESP32-P4
    assetsInlineLimit: 4096,
    rollupOptions: {
      output: {
        // Single-chunk output for simpler embedded serving
        manualChunks: undefined,
      },
    },
  },
  server: {
    // Proxy API calls to the ESP32-P4 during development
    proxy: {
      '/api': {
        target: 'http://esptari.local',
        changeOrigin: true,
      },
      '/ws': {
        target: 'ws://esptari.local',
        ws: true,
      },
    },
  },
})
