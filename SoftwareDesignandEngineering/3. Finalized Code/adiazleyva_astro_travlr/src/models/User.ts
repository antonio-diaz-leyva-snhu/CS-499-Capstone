// Ported from app_api/models/users.js. Hashing moved to src/lib/auth.ts so the model
// is only a schema. `algo` is absent on accounts created by the Express API.
import mongoose, { Schema, type Model, type InferSchemaType } from 'mongoose';

const userSchema = new Schema({
  email: { type: String, required: true, unique: true, lowercase: true, trim: true },
  name: { type: String, required: true },
  hash: { type: String, required: true },
  salt: { type: String, required: true },
  algo: { type: String, enum: ['scrypt', 'pbkdf2-legacy'] },
});

export type UserRecord = InferSchemaType<typeof userSchema>;

const UserModel: Model<UserRecord> =
  (mongoose.models.users as Model<UserRecord> | undefined) ??
  mongoose.model<UserRecord>('users', userSchema);

export default UserModel;
