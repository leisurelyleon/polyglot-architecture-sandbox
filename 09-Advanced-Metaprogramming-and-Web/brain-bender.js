const { Worker, isMainThread, parentPort, workerData } = require('worker_threads');

// A constant to represent the state of our shared lock
const UNLOCKED = 0;
const LOCKED = 1;

if (isMainThread) {
    console.log("[Main] Orchestrating thread pool...");

    // 1. Allocate  a raw block of memory shared across all threads.
    // We need 4 bytes for the mutex lock, and 400 bytes for an array of 100 integers.
    const sharedBuffer = new SharedArrayBuffer(4 + (100 * 4));

    // 2. Create typed arrays pointing to specific offsets in that raw buffer
    const mutex = new Int32Array(sharedBuffer, 0, 1);
    const sharedData = new Int32Array(sharedBuffer, 4, 100); 
    
    // Initialize the lock state
    Atomics.store(mutex, 0, UNLOCKED);

    const numWorkers = 4;
    let completedWorkers = 0;

    // 3. Spawn workers and pass them the shared memory reference
    for (let i = 0; i < numWorkers; i++) {
        const worker = new Worker(__filename, { 
            workerData: { id: i, sharedBuffer } 
        });

        worker.on('message', (msg) => {
            console.log(`[Main] Received message from Worker ${msg.id}: ${msg.status}`);
        });

        worker.on('exit', () => {
            completedWorkers++;
            if (completedWorkers === numWorkers) {
                console.log("[Main] All workers finished.");
                // Calculate the sum of our shared memory array
                const sum = sharedData.reduce((acc, val) => acc + val, 0);
                console.log(`[Main] Final synchronized data sum: ${sum}`);
            }
        });
    }

} else { 
    // --- WORKER THREAD LOGIC ---
    const { id, sharedBuffer } = workerData;
    const mutex = new Int32Array(sharedBuffer, 0, 1);
    const sharedData = new Int32Array(sharedBuffer, 4, 100);
    
    console.log(`[Worker ${id}] Booted up and accessing shared memory.`);

    // Perform some arbitrary work that requires exclusive access to the memory
    for (let i = 0; i < 25; i++) {

        // 4. Spin-lock implementation using Atomics
        // This will block the thread in a while loop until it successfully changes to UNLOCKED to LOCKED
        while (Atomics.compareExchange(mutex, 0, UNLOCKED, LOCKED) !== UNLOCKED) {
            // In a real high-contention scenario, we'd use Atomics.wait() here to sleep
            // instead of burning CPU cycles, but spin-locks are classic low-level syntax!
        }

        // --- CRITICAL SECTION START ---
        // Only one thread can be here at a time.

        // Pick a random index and safely increment it 
        const randomIndex = Math.floor(Math.random() * 100);

        // Even though we have a lock, using Atomics.add is best practice for shared memory
        const previousValue = Atomics.add(sharedData, randomIndex, 1);

        // --- CRITICAL SECTION END ---

        // 5. Release the lock
        Atomics.store(mutex, 0, UNLOCKED);
        // Wake up any threads that might me sleeping (if we were using Atomics.wait)
        Atomics.notify(mutex, 0, 1);
    }

    parentPort.postMessage({ id, status: "Processing complete" });
}
