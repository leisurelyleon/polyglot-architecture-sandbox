// 1. The Server-Side Handlers (Simulated)
type Context = { userId: string; role: 'admin' | 'user' };

const backendProcedures = {
  getUser: async (ctx: Context, id: string) => {
    return { id, name: 'Alice', active: true };
  },
  updateSettings: async (ctx: Context, id: string, payload: { theme: string }) => {
    return { success: true, newTheme: payload.theme };
  }
};

// 2. The Type Stripper
// We want the frontend to call these WITHOUT providing the 'Context' argument
type OmitFirstArg<F> = F extends (ctx: any, ...args: infer Args) => infer Return
  ? (...args: Args) => Return
  : never;

// 3. Mapping the entire backend router to the frontend schema
type ClientRouter<Router> = {
  [Procedure in keyof Router]: OmitFirstArg<Router[Procedure]>;
};

// 4. The RPC Client Factory
function createRpcClient<Router extends Record<string, any>>(
  endpoint: string
): ClientRouter<Router> {
  // We use a Proxy to dynamically intercept function calls on the client
  return new Proxy({} as ClientRouter<Router>, {
    get(target, prop: string) {
      return async (...args: any[]) => {
        const response = await fetch(`${endpoint}/${prop}`, {
          method: 'POST',
          body: JSON.stringify({ args })
        });
        return response.json();
      };
    }
  });
}

// 5. Instantiating the tightly coupled client
const api = createRpcClient<typeof backendProcedures>('https://api.example.com');

// The frontend now gets perfect auto-complete!
// api.getUser(123) -> TS Error: Argument of type 'number' is not assignable to 'string'.
// api.getUser('123') -> Returns Promise<{ id: string, name: string, active: boolean }> 
