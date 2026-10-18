import type { APIRoute } from 'astro';
import { attemptLogin } from '../../lib/session';
import { COOKIE_NAME, cookieOptions } from '../../lib/config.ts';
import { json, readBody } from '../../lib/http.ts';

export const prerender = false;

export const POST: APIRoute = async ({ request, cookies, clientAddress }) => {
  const body = await readBody(request);
  const result = await attemptLogin(body?.email, body?.password, clientAddress);
  if (!result.ok) return json({ message: result.message }, result.status);
  cookies.set(COOKIE_NAME, result.token, cookieOptions());
  // The token is also returned so non-browser clients can send it as a Bearer header.
  return json({ token: result.token });
};
