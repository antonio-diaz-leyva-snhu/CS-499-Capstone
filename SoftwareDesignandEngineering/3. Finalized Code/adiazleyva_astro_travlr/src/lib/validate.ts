// Input checks for trips, shared by the API and the admin forms. The original API saved
// whatever it was sent; these rules run before anything reaches the database.
import { sanitizeDescription } from './sanitize.ts';

export interface TripInput {
  code: string;
  name: string;
  length: string;
  start: string; // ISO date string
  resort: string;
  perPerson: string;
  image: string;
  description: string;
}

export type ValidationResult =
  | { ok: true; value: TripInput }
  | { ok: false; errors: Record<string, string> };

const text = (raw: Record<string, unknown>, key: string): string =>
  typeof raw[key] === 'string' ? (raw[key] as string).trim() : '';

export function validateTrip(raw: Record<string, unknown>): ValidationResult {
  const errors: Record<string, string> = {};

  const code = text(raw, 'code');
  if (!/^[A-Za-z0-9_-]{3,32}$/.test(code)) {
    errors.code = 'Use 3 to 32 letters, numbers, dashes or underscores.';
  }

  const name = text(raw, 'name');
  if (name.length < 1 || name.length > 100) errors.name = 'Enter a name up to 100 characters.';

  const length = text(raw, 'length');
  if (length.length < 1 || length.length > 60) errors.length = 'Enter a length such as "4 nights / 5 days".';

  const startText = text(raw, 'start');
  const startDate = new Date(startText);
  if (!startText || Number.isNaN(startDate.getTime())) errors.start = 'Enter a valid start date.';

  const resort = text(raw, 'resort');
  if (resort.length < 1 || resort.length > 100) errors.resort = 'Enter a resort up to 100 characters.';

  const perPerson = text(raw, 'perPerson');
  if (!/^\d{1,6}(\.\d{1,2})?$/.test(perPerson)) errors.perPerson = 'Enter a price such as 799.00.';

  const image = text(raw, 'image');
  if (!/^[A-Za-z0-9_-]+\.(jpg|jpeg|png|gif|webp)$/i.test(image)) {
    errors.image = 'Enter an image file name such as reef1.jpg.';
  }

  const rawDescription = text(raw, 'description');
  const description = sanitizeDescription(rawDescription).trim();
  if (!description || rawDescription.length > 5000) {
    errors.description = 'Enter a description up to 5000 characters.';
  }

  if (Object.keys(errors).length > 0) return { ok: false, errors };
  return {
    ok: true,
    value: { code, name, length, start: startDate.toISOString(), resort, perPerson, image, description },
  };
}
