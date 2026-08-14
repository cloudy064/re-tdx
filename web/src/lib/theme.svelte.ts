/**
 * 主题状态。深色为默认；选择持久化到 localStorage，并由 index.html 的引导脚本
 * 在首帧前读取，避免闪白。
 */

export type Theme = 'dark' | 'light';

const STORAGE_KEY = 'tdx.theme';

function initial(): Theme {
  if (typeof document === 'undefined') return 'dark';
  return document.documentElement.dataset.theme === 'light' ? 'light' : 'dark';
}

class ThemeState {
  current = $state<Theme>(initial());

  set(next: Theme) {
    this.current = next;
    document.documentElement.dataset.theme = next;
    document.documentElement.style.colorScheme = next;
    try {
      localStorage.setItem(STORAGE_KEY, next);
    } catch {
      /* 隐私模式下 localStorage 可能不可写，仅放弃持久化 */
    }
  }

  toggle() {
    this.set(this.current === 'dark' ? 'light' : 'dark');
  }
}

export const theme = new ThemeState();
