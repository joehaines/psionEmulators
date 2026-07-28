import type { Config } from 'tailwindcss';

export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      colors: {
        // Semantic palette defined as RGB-channel CSS variables (see
        // src/index.css) so the whole scheme flips automatically under
        // prefers-color-scheme: dark. Light-mode values in comments.
        psion: {
          dark:      'rgb(var(--psion-dark) / <alpha-value>)',      // #ffffff — main page / dialog backgrounds
          mid:       'rgb(var(--psion-mid) / <alpha-value>)',       // #f4f4f2 — panels, sidebars, cards
          accent:    'rgb(var(--psion-accent) / <alpha-value>)',    // #5b5c5e — Psion brand gray (from logo)
          highlight: 'rgb(var(--psion-highlight) / <alpha-value>)', // #fadf3c — Psion brand yellow (from logo)
          charcoal:  'rgb(var(--psion-charcoal) / <alpha-value>)',  // #2b2b2b — primary text / dark UI chrome
        },
      },
      fontFamily: {
        mono: ['JetBrains Mono', 'Fira Code', 'Consolas', 'monospace'],
      },
      // Yellow-to-white attention pulse, used on the 5mx Pro "Insert
      // CF card containing OS" button so the user can see what they
      // need to click after the bootloader splash settles into the
      // "insert a memory disk" wait state.
      keyframes: {
        'attention-yellow': {
          '0%, 100%': { backgroundColor: 'rgb(var(--psion-highlight))' },
          '50%':       { backgroundColor: 'rgb(var(--psion-dark))' },
        },
        // Indeterminate loading bar: a 1/3-width segment sweeps through the
        // track (both endpoints are fully off-screen, so the loop cut is
        // invisible inside the overflow-hidden track).
        'progress-sweep': {
          '0%':   { transform: 'translateX(-100%)' },
          '100%': { transform: 'translateX(300%)' },
        },
      },
      animation: {
        'attention-yellow': 'attention-yellow 1.2s ease-in-out infinite',
        'progress-sweep': 'progress-sweep 1.2s ease-in-out infinite',
      },
    },
  },
  plugins: [],
} satisfies Config;
