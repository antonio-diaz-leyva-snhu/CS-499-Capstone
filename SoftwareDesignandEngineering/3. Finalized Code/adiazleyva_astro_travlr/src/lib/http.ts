// Small helpers shared by the API endpoints.
export const json = (body: unknown, status = 200, headers: Record<string, string> = {}): Response =>
  new Response(JSON.stringify(body), {
    status,
    headers: { 'Content-Type': 'application/json', ...headers },
  });

// Reads a JSON or form body into a plain object, or returns null when it cannot.
export async function readBody(request: Request): Promise<Record<string, unknown> | null> {
  const type = request.headers.get('content-type') ?? '';
  try {
    if (type.includes('application/json')) {
      const data = await request.json();
      return data && typeof data === 'object' && !Array.isArray(data) ? data : null;
    }
    if (type.includes('application/x-www-form-urlencoded') || type.includes('multipart/form-data')) {
      const form = await request.formData();
      const body: Record<string, unknown> = {};
      form.forEach((value, key) => {
        body[key] = typeof value === 'string' ? value : '';
      });
      return body;
    }
  } catch {
    return null;
  }
  return null;
}
