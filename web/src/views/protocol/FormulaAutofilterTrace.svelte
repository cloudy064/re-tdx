<script lang="ts">
  import { count } from '../../lib/fmt';
  import type { FormulaAutofilterDecision } from '../../types';

  interface Props {
    decisions: FormulaAutofilterDecision[];
    positionChanges: number;
  }

  const { decisions, positionChanges }: Props = $props();
  const PREVIEW_LIMIT = 100;
  let expanded = $state(false);
  const preview = $derived(decisions.slice(-PREVIEW_LIMIT));
  const rejected = $derived(decisions.filter((decision) => !decision.accepted).length);
</script>

<details class="trace" ontoggle={(event) => {
  expanded = (event.currentTarget as HTMLDetailsElement).open;
}}>
  <summary>
    <strong>配对决策轨迹</strong>
    <span>{count(decisions.length)} 次决策</span>
    <span>{count(positionChanges)} 次持仓变化</span>
    <span>{count(rejected)} 次拒绝</span>
  </summary>
  {#if expanded}
    <div class="body">
      <p>
        按 K 线、源码语句顺序记录每个原始候选的接受或拒绝原因；这里只显示最近
        {count(Math.min(decisions.length, PREVIEW_LIMIT))} 条。
      </p>
      {#if decisions.length > PREVIEW_LIMIT}
        <p>已省略更早 {count(decisions.length - PREVIEW_LIMIT)} 条，完整轨迹仍保留在响应中。</p>
      {/if}
      <pre>{JSON.stringify(preview, null, 2)}</pre>
    </div>
  {/if}
</details>

<style>
  .trace {
    border: 1px solid var(--line);
    border-radius: var(--radius-sm);
    background: var(--surface-2);
  }
  summary {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-2);
    align-items: center;
    padding: var(--sp-2) var(--sp-3);
    cursor: pointer;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }
  summary strong { color: var(--fg); }
  .body { padding: 0 var(--sp-3) var(--sp-3); }
  p { margin: var(--sp-1) 0; color: var(--fg-mute); font-size: var(--fs-micro); }
  pre {
    max-height: 28rem;
    overflow: auto;
    margin: var(--sp-2) 0 0;
    padding: var(--sp-2);
    border: 1px solid var(--line);
    border-radius: var(--radius-sm);
    background: var(--surface-1);
    color: var(--fg);
    font-size: var(--fs-micro);
  }
</style>
