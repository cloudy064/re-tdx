<script lang="ts">
  /** 极简加载指示。终端风格用细环，不用跳动的点或进度条。 */
  interface Props {
    size?: number;
  }
  const { size = 13 }: Props = $props();
</script>

<span
  class="spinner"
  style="width:{size}px;height:{size}px"
  role="status"
  aria-label="加载中"
></span>

<style>
  .spinner {
    display: inline-block;
    flex: none;
    border: 1.5px solid var(--line-strong);
    border-top-color: var(--fg-dim);
    border-radius: 50%;
    animation: spin 620ms linear infinite;
  }

  @keyframes spin {
    to {
      transform: rotate(360deg);
    }
  }
</style>
