<script lang="ts">
  /** Route-level lazy loader for protocol laboratories. */
  import type { Component } from 'svelte';

  interface Props { tab: string; }
  type ViewModule = { default: Component };
  type ViewLoader = () => Promise<ViewModule>;

  const { tab }: Props = $props();
  const loaders: Record<string, ViewLoader> = {
    jsn: () => import('./JsnCatalog.svelte'),
    tqlex: () => import('./TqlexConsole.svelte'),
    pbrpc: () => import('./PbrpcConsole.svelte'),
    formulas: () => import('./FormulaLibrary.svelte'),
    routes: () => import('./CloudRoutes.svelte'),
    workflows: () => import('./CloudWorkflows.svelte'),
    coverage: () => import('./CoverageAudit.svelte'),
    pools: () => import('./TPoolLab.svelte'),
    level2: () => import('./Level2Lab.svelte')
  };
  const loader = $derived(loaders[tab] ?? loaders.jsn);
</script>

{#key tab}
  {#await loader()}
    <div class="route-state">正在加载协议模块…</div>
  {:then loaded}
    {@const View = loaded.default}
    <View />
  {:catch error}
    <div class="route-state error">协议模块加载失败：{error instanceof Error ? error.message : String(error)}</div>
  {/await}
{/key}

<style>
  .route-state {
    display: grid;
    flex: 1;
    place-items: center;
    min-height: 120px;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }
  .route-state.error { color: var(--negative); }
</style>
