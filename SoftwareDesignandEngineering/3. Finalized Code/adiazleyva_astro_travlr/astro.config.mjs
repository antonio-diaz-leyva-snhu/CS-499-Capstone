// @ts-check
import { defineConfig } from 'astro/config';
import node from '@astrojs/node';

// Pages are prerendered by default. Pages that need live data (trip listing,
// admin, API) opt out with `export const prerender = false`, which is why the
// Node adapter is required.
export default defineConfig({
  adapter: node({ mode: 'standalone' }),
});
