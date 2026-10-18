// Trip queries shared by pages and (in the next phase) the API endpoints, so the
// two cannot drift apart. Replaces the read side of app_api/controllers/trips.js.
import { connectDB } from './db';
import TripModel from '../models/Trip';

export interface Trip {
  _id: string;
  code: string;
  name: string;
  length: string;
  start: string; // ISO date
  resort: string;
  perPerson: string;
  image: string;
  description: string;
}

const toTrip = (d: any): Trip => ({
  _id: String(d._id),
  code: d.code,
  name: d.name,
  length: d.length,
  start: new Date(d.start).toISOString(),
  resort: d.resort,
  perPerson: d.perPerson,
  image: d.image,
  description: d.description,
});

export async function getTrips(): Promise<Trip[]> {
  await connectDB();
  const docs = await TripModel.find({}).lean().exec();
  return docs.map(toTrip);
}

// Returns a single trip or null (the Express version returned a one-item array).
export async function getTripByCode(code: string): Promise<Trip | null> {
  await connectDB();
  const doc = await TripModel.findOne({ code }).lean().exec();
  return doc ? toTrip(doc) : null;
}

// ---- Write side (replaces tripsAddTrip / tripsUpdateTrip / tripsDeleteTrip) ----
export class DuplicateTripError extends Error {}

export async function createTrip(input: Omit<Trip, '_id'>): Promise<Trip> {
  await connectDB();
  // The original allowed two trips with the same code; codes are the lookup key.
  if (await TripModel.exists({ code: input.code })) {
    throw new DuplicateTripError(`A trip with code "${input.code}" already exists.`);
  }
  const doc = await TripModel.create(input);
  return toTrip(doc.toObject());
}

export async function updateTrip(code: string, input: Omit<Trip, '_id' | 'code'>): Promise<Trip | null> {
  await connectDB();
  const doc = await TripModel.findOneAndUpdate({ code }, input, {
    new: true,
    runValidators: true,
  })
    .lean()
    .exec();
  return doc ? toTrip(doc) : null;
}

export async function deleteTrip(code: string): Promise<boolean> {
  await connectDB();
  const result = await TripModel.deleteOne({ code }).exec();
  return result.deletedCount > 0;
}
