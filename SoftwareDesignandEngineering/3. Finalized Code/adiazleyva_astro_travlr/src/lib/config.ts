// Runtime settings. Secrets come from the environment (.env), never from code.
export const COOKIE_NAME = 'travlr-token';
export const TOKEN_TTL_SECONDS = 60 * 60; // 1 hour, same as the original API

export function getJwtSecret(): string {
  const secret = import.meta.env?.JWT_SECRET ?? process.env.JWT_SECRET;
  if (!secret || String(secret).length < 16) {
    throw new Error('JWT_SECRET is missing or shorter than 16 characters. Set it in .env.');
  }
  return String(secret);
}

// The token lives in an httpOnly cookie, so page scripts can never read it
// (the Angular admin kept it in localStorage). `secure` is on for production builds.
export function cookieOptions() {
  return {
    httpOnly: true,
    sameSite: 'strict' as const,
    secure: Boolean(import.meta.env?.PROD),
    path: '/',
    maxAge: TOKEN_TTL_SECONDS,
  };
}
