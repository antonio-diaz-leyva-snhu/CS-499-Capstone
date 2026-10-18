/// <reference path="../.astro/types.d.ts" />

declare namespace App {
  interface Locals {
    /** The signed-in administrator, set by src/middleware.ts (null when signed out). */
    user: import('./lib/auth.ts').TokenPayload | null;
  }
}
