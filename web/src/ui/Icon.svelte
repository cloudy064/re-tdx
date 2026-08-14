<script lang="ts" module>
  /**
   * 图标集。
   *
   * 重构前导航和数据中心用 emoji 当图标（📈🧩🏛️🔬⚙️💼🔓💰🏦🐉🌐🔭）：
   * 跨平台字形不一致、无法跟随主题着色、语义模糊。这里换成手写 SVG，
   * 统一 24 视口 / 1.5 线宽 / currentColor，随文字色走。
   */
  export type IconName =
    | 'market' | 'sectors' | 'data' | 'protocol' | 'system'
    | 'search' | 'close' | 'refresh' | 'filter' | 'download' | 'copy'
    | 'chevron-right' | 'chevron-down' | 'chevron-left' | 'panel'
    | 'sun' | 'moon' | 'check' | 'alert' | 'info' | 'clock'
    | 'arrow-up' | 'arrow-down' | 'external' | 'star' | 'inbox';

  const PATHS: Record<IconName, string> = {
    // 导航
    market: 'M3 3v18h18M7 15l4-5 3 3 5-7',
    sectors: 'M3 3h7v7H3zM14 3h7v7h-7zM3 14h7v7H3zM14 14h7v7h-7z',
    data: 'M12 3c4.4 0 8 1.3 8 3s-3.6 3-8 3-8-1.3-8-3 3.6-3 8-3zM4 6v12c0 1.7 3.6 3 8 3s8-1.3 8-3V6M4 12c0 1.7 3.6 3 8 3s8-1.3 8-3',
    protocol: 'M4 5h16M4 5v14h16V5M8 10l2 2-2 2M13 14h4',
    system: 'M4 7h10M18 7h2M4 17h4M12 17h8M16 4v6M8 14v6',
    // 操作
    search: 'M11 4a7 7 0 100 14 7 7 0 000-14zM20 20l-4.2-4.2',
    close: 'M6 6l12 12M18 6L6 18',
    refresh: 'M20 12a8 8 0 11-2.6-5.9M20 4v5h-5',
    filter: 'M4 5h16l-6 7v6l-4 2v-8z',
    download: 'M12 4v11M8 11l4 4 4-4M5 20h14',
    copy: 'M9 9h10v10H9zM5 15V5h10',
    'chevron-right': 'M9 5l7 7-7 7',
    'chevron-down': 'M5 9l7 7 7-7',
    'chevron-left': 'M15 5l-7 7 7 7',
    panel: 'M4 4h16v16H4zM10 4v16',
    sun: 'M12 7a5 5 0 100 10 5 5 0 000-10zM12 2v2M12 20v2M4.2 4.2l1.5 1.5M18.3 18.3l1.5 1.5M2 12h2M20 12h2M4.2 19.8l1.5-1.5M18.3 5.7l1.5-1.5',
    moon: 'M20 14.5A8.5 8.5 0 019.5 4a8.5 8.5 0 1010.5 10.5z',
    check: 'M5 13l4 4L19 7',
    alert: 'M12 4l9 16H3zM12 10v4M12 17.5v.5',
    info: 'M12 3a9 9 0 100 18 9 9 0 000-18zM12 11v6M12 7.5v.5',
    clock: 'M12 3a9 9 0 100 18 9 9 0 000-18zM12 7v5l3.5 2',
    'arrow-up': 'M12 19V5M6 11l6-6 6 6',
    'arrow-down': 'M12 5v14M6 13l6 6 6-6',
    external: 'M14 4h6v6M20 4l-8 8M18 14v5a1 1 0 01-1 1H5a1 1 0 01-1-1V7a1 1 0 011-1h5',
    star: 'M12 3.5l2.6 5.4 5.9.8-4.3 4.1 1.1 5.9-5.3-2.9-5.3 2.9 1.1-5.9L3.5 9.7l5.9-.8z',
    inbox: 'M4 13h5l1 3h4l1-3h5M4 13l2.5-8h11L20 13v6H4z'
  };
</script>

<script lang="ts">
  interface Props {
    name: IconName;
    size?: number;
    /** 线宽。密集表格里可降到 1.25。 */
    width?: number;
  }

  const { name, size = 15, width = 1.5 }: Props = $props();
</script>

<svg
  class="icon"
  viewBox="0 0 24 24"
  {...{ width: size, height: size }}
  fill="none"
  stroke="currentColor"
  stroke-width={width}
  stroke-linecap="round"
  stroke-linejoin="round"
  aria-hidden="true"
  focusable="false"
><path d={PATHS[name]} /></svg>

<style>
  .icon {
    display: block;
    flex: none;
  }
</style>
