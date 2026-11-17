import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'
import { resolve } from 'path';

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [svelte()],
  resolve: {
    alias: {
      '@': resolve(__dirname, 'node_modules'),
    }
  },
  server: {
    proxy: {
      '/api': {
        target: "http://127.0.0.1:3030",
        changeOrigin: true
      }
    }
  }
})
