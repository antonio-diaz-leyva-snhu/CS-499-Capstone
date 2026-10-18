// One guard in front of every request. Replaces authenticateJWT in app_api/routes/index.js.
// Rejections return immediately, so no handler runs for an unauthenticated write.
import { defineMiddleware } from 'astro:middleware';
import { COOKIE_NAME, getJwtSecret } from './lib/config.ts';
import { verifyToken } from './lib/auth.ts';
import { decide, needsCredentials } from './lib/guard.ts';

export const onRequest = defineMiddleware(async (context, next) => {
  context.locals.user = null;
  const { pathname } = context.url;

  // Public pages never read cookies or headers.
  if (needsCredentials(pathname)) {
    const header = context.request.headers.get('authorization') ?? '';
    const bearer = /^Bearer\s+(.+)$/i.exec(header)?.[1];
    const token = bearer ?? context.cookies.get(COOKIE_NAME)?.value;

    try {
      context.locals.user = verifyToken(token, getJwtSecret());
    } catch {
      context.locals.user = null; // misconfigured secret: fail closed
    }

    const decision = decide(context.request.method, pathname, context.locals.user !== null);
    if (decision.action === 'redirect') return context.redirect(decision.location, 303);
    if (decision.action === 'deny') {
      return new Response(JSON.stringify({ message: 'Authentication required.' }), {
        status: decision.status,
        headers: { 'Content-Type': 'application/json' },
      });
    }
  }
  return next();
});
