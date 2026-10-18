import type { APIRoute } from 'astro';
import { getTripByCode, updateTrip, deleteTrip } from '../../../lib/trips';
import { validateTrip } from '../../../lib/validate.ts';
import { json, readBody } from '../../../lib/http.ts';

export const prerender = false;

// Returns one trip object (the Express version returned a one-item array).
export const GET: APIRoute = async ({ params }) => {
  try {
    const trip = await getTripByCode(params.code ?? '');
    return trip ? json(trip) : json({ message: 'Trip not found.' }, 404);
  } catch (err) {
    console.error(err);
    return json({ message: 'Could not load the trip.' }, 500);
  }
};

export const PUT: APIRoute = async ({ params, request }) => {
  const body = await readBody(request);
  if (!body) return json({ message: 'Send a JSON or form body.' }, 400);
  // The code in the URL is the identity; a different code in the body is ignored.
  const result = validateTrip({ ...body, code: params.code });
  if (!result.ok) return json({ message: 'Invalid trip.', errors: result.errors }, 400);
  const { code: _code, ...fields } = result.value;
  try {
    const trip = await updateTrip(params.code ?? '', fields);
    return trip ? json(trip) : json({ message: 'Trip not found.' }, 404);
  } catch (err) {
    console.error(err);
    return json({ message: 'Could not update the trip.' }, 500);
  }
};

export const DELETE: APIRoute = async ({ params }) => {
  try {
    const removed = await deleteTrip(params.code ?? '');
    return removed ? json({ message: 'Trip deleted.' }) : json({ message: 'Trip not found.' }, 404);
  } catch (err) {
    console.error(err);
    return json({ message: 'Could not delete the trip.' }, 500);
  }
};
