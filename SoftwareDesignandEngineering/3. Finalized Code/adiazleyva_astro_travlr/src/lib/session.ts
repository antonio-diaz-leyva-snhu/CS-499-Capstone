// Sign-in logic shared by POST /api/login and the /admin/login page.
// Replaces the Passport local strategy plus the login controller.
import { findUserByEmail, upgradePassword } from './users';
import { hashPassword, verifyPassword, signToken } from './auth.ts';
import { createLimiter } from './ratelimit.ts';
import { getJwtSecret, TOKEN_TTL_SECONDS } from './config.ts';

const limiter = createLimiter(5, 15 * 60 * 1000);
// Hashed once so an unknown email costs the same time as a wrong password.
const DUMMY = hashPassword('travlr-dummy-password');

export type LoginResult =
  | { ok: true; token: string }
  | { ok: false; status: 400 | 401 | 429 | 500; message: string };

export async function attemptLogin(
  email: unknown,
  password: unknown,
  clientAddress: string,
): Promise<LoginResult> {
  if (typeof email !== 'string' || typeof password !== 'string' || !email || !password) {
    return { ok: false, status: 400, message: 'Email and password are required.' };
  }
  const key = `${clientAddress}|${email.trim().toLowerCase()}`;
  const gate = limiter.check(key);
  if (!gate.allowed) {
    return {
      ok: false,
      status: 429,
      message: `Too many attempts. Try again in ${Math.ceil(gate.retryAfterSeconds / 60)} minute(s).`,
    };
  }

  try {
    const user = await findUserByEmail(email);
    const result = verifyPassword(password, user ?? DUMMY);
    if (!user || !result.ok) {
      limiter.fail(key);
      return { ok: false, status: 401, message: 'Incorrect email or password.' };
    }
    limiter.reset(key);
    if (result.needsUpgrade) await upgradePassword(user._id, password);
    const token = signToken(
      { _id: user._id, email: user.email, name: user.name },
      getJwtSecret(),
      TOKEN_TTL_SECONDS,
    );
    return { ok: true, token };
  } catch (err) {
    console.error('login failed', err);
    return { ok: false, status: 500, message: 'Sign-in is unavailable right now.' };
  }
}
