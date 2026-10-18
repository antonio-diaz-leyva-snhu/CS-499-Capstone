import type { APIRoute } from 'astro';
import { countUsers, createUser, DuplicateUserError } from '../../lib/users';
import { json, readBody } from '../../lib/http.ts';

export const prerender = false;

// The original let anyone register. Now it needs a signed-in admin, except for the very
// first account, which bootstraps an empty database.
export const POST: APIRoute = async ({ request, locals }) => {
  try {
    if (!locals.user && (await countUsers()) > 0) {
      return json({ message: 'Authentication required.' }, 401);
    }
    const body = await readBody(request);
    const name = typeof body?.name === 'string' ? body.name.trim() : '';
    const email = typeof body?.email === 'string' ? body.email.trim() : '';
    const password = typeof body?.password === 'string' ? body.password : '';
    if (!name || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email) || password.length < 12) {
      return json({ message: 'Name, a valid email and a password of 12+ characters are required.' }, 400);
    }
    return json(await createUser(email, name, password), 201);
  } catch (err) {
    if (err instanceof DuplicateUserError) return json({ message: err.message }, 409);
    console.error(err);
    return json({ message: 'Could not register.' }, 500);
  }
};
