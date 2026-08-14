<script lang="ts">
  import { onMount, type Component } from 'svelte';
  import { app } from './lib/store.svelte';
  import { router } from './lib/router.svelte';
  import NavRail from './shell/NavRail.svelte';
  import Omnibox from './shell/Omnibox.svelte';
  import TopBar from './shell/TopBar.svelte';
  import Icon from './ui/Icon.svelte';

  let searchOpen = $state(false);
  type RouteModule = { default: Component<any> };
  const routeLoaders: Record<string, () => Promise<RouteModule>> = {
    stock: () => import('./views/market/StockWorkbench.svelte'),
    ranking: () => import('./views/market/RankingView.svelte'),
    sectors: () => import('./views/sectors/SectorBrowser.svelte'),
    industry: () => import('./views/sectors/IndustryProfileView.svelte'),
    data: () => import('./views/data/DataCenter.svelte'),
    protocol: () => import('./views/protocol/ProtocolLab.svelte'),
    system: () => import('./views/system/SystemView.svelte')
  };
  const routeKey = $derived(routeLoaders[router.root] ? router.root : 'stock');
  const routeLoader = $derived(routeLoaders[routeKey]);

  onMount(() => {
    const stop = router.start();
    void app.loadHealth();
    return stop;
  });

  function onKeydown(event: KeyboardEvent) {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'k') {
      event.preventDefault();
      searchOpen = true;
    }
  }
</script>

<svelte:window onkeydown={onKeydown} />

<div class="shell">
  <TopBar onOpenSearch={() => (searchOpen = true)} />

  <div class="body">
    <NavRail />

    <main class="work">
      {#if app.healthError}
        <div class="offline">
          <Icon name="alert" size={14} />
          <div>
            <strong>无法连接本地后端</strong>
            <span>{app.healthError} —— 请确认 re-tdx 服务正在 127.0.0.1:8765 运行。</span>
          </div>
          <button type="button" onclick={() => void app.loadHealth()}>重试</button>
        </div>
      {/if}

      {#key routeKey}
        {#await routeLoader()}
          <div class="route-state">正在加载工作区…</div>
        {:then loaded}
          {@const View = loaded.default}
          {#if routeKey === 'data'}
            <View view={router.segment(1)} />
          {:else if routeKey === 'protocol'}
            <View tab={router.segment(1)} />
          {:else}
            <View />
          {/if}
        {:catch error}
          <div class="route-state error">工作区加载失败：{error instanceof Error ? error.message : String(error)}</div>
        {/await}
      {/key}
    </main>
  </div>

  <Omnibox open={searchOpen} onClose={() => (searchOpen = false)} />
</div>

<style>
  .shell {
    display: flex;
    flex-direction: column;
    height: 100%;
    overflow: hidden;
  }

  .body {
    display: flex;
    flex: 1;
    min-height: 0;
  }

  /* 工作区自身不滚动，由内部面板各自滚动——终端布局的关键 */
  .work {
    display: flex;
    flex: 1;
    flex-direction: column;
    gap: var(--sp-2);
    min-width: 0;
    min-height: 0;
    padding: var(--sp-2);
  }

  .route-state {
    display: grid;
    flex: 1;
    place-items: center;
    min-height: 120px;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }

  .route-state.error { color: var(--negative); }

  .offline {
    display: flex;
    flex: none;
    align-items: center;
    gap: var(--sp-3);
    padding: var(--sp-2) var(--sp-4);
    color: var(--up);
    background: var(--up-soft);
    border: 1px solid var(--up-soft);
    border-radius: var(--radius-lg);
  }

  .offline div {
    display: flex;
    flex: 1;
    flex-wrap: wrap;
    align-items: baseline;
    gap: var(--sp-2);
    min-width: 0;
  }

  .offline strong {
    font-size: var(--fs-body);
  }

  .offline span {
    font-size: var(--fs-micro);
    color: var(--fg-dim);
  }

  .offline button {
    flex: none;
    height: 22px;
    padding: 0 var(--sp-3);
    font-size: var(--fs-micro);
    color: var(--fg);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  .offline button:hover {
    background: var(--bg-hover);
  }
</style>
