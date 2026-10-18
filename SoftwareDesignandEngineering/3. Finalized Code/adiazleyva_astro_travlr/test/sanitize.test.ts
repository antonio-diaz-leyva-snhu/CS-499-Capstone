import { test } from 'node:test';
import assert from 'node:assert/strict';
import { sanitizeDescription } from '../src/lib/sanitize.ts';

test('keeps allowed formatting', () => {
  assert.equal(sanitizeDescription('<p>Hi <strong>there</strong></p>'), '<p>Hi <strong>there</strong></p>');
});

test('drops script tags and attributes', () => {
  const out = sanitizeDescription('<p onclick="x()">a</p><script>alert(1)</script><img src=x onerror=y>');
  assert.equal(out, '<p>a</p>alert(1)');
  assert.ok(!out.includes('<script'));
  assert.ok(!out.includes('onerror'));
});

test('escapes stray angle brackets in text', () => {
  assert.equal(sanitizeDescription('1 < 2'), '1 &lt; 2');
});

test('handles empty and non-string input', () => {
  assert.equal(sanitizeDescription(''), '');
  assert.equal(sanitizeDescription(undefined as unknown as string), '');
});
