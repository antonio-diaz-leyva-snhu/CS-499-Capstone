// A small in-memory limiter for sign-in attempts. It resets when the server restarts
// and is per process, which suits a single-server deployment.
export interface Limiter {
  check(key: string, now?: number): { allowed: boolean; retryAfterSeconds: number };
  fail(key: string, now?: number): void;
  reset(key: string): void;
}

export function createLimiter(max: number, windowMs: number): Limiter {
  const failures = new Map<string, number[]>();

  const recent = (key: string, now: number): number[] => {
    const list = (failures.get(key) ?? []).filter((time) => now - time < windowMs);
    if (list.length > 0) failures.set(key, list);
    else failures.delete(key);
    return list;
  };

  return {
    check(key, now = Date.now()) {
      const list = recent(key, now);
      if (list.length < max) return { allowed: true, retryAfterSeconds: 0 };
      return { allowed: false, retryAfterSeconds: Math.ceil((list[0] + windowMs - now) / 1000) };
    },
    fail(key, now = Date.now()) {
      const list = recent(key, now);
      list.push(now);
      failures.set(key, list);
    },
    reset(key) {
      failures.delete(key);
    },
  };
}
