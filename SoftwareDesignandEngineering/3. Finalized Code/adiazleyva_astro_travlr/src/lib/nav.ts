// Single source of truth for site navigation (was duplicated in header.hbs and footer.hbs).
// Rooms, Meals, News, About and Contact are the original template pages, carried over
// as static files in /public until they are rebuilt as Astro pages.
export const navLinks = [
  { href: '/', label: 'Home' },
  { href: '/travel', label: 'Travel' },
  { href: '/rooms.html', label: 'Rooms' },
  { href: '/meals.html', label: 'Meals' },
  { href: '/news.html', label: 'News' },
  { href: '/about.html', label: 'About' },
  { href: '/contact.html', label: 'Contact' },
];

export const isCurrent = (href: string, path: string): boolean =>
  href === '/' ? path === '/' : path === href || path.startsWith(href + '/');
