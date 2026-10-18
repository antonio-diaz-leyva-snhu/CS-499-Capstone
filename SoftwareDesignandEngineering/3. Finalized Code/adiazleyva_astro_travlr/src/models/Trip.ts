// Ported from app_api/models/travlr.js. The original misspelled `required` as `requried`
// on `start` and `resort`, so those rules never applied; they are enforced here.
import mongoose, { Schema, type Model, type InferSchemaType } from 'mongoose';

const tripSchema = new Schema({
  code: { type: String, required: true, index: true },
  name: { type: String, required: true, index: true },
  length: { type: String, required: true },
  start: { type: Date, required: true },
  resort: { type: String, required: true },
  perPerson: { type: String, required: true },
  image: { type: String, required: true },
  description: { type: String, required: true },
});

export type TripRecord = InferSchemaType<typeof tripSchema>;

// Reuse the compiled model if dev hot-reload evaluates this module again.
const TripModel: Model<TripRecord> =
  (mongoose.models.trips as Model<TripRecord> | undefined) ??
  mongoose.model<TripRecord>('trips', tripSchema);

export default TripModel;
