import { connectDB } from './db';
import UserModel from '../models/User';
import { hashPassword, type PasswordRecord } from './auth.ts';

export interface StoredUser extends PasswordRecord {
  _id: string;
  email: string;
  name: string;
}

export class DuplicateUserError extends Error {}

const toUser = (d: any): StoredUser => ({
  _id: String(d._id),
  email: d.email,
  name: d.name,
  salt: d.salt,
  hash: d.hash,
  algo: d.algo === 'scrypt' ? 'scrypt' : 'pbkdf2-legacy',
});

// Case-insensitive match, so "Admin@x.com" finds accounts stored in any case.
export async function findUserByEmail(email: string): Promise<StoredUser | null> {
  await connectDB();
  const doc = await UserModel.findOne({ email: String(email).trim() })
    .collation({ locale: 'en', strength: 2 })
    .lean()
    .exec();
  return doc ? toUser(doc) : null;
}

export async function countUsers(): Promise<number> {
  await connectDB();
  return UserModel.estimatedDocumentCount().exec();
}

export async function createUser(email: string, name: string, password: string) {
  await connectDB();
  const { salt, hash, algo } = hashPassword(password);
  try {
    const doc = await UserModel.create({ email, name, salt, hash, algo });
    return { _id: String(doc._id), name: doc.name, email: doc.email };
  } catch (err: any) {
    if (err?.code === 11000) throw new DuplicateUserError('That email is already registered.');
    throw err;
  }
}

// Re-hash a legacy PBKDF2 password with scrypt after a successful sign-in.
export async function upgradePassword(id: string, password: string): Promise<void> {
  await connectDB();
  const { salt, hash, algo } = hashPassword(password);
  await UserModel.updateOne({ _id: id }, { salt, hash, algo }).exec();
}
