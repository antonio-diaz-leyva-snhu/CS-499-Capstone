// Trip descriptions are stored as HTML and rendered unescaped. This keeps a short
// allow-list of formatting tags and drops everything else, including every attribute,
// so a description can never carry a script, event handler or link.
const ALLOWED_TAGS = new Set(['p', 'strong', 'b', 'em', 'i', 'u', 'ul', 'ol', 'li', 'br', 'h3', 'h4']);

const escapeText = (text: string): string => text.replace(/</g, '&lt;').replace(/>/g, '&gt;');

export function sanitizeDescription(html: string): string {
  // Splitting on a capturing group puts tags at the odd positions and text at the even ones.
  return String(html ?? '')
    .split(/(<[^>]*>)/)
    .map((piece, index) => {
      if (index % 2 === 0) return escapeText(piece);
      const tag = /^<\s*(\/?)\s*([a-zA-Z][a-zA-Z0-9]*)/.exec(piece);
      if (!tag) return '';
      const name = tag[2].toLowerCase();
      if (!ALLOWED_TAGS.has(name)) return '';
      return tag[1] ? `</${name}>` : `<${name}>`;
    })
    .join('');
}
