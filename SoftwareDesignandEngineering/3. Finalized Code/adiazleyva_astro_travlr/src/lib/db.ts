// Replaces app_api/models/db.js. Astro serves many requests from one process, so the
// connection is opened once and reused. The old nodemon/SIGINT shutdown hooks are not
// needed: the process owns the connection and the driver closes it on exit.
import mongoose from 'mongoose';

const host = import.meta.env?.DB_HOST ?? process.env.DB_HOST ?? '127.0.0.1';
const dbURI = `mongodb://${host}/travlr`;

type Cache = { conn: typeof mongoose | null; promise: Promise<typeof mongoose> | null };
const g = globalThis as typeof globalThis & { __travlrMongoose?: Cache };
const cache: Cache = (g.__travlrMongoose ??= { conn: null, promise: null });

export async function connectDB(): Promise<typeof mongoose> {
  if (cache.conn) return cache.conn;
  cache.promise ??= mongoose.connect(dbURI, { serverSelectionTimeoutMS: 5000 });
  try {
    cache.conn = await cache.promise;
  } catch (err) {
    cache.promise = null; // allow a retry on the next request
    throw err;
  }
  return cache.conn;
}
