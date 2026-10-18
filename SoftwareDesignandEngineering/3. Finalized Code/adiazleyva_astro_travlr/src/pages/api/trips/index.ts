import type { APIRoute } from 'astro';
import { getTrips, createTrip, DuplicateTripError } from '../../../lib/trips';
import { validateTrip } from '../../../lib/validate.ts';
import { json, readBody } from '../../../lib/http.ts';

export const prerender = false;

export const GET: APIRoute = async () => {
  try {
    return json(await getTrips());
  } catch (err) {
    console.error(err);
    return json({ message: 'Could not load trips.' }, 500);
  }
};

// Admin only: the middleware has already rejected unauthenticated writes.
export const POST: APIRoute = async ({ request }) => {
  const body = await readBody(request);
  if (!body) return json({ message: 'Send a JSON or form body.' }, 400);
  const result = validateTrip(body);
  if (!result.ok) return json({ message: 'Invalid trip.', errors: result.errors }, 400);
  try {
    return json(await createTrip(result.value), 201);
  } catch (err) {
    if (err instanceof DuplicateTripError) return json({ message: err.message }, 409);
    console.error(err);
    return json({ message: 'Could not save the trip.' }, 500);
  }
};
