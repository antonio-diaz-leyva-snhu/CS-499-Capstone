// Password hashing and signed tokens. Replaces the methods on app_api/models/users.js,
// the Passport local strategy, and the jsonwebtoken dependency.
//
// Passwords: new hashes use scrypt. Accounts created by the old Express API used
// PBKDF2 with 1,000 iterations; they still sign in, and are upgraded to scrypt on login.
// Tokens: standard HS256 JWTs, so tokens issued by the old API (same secret) still verify.
import { randomBytes, scryptSync, pbkdf2Sync, timingSafeEqual, createHmac } from 'node:crypto';

export interface PasswordRecord {
  salt: string;
  hash: string;
  algo?: 'scrypt' | 'pbkdf2-legacy';
}

export interface TokenClaims {
  _id: string;
  email: string;
  name: string;
}

export interface TokenPayload extends TokenClaims {
  iat: number;
  exp: number;
}

const SCRYPT_OPTIONS = { N: 32768, r: 8, p: 1, maxmem: 64 * 1024 * 1024 };

const scryptHex = (password: string, salt: string): string =>
  scryptSync(password, salt, 64, SCRYPT_OPTIONS).toString('hex');

function equalHex(a: string, b: string): boolean {
  const left = Buffer.from(a, 'hex');
  const right = Buffer.from(b, 'hex');
  return left.length > 0 && left.length === right.length && timingSafeEqual(left, right);
}

export function hashPassword(password: string): PasswordRecord {
  const salt = randomBytes(16).toString('hex');
  return { salt, hash: scryptHex(password, salt), algo: 'scrypt' };
}

export function verifyPassword(
  password: string,
  record: PasswordRecord,
): { ok: boolean; needsUpgrade: boolean } {
  if (!record.salt || !record.hash) return { ok: false, needsUpgrade: false };
  if (record.algo === 'scrypt') {
    return { ok: equalHex(scryptHex(password, record.salt), record.hash), needsUpgrade: false };
  }
  // Legacy record from the Express API.
  const legacy = pbkdf2Sync(password, record.salt, 1000, 64, 'sha512').toString('hex');
  const ok = equalHex(legacy, record.hash);
  return { ok, needsUpgrade: ok };
}

const encode = (value: string | Buffer): string => Buffer.from(value).toString('base64url');
const sign = (data: string, secret: string): Buffer =>
  createHmac('sha256', secret).update(data).digest();

export function signToken(
  claims: TokenClaims,
  secret: string,
  ttlSeconds = 3600,
  now = Date.now(),
): string {
  const iat = Math.floor(now / 1000);
  const header = encode(JSON.stringify({ alg: 'HS256', typ: 'JWT' }));
  const payload = encode(JSON.stringify({ ...claims, iat, exp: iat + ttlSeconds }));
  const signature = encode(sign(`${header}.${payload}`, secret));
  return `${header}.${payload}.${signature}`;
}

// Returns the payload for a valid, unexpired token, otherwise null. Never throws.
export function verifyToken(
  token: string | null | undefined,
  secret: string,
  now = Date.now(),
): TokenPayload | null {
  if (!token) return null;
  const parts = token.split('.');
  if (parts.length !== 3) return null;
  const [header, payload, signature] = parts;
  try {
    // Only HS256 is accepted, so a token cannot choose a weaker algorithm such as "none".
    const parsedHeader = JSON.parse(Buffer.from(header, 'base64url').toString('utf8'));
    if (parsedHeader?.alg !== 'HS256') return null;

    const expected = sign(`${header}.${payload}`, secret);
    const given = Buffer.from(signature, 'base64url');
    if (given.length !== expected.length || !timingSafeEqual(given, expected)) return null;

    const claims = JSON.parse(Buffer.from(payload, 'base64url').toString('utf8'));
    if (typeof claims?.exp !== 'number' || claims.exp <= Math.floor(now / 1000)) return null;
    if (typeof claims._id !== 'string' || typeof claims.email !== 'string') return null;
    return claims as TokenPayload;
  } catch {
    return null;
  }
}
