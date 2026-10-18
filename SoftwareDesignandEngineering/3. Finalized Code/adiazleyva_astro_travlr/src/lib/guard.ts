// The access rules, kept free of Astro so they can be unit tested.
// Replaces the per-route authenticateJWT call in app_api/routes/index.js.
// Unlike the original, a failed check ends the request: nothing runs after a rejection.
const SAFE_METHODS = new Set(['GET', 'HEAD', 'OPTIONS']);

export type GuardDecision =
  | { action: 'allow' }
  | { action: 'redirect'; location: string }
  | { action: 'deny'; status: 401 };

export function normalizePath(pathname: string): string {
  let path = pathname;
  try {
    path = decodeURIComponent(path);
  } catch {
    // keep the raw path
  }
  path = path.toLowerCase().replace(/\/{2,}/g, '/');
  return path.length > 1 ? path.replace(/\/+$/, '') : path;
}

const isAdmin = (path: string) => path === '/admin' || path.startsWith('/admin/');
const isLogin = (path: string) => path === '/admin/login';
const isTripsApi = (path: string) => path === '/api/trips' || path.startsWith('/api/trips/');

// Only these paths need the request's credentials read; public pages never touch cookies.
export function needsCredentials(pathname: string): boolean {
  const path = normalizePath(pathname);
  return isAdmin(path) || path.startsWith('/api/');
}

export function decide(method: string, pathname: string, authenticated: boolean): GuardDecision {
  if (authenticated) return { action: 'allow' };
  const path = normalizePath(pathname);
  const verb = method.toUpperCase();

  if (isAdmin(path) && !isLogin(path)) {
    return SAFE_METHODS.has(verb)
      ? { action: 'redirect', location: '/admin/login' }
      : { action: 'deny', status: 401 };
  }
  if (isTripsApi(path) && !SAFE_METHODS.has(verb)) {
    return { action: 'deny', status: 401 };
  }
  return { action: 'allow' };
}
