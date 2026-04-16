import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import svgr from "vite-plugin-svgr"

export default defineConfig({
  build: {
    chunkSizeWarningLimit: 2000
  },
  plugins: [react(),svgr()],
  server: {
    proxy: {
      '/resources': {
        target: 'http://192.168.1.33',
        changeOrigin: false,
      },
      '/var': {
        target: 'http://192.168.1.33',
        changeOrigin: false,
      },
      '/pipedal': {
        target: 'ws://192.168.1.33',
        ws: true,
        changeOrigin: false,
      },
    }
  }
})
