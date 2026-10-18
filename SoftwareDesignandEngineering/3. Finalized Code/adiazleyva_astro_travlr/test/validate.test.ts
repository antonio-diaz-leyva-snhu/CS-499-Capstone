import { test } from 'node:test';
import assert from 'node:assert/strict';
import { validateTrip } from '../src/lib/validate.ts';

const good = {
  code: 'GALR210314',
  name: 'Gale Reef',
  length: '4 nights / 5 days',
  start: '2026-03-14',
  resort: 'Emerald Bay',
  perPerson: '799.00',
  image: 'reef1.jpg',
  description: '<p>Nice <script>x</script></p>',
};

test('accepts a valid trip and sanitizes the description', () => {
  const r = validateTrip(good);
  assert.equal(r.ok, true);
  if (r.ok) {
    assert.equal(r.value.description, '<p>Nice x</p>');
    assert.match(r.value.start, /^2026-03-14T/);
  }
});

test('rejects bad fields with a message per field', () => {
  const r = validateTrip({ ...good, code: 'a b', start: 'nope', perPerson: '-5', image: '../x.jpg', name: '' });
  assert.equal(r.ok, false);
  if (!r.ok) assert.deepEqual(Object.keys(r.errors).sort(), ['code', 'image', 'name', 'perPerson', 'start']);
});

test('rejects non-string values and oversized descriptions', () => {
  assert.equal(validateTrip({ ...good, name: 42 }).ok, false);
  assert.equal(validateTrip({ ...good, description: 'a'.repeat(5001) }).ok, false);
  assert.equal(validateTrip({}).ok, false);
});
