/**
 * 异步资源容器。
 *
 * 重构前每个视图都手写一遍 `busy` / `error` / `requestId` 竞态守卫（十余份拷贝，
 * 且有几处漏掉了守卫，快速切换筛选条件会让旧响应覆盖新响应）。这里统一实现：
 * 每次 load 递增序号，只有最后一次请求的结果会被采纳。
 */

import { getJson } from '../api';

export interface LoadOptions {
  /** 静默刷新：保留旧数据与旧错误，不闪 loading。用于轮询。 */
  silent?: boolean;
}

export class Resource<T> {
  data = $state<T | null>(null);
  busy = $state(false);
  error = $state('');
  /** 是否已经完成过至少一次请求，用于区分「未加载」和「加载完但是空的」。 */
  loaded = $state(false);

  #seq = 0;

  async load(path: string, options: LoadOptions = {}): Promise<T | null> {
    return this.loadWith(() => getJson<T>(path), options);
  }

  async loadWith(loader: () => Promise<T>, options: LoadOptions = {}): Promise<T | null> {
    const ticket = ++this.#seq;
    if (!options.silent) {
      this.busy = true;
      this.error = '';
    }
    try {
      const result = await loader();
      if (ticket !== this.#seq) return null; // 已被更新的请求取代
      this.data = result;
      this.error = '';
      this.loaded = true;
      return result;
    } catch (err) {
      if (ticket !== this.#seq) return null;
      this.error = err instanceof Error ? err.message : '请求失败';
      this.loaded = true;
      return null;
    } finally {
      if (ticket === this.#seq) this.busy = false;
    }
  }

  reset() {
    this.#seq += 1; // 使在途请求作废
    this.data = null;
    this.busy = false;
    this.error = '';
    this.loaded = false;
  }
}
