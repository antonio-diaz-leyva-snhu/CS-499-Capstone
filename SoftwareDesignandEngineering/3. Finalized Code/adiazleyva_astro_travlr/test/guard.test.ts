import { test } from 'node:test';
import assert from 'node:assert/strict';
import { decide, normalizePath, needsCredentials } from '../src/lib/guard.ts';

test('public pages and reads are allowed without a token', () => {
  assert.equal(decide('GET', '/', false).action, 'allow');
  assert.equal(decide('GET', '/travel', false).action, 'allow');
  assert.equal(decide('GET', '/api/trips', false).action, 'allow');
  assert.equal(decide('GET', '/api/trips/GALR210314', false).action, 'allow');
  assert.equal(decide('POST', '/api/login', false).action, 'allow');
});

test('trip writes need a token', () => {
  for (const method of ['POST', 'PUT', 'DELETE', 'PATCH']) {
    assert.deepEqual(decide(method, '/api/trips/ABC', false), { action: 'deny', status: 401 });
  }
  assert.deepEqual(decide('POST', '/api/trips', false), { action: 'deny', status: 401 });
  assert.equal(decide('PUT', '/api/trips/ABC', true).action, 'allow');
});

test('admin pages redirect to login when signed out', () => {
  assert.deepEqual(decide('GET', '/admin', false), { action: 'redirect', location: '/admin/login' });
  assert.equal(decide('GET', '/admin/trips/new', false).action, 'redirect');
  assert.equal(decide('GET', '/admin/login', false).action, 'allow');
  assert.equal(decide('POST', '/admin/login', false).action, 'allow');
  assert.deepEqual(decide('POST', '/admin', false), { action: 'deny', status: 401 });
});

test('path tricks do not bypass the guard', () => {
  for (const p of ['/ADMIN', '//admin', '/admin/', '/%61dmin', '/Admin/Trips/New', '/admin//trips']) {
    assert.notEqual(decide('GET', p, false).action, 'allow', p);
  }
  assert.equal(decide('DELETE', '/API/Trips/ABC', false).action, 'deny');
  assert.equal(decide('DELETE', '/api//trips/ABC/', false).action, 'deny');
});

test('signed-in users are allowed everywhere', () => {
  assert.equal(decide('GET', '/admin', true).action, 'allow');
  assert.equal(decide('DELETE', '/api/trips/X', true).action, 'allow');
});

test('normalizePath and needsCredentials', () => {
  assert.equal(normalizePath('//Admin/'), '/admin');
  assert.equal(normalizePath('/'), '/');
  assert.equal(needsCredentials('/travel'), false);
  assert.equal(needsCredentials('/admin/x'), true);
  assert.equal(needsCredentials('/api/trips'), true);
});
