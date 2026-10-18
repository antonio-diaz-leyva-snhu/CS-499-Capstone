import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createLimiter } from '../src/lib/ratelimit.ts';

test('blocks after max failures and recovers after the window', () => {
  const l = createLimiter(3, 1000);
  for (let i = 0; i < 3; i++) {
    assert.equal(l.check('k', 0).allowed, true);
    l.fail('k', i);
  }
  const blocked = l.check('k', 10);
  assert.equal(blocked.allowed, false);
  assert.ok(blocked.retryAfterSeconds >= 1);
  assert.equal(l.check('k', 2000).allowed, true);
});

test('reset clears failures and keys are independent', () => {
  const l = createLimiter(1, 1000);
  l.fail('a', 0);
  assert.equal(l.check('a', 1).allowed, false);
  assert.equal(l.check('b', 1).allowed, true);
  l.reset('a');
  assert.equal(l.check('a', 1).allowed, true);
});
