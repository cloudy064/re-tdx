/**
 * 应用级共享状态：后端健康、当前标的、最近访问列表、导航栏折叠。
 */

import { getJson } from '../api';
import type { Health } from '../types';

export interface StockRef {
  market: string;
  code: string;
  name: string;
}

const RECENT_KEY = 'tdx.recent';
const NAV_KEY = 'tdx.nav.collapsed';
const RECENT_MAX = 12;

function readRecent(): StockRef[] {
  try {
    const raw = localStorage.getItem(RECENT_KEY);
    if (!raw) return [];
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed.slice(0, RECENT_MAX) : [];
  } catch {
    return [];
  }
}

function readNavCollapsed(): boolean {
  try {
    return localStorage.getItem(NAV_KEY) === '1';
  } catch {
    return false;
  }
}

class AppState {
  health = $state<Health | null>(null);
  healthError = $state('');
  booting = $state(true);

  /** 当前工作台标的。刷新后由路由回填，这里只是默认值。 */
  stock = $state<StockRef>({ market: 'sz', code: '000001', name: '' });

  recent = $state<StockRef[]>(readRecent());
  navCollapsed = $state<boolean>(readNavCollapsed());

  async loadHealth() {
    this.booting = true;
    try {
      this.health = await getJson<Health>('/api/v1/health');
      this.healthError = '';
    } catch (err) {
      this.health = null;
      this.healthError = err instanceof Error ? err.message : '无法连接后端服务';
    } finally {
      this.booting = false;
    }
  }

  setStock(next: StockRef) {
    this.stock = next;
    if (next.code) this.pushRecent(next);
  }

  /** 只在拿到真实名称后才写入最近列表，避免存一堆没有名字的代码。 */
  pushRecent(ref: StockRef) {
    if (!ref.name) return;
    const rest = this.recent.filter(
      (item) => !(item.market === ref.market && item.code === ref.code)
    );
    this.recent = [ref, ...rest].slice(0, RECENT_MAX);
    try {
      localStorage.setItem(RECENT_KEY, JSON.stringify(this.recent));
    } catch {
      /* 持久化失败不影响本次会话 */
    }
  }

  toggleNav() {
    this.navCollapsed = !this.navCollapsed;
    try {
      localStorage.setItem(NAV_KEY, this.navCollapsed ? '1' : '0');
    } catch {
      /* 同上 */
    }
  }
}

export const app = new AppState();
