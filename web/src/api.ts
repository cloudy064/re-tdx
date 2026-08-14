export class ApiError extends Error {
  constructor(
    public status: number,
    message: string
  ) {
    super(message);
  }
}

async function requestJson<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, {
    ...init,
    cache: 'no-store',
    headers: { Accept: 'application/json', ...init?.headers }
  });
  const payload = await response.json().catch(() => ({})) as Record<string, unknown>;
  if (!response.ok) {
    const message = typeof payload.message === 'string'
      ? payload.message
      : `接口返回 HTTP ${response.status}`;
    throw new ApiError(response.status, message);
  }
  return payload as T;
}

export function getJson<T>(path: string): Promise<T> {
  return requestJson<T>(path);
}

export function postActionJson<T>(path: string, action = 'jsn-download'): Promise<T> {
  return requestJson<T>(path, {
    method: 'POST',
    headers: { 'X-TDX-Action': action }
  });
}

export function postJson<T>(path: string, body: unknown, action: string): Promise<T> {
  return requestJson<T>(path, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'X-TDX-Action': action
    },
    body: JSON.stringify(body)
  });
}

export function queryString(values: Record<string, string | number>): string {
  const params = new URLSearchParams();
  for (const [key, value] of Object.entries(values)) {
    if (value !== '') params.set(key, String(value));
  }
  return params.toString();
}
