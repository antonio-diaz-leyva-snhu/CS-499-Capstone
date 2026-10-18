import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createHmac, pbkdf2Sync } from 'node:crypto';
import { hashPassword, verifyPassword, signToken, verifyToken } from '../src/lib/auth.ts';

const SECRET = 'test-secret-of-sufficient-length';
const claims = { _id: 'u1', email: 'a@b.co', name: 'Ann' };

test('scrypt hash verifies and rejects wrong password', () => {
  const rec = hashPassword('correct horse battery');
  assert.equal(rec.algo, 'scrypt');
  assert.deepEqual(verifyPassword('correct horse battery', rec), { ok: true, needsUpgrade: false });
  assert.equal(verifyPassword('wrong', rec).ok, false);
});

test('legacy PBKDF2 record verifies and is flagged for upgrade', () => {
  const salt = 'abcdef0123456789';
  const hash = pbkdf2Sync('oldpass', salt, 1000, 64, 'sha512').toString('hex');
  assert.deepEqual(verifyPassword('oldpass', { salt, hash }), { ok: true, needsUpgrade: true });
  assert.equal(verifyPassword('nope', { salt, hash }).ok, false);
});

test('missing salt or hash never verifies', () => {
  assert.equal(verifyPassword('x', { salt: '', hash: '' }).ok, false);
});

test('valid token round-trips', () => {
  const token = signToken(claims, SECRET, 3600, 1_000_000);
  const payload = verifyToken(token, SECRET, 1_000_000 + 1000);
  assert.equal(payload?.email, 'a@b.co');
});

test('expired token is rejected', () => {
  const token = signToken(claims, SECRET, 60, 1_000_000);
  assert.equal(verifyToken(token, SECRET, 1_000_000 + 61_000), null);
});

test('wrong secret, tampered payload and junk are rejected', () => {
  const token = signToken(claims, SECRET);
  assert.equal(verifyToken(token, 'another-secret-value-here'), null);
  const [h, , s] = token.split('.');
  const forged = Buffer.from(JSON.stringify({ ...claims, exp: 9999999999 })).toString('base64url');
  assert.equal(verifyToken(`${h}.${forged}.${s}`, SECRET), null);
  assert.equal(verifyToken('not.a.token', SECRET), null);
  assert.equal(verifyToken('', SECRET), null);
  assert.equal(verifyToken(null, SECRET), null);
});

test('alg "none" tokens are rejected', () => {
  const h = Buffer.from(JSON.stringify({ alg: 'none', typ: 'JWT' })).toString('base64url');
  const p = Buffer.from(JSON.stringify({ ...claims, iat: 1, exp: 9999999999 })).toString('base64url');
  assert.equal(verifyToken(`${h}.${p}.`, SECRET), null);
});

test('tokens from the old jsonwebtoken-style HS256 format verify', () => {
  const h = Buffer.from(JSON.stringify({ alg: 'HS256', typ: 'JWT' })).toString('base64url');
  const p = Buffer.from(JSON.stringify({ ...claims, exp: 4102444800 })).toString('base64url');
  const s = createHmac('sha256', SECRET).update(`${h}.${p}`).digest('base64url');
  assert.equal(verifyToken(`${h}.${p}.${s}`, SECRET)?._id, 'u1');
});
