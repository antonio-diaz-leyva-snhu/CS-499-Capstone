// Replaces app_api/models/seed.js: wipes the trips collection and reloads data/trips.json.
// Run with: npm run seed   (reads DB_HOST from .env)
import mongoose from 'mongoose';
import { readFile } from 'node:fs/promises';

const host = process.env.DB_HOST ?? '127.0.0.1';
const trips = JSON.parse(await readFile(new URL('../data/trips.json', import.meta.url), 'utf8'));

await mongoose.connect(`mongodb://${host}/travlr`, { serverSelectionTimeoutMS: 5000 });
const col = mongoose.connection.collection('trips');
await col.deleteMany({});
await col.insertMany(trips.map((t) => ({ ...t, start: new Date(t.start) })));
console.log(`Seeded ${trips.length} trips into travlr.trips on ${host}`);
await mongoose.connection.close();
