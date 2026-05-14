import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.locks.LockSupport;

public class LmaxDisruptorSimulator {

    // 1. The Padding Hack: Prevent False Sharing on modern multi-core CPUs
    // A cache line is typically 64 bytes. We pad our atomic longs so they sit on separate cache lines.
    static class PaddedAtomicLong extends AtomicLong {
        public volatile long p1, p2, p3, p4, p5, p6, p7 = 7L;
        public PaddedAtomicLong(long initialValue) { super(initialValue); }
    }

    // 2. The Pre-Allocated Event Payload (Zero Garbage Collection)
    static class TradeEvent {
        public long price;
        public int quantity;
        public String symbol;
    }

    // 3. The Lock-Free Ring Buffer
    static class RingBuffer {
        private final TradeEvent[] entries;
        private final int mask;
        
        // Separate cache lines for producer and consumer trackers
        private final PaddedAtomicLong producerSequence = new PaddedAtomicLong(0);
        private final PaddedAtomicLong consumerSequence = new PaddedAtomicLong(0);

        public RingBuffer(int capacity) {
            // Capacity must be a power of 2 for fast bitwise masking
            this.mask = capacity - 1;
            this.entries = new TradeEvent[capacity];
            for (int i = 0; i < capacity; i++) {
                entries[i] = new TradeEvent(); // Pre-allocate objects at startup
            }
        }

        // Producer Thread
        public void publish(long price, int qty, String symbol) {
            long sequence = producerSequence.getAndIncrement();
            
            // Wait/Spin if the buffer wraps around and catches the consumer
            while (sequence - consumerSequence.get() >= entries.length) {
                LockSupport.parkNanos(1); // Micro-sleep to yield CPU
            }

            // Write data to the pre-allocated slot without instantiating a new object
            TradeEvent event = entries[(int) (sequence & mask)];
            event.price = price;
            event.quantity = qty;
            event.symbol = symbol;
        }

        // Consumer Thread
        public void consume() {
            long sequence = consumerSequence.get();
            
            // Spin until the producer has moved past our target sequence
            while (sequence >= producerSequence.get()) {
                LockSupport.parkNanos(1);
            }

            TradeEvent event = entries[(int) (sequence & mask)];
            System.out.println("[Disruptor Engine] Processed trade: " + event.quantity + " of " + event.symbol);

            // Memory barrier release
            consumerSequence.lazySet(sequence + 1);
        }
    }

    public static void main(String[] args) {
        RingBuffer buffer = new RingBuffer(1024);

        Thread producer = new Thread(() -> {
            for (int i = 0; i < 50000; i++) buffer.publish(15000, 10, "AAPL");
        });

        Thread consumer = new Thread(() -> {
            for (int i = 0; i < 50000; i++) buffer.consume();
        });

        consumer.start();
        producer.start();
    }
}
