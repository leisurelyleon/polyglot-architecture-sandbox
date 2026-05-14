// 1. A deeply nested configuration object (read-only)
const designSystem = {
  colors: {
    primary: { 500: '#3b82f6', 600: '#2563eb' },
    surface: { light: '#ffffff', dark: '#1f2937' }
  },
  spacing: {
    padding: { sm: '4px', md: '8px', lg: '16px' },
    margin: { x: { auto: 'auto', 0: '0px' } }
  },
  typography: {
    fontFamily: { sans: 'Inter, sans-serif', mono: 'Fira Code, monospace' }
  }
} as const;

// 2. The Recursive Type Magic
// This extracts all valid paths as a string union: "colors.primary.500" | "spacing.margin.x.auto" | etc.
type DeepPaths<T> = T extends object
  ? {
      [K in keyof T]: K extends string | number
        ? T[K] extends object
          ? `${K}.${DeepPaths<T[K]>}` // Recursively append the next level
          : `${K}`                   // Base case: we hit a primitive value
        : never;
    }[keyof T]
  : never;

// 3. Extract the exact value type at a specific path
type PathValue<T, P extends string> = P extends `${infer Key}.${infer Rest}`
  ? Key extends keyof T
    ? PathValue<T[Key], Rest>
    : never
  : P extends keyof T
  ? T[P]
  : never;

// 4. The utility function enforcing strict typing
type ValidThemePaths = DeepPaths<typeof designSystem>;

function getThemeValue<Path extends ValidThemePaths>(
  path: Path
): PathValue<typeof designSystem, Path> {
  // Runtime implementation using reduce to navigate the object via the dot-notation path
  return path.split('.').reduce((acc: any, key) => acc[key], designSystem);
}

// 5. Usage (Try typing this out—if you misspell 'primary', the TS compiler screams!)
const validBlue = getThemeValue('colors.primary.500'); // Returns '#3b82f6'
const validMono = getThemeValue('typography.fontFamily.mono'); // Returns 'Fira Code, monospace'
// const error = getThemeValue('colors.fake.100'); // COMPILE ERROR!
