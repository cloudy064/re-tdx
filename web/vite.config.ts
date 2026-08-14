import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

export default defineConfig({
  // The C++ server exposes history-style routes such as /protocol/formulas.
  // Root-relative assets keep direct navigation and refresh from resolving
  // JavaScript under the nested route (for example /protocol/assets/*).
  base: '/',
  plugins: [svelte()],
  server: {
    host: '127.0.0.1',
    port: 5173,
    proxy: {
      '/api': 'http://127.0.0.1:8765'
    }
  },
  build: {
    target: 'es2020',
    sourcemap: false
  }
});
