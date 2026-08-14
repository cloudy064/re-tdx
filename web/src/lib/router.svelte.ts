/**
 * Hash 路由。
 *
 * 重构前个股详情是无路由的 slide-over 抽屉：刷新丢状态、浏览器后退直接退出应用、
 * 无法把某只股票的某个页签发给别人。这里用 hash 记录完整位置，零依赖。
 *
 * 形式：`#/stock/sz/000001/funds`、`#/data/ownership`、`#/protocol/tqlex`
 */

const ROUTE_ROOTS = new Set([
  'stock', 'ranking', 'sectors', 'industry', 'data', 'protocol', 'system'
]);

function parseHash(): string[] {
  const raw = typeof location === 'undefined' ? '' : location.hash.replace(/^#/, '');
  return raw.split('/').filter(Boolean);
}

function parsePathname(): string[] {
  if (typeof location === 'undefined') return [];
  const segments = location.pathname.split('/').filter(Boolean);
  return segments.length && ROUTE_ROOTS.has(segments[0]) ? segments : [];
}

function initialSegments(): string[] {
  const hash = parseHash();
  return hash.length ? hash : parsePathname();
}

class Router {
  segments = $state<string[]>(initialSegments());

  /** 归一化路径，如 `/data/ownership`；空 hash 落到 `/stock`。 */
  get path(): string {
    return this.segments.length ? `/${this.segments.join('/')}` : '/stock';
  }

  /** 第一段，等价于顶层区域。 */
  get root(): string {
    return this.segments[0] ?? 'stock';
  }

  segment(index: number): string {
    return this.segments[index] ?? '';
  }

  start(): () => void {
    const sync = () => {
      this.segments = parseHash();
    };
    window.addEventListener('hashchange', sync);
    if (!location.hash) {
      const direct = parsePathname();
      if (direct.length) {
        const target = `/#/${direct.join('/')}`;
        history.replaceState(null, '', target);
        this.segments = direct;
      } else {
        this.go('/stock');
      }
    }
    return () => window.removeEventListener('hashchange', sync);
  }

  /** 压入一条新历史记录（用户显式导航）。 */
  go(path: string) {
    const next = `#${path.startsWith('/') ? path : `/${path}`}`;
    if (location.hash === next) return;
    location.hash = next;
  }

  /** 原地替换，不产生历史记录（用于切页签这类不值得回退的动作）。 */
  replace(path: string) {
    const next = `#${path.startsWith('/') ? path : `/${path}`}`;
    if (location.hash === next) return;
    history.replaceState(null, '', next);
    this.segments = parseHash();
  }
}

export const router = new Router();

/** 生成个股工作台路径。 */
export function stockPath(market: string, code: string, tab?: string): string {
  const base = `/stock/${market}/${code}`;
  return tab ? `${base}/${tab}` : base;
}

/** 生成可刷新、可分享的板块浏览器精确检索路径。 */
export function sectorPath(family: string, code: string): string {
  return `/sectors/${encodeURIComponent(family)}/${encodeURIComponent(code)}`;
}
