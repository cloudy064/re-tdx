<script lang="ts">
  /**
   * 主从分栏。左侧主表，右侧关联详情，各自滚动、总高填满工作区。
   * 窄屏下自动改为上下堆叠。
   */
  import type { Snippet } from 'svelte';

  interface Props {
    /** 右栏宽度。默认 340px，机构/股东这类长名称的详情可加宽。 */
    asideWidth?: string;
    /** 不传 aside 时主区独占整宽。 */
    main: Snippet;
    aside?: Snippet;
  }

  const { asideWidth = '340px', main, aside }: Props = $props();
</script>

<div
  class="split"
  class:single={!aside}
  style={aside ? `--aside:${asideWidth}` : undefined}
>
  <div class="main">{@render main()}</div>
  {#if aside}
    <div class="aside">{@render aside()}</div>
  {/if}
</div>

<style>
  .split {
    display: grid;
    grid-template-columns: minmax(0, 1fr) var(--aside);
    gap: var(--sp-2);
    flex: 1;
    min-height: 0;
  }

  .split.single {
    grid-template-columns: minmax(0, 1fr);
  }

  .main,
  .aside {
    display: flex;
    flex-direction: column;
    min-width: 0;
    min-height: 0;
  }

  @media (max-width: 1180px) {
    .split {
      grid-template-columns: minmax(0, 1fr);
      grid-template-rows: minmax(0, 1.4fr) minmax(0, 1fr);
    }
  }
</style>
