import type { Config } from 'tailwindcss';

export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      colors: {
        psion: {
          dark:      '#ffffff',   // white — main page / dialog backgrounds
          mid:       '#f4f4f2',   // warm light gray — panels, sidebars, cards
          accent:    '#5b5c5e',   // Psion brand gray (from logo)
          highlight: '#fadf3c',   // Psion brand yellow (from logo)
          charcoal:  '#2b2b2b',   // near-black — primary text / dark UI chrome
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
          '0%, 100%': { backgroundColor: '#fadf3c' /* psion-highlight */ },
          '50%':       { backgroundColor: '#ffffff' /* psion-dark    */ },
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
