// ESLint flat config for the frontend (TypeScript + React).
// Opt-in: `npm run lint`. The build does not run ESLint.
import js from '@eslint/js';
import globals from 'globals';
import tseslint from 'typescript-eslint';
import reactHooks from 'eslint-plugin-react-hooks';
import reactRefresh from 'eslint-plugin-react-refresh';
import prettier from 'eslint-config-prettier';

export default tseslint.config(
  { ignores: ['dist', 'public', 'node_modules'] },
  {
    files: ['**/*.{ts,tsx,mts,mjs}'],
    extends: [js.configs.recommended, ...tseslint.configs.recommended],
    languageOptions: {
      ecmaVersion: 2022,
      globals: { ...globals.browser, ...globals.node },
    },
    plugins: { 'react-hooks': reactHooks, 'react-refresh': reactRefresh },
    linterOptions: { reportUnusedDisableDirectives: false },
    rules: {
      ...reactHooks.configs.recommended.rules,
      // Pragmatic levels for a large, hand-written codebase: surface issues as
      // warnings without failing the run or demanding a mass rewrite. Tighten
      // incrementally.
      '@typescript-eslint/no-explicit-any': 'off',
      '@typescript-eslint/no-unused-vars': ['warn', { argsIgnorePattern: '^_', varsIgnorePattern: '^_' }],
      '@typescript-eslint/no-unused-expressions': 'warn',
      'no-empty': ['warn', { allowEmptyCatch: true }],
      'no-constant-condition': ['warn', { checkLoops: false }],
      'no-control-regex': 'off',
      'prefer-const': 'warn',
    },
  },
  prettier,
);
