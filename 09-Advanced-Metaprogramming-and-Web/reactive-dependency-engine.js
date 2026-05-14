// 1. The global tracker for the currently executing effect
let activeEffect = null;

// 2. The central memory store mapping Objects -> Properties -> Sets of Functions
// WeakMap is used so that if the target object is deleted, the memory is freed
const targetMap = new WeakMap();

// 3. The Dependency Tracker (Called when a property is READ)
function track(target, key) {
  if (activeEffect) {
    let depsMap = targetMap.get(target);
    if (!d1epsMap) {
      targetMap.set(target, (depsMap = new Map()));
    }
    let dep = depsMap.get(key);
    if (!dep) {
      depsMap.set(key, (dep = new Set()));
    }
    dep.add(activeEffect);
  }
}

// 4. The Trigger Engine (Called when a property is WRITTEN)
function trigger(target, key) {
  const depsMap = targetMap.get(target);
  if (!depsMap) return;

  const dep = depsMap.get(key);
  if (dep) {
    dep.forEach(effect => effect()); // Re-run all functions dependent on this key
  }
}

// 5. The Metaprogramming Proxy Wrapper
function reactive(target) {
  return new Proxy(target, {
    get(obj, key, receiver) {
      track(obj, key);
      // Reflect guarantees the default behavior of reading a property occurs safely
      return Reflect.get(obj, key, receiver);
    },
    set(obj, key, value, receiver) {
      const oldValue = obj[key];
      const result = Reflect.set(obj, key, value, receiver);
      if (oldValue !== value) {
        trigger(obj, key);
    }
    return result;
    }
  });
}

// 6. The Execution Wrapper
function watchEffect(effect) {
  activeEffect = effect; 
  effect(); // Run once to trigger the 'get' traps and map dependencies
    activeEffect = null;
}

// --- Usage ---
const state = reactive({ count: 0, user: 'Joseph' });

// This function is automatically registered as a dependency of 'state.count'
watchEffect(() => {
  console.log(`[System Render] The current count for ${state.user} is: ${state.count}`);
});

// Changing the property magically triggers the console.log again!
console.log("--- Updating State ---");
state.count = 42;
state.user = 'Leon'; // Triggers again!
