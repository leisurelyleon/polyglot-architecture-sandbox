use std::sync::atomic::{AtomicUsize, Ordering};
use std::cell::UnsafeCell;
use std::ffi::CString;
use std::ptr;
use std::mem;

// 1. The data payload to be transmitted between processes
#[derive(Copy, Clone, Default)]
#[repr(C)]
pub struct SignalPayload {
    pub instrument_id: u32,
    pub target_price: u32,
    pub volume: u32,
    pub side: u8,
}

// 2. The Lock-Free Queue mapped into Shared Memory
// #[repr(C)] ensures Rust doesn't reorder the struct fields, allowing C++ or other Rust processes to read it.
// We pad the indices to 64 bytes to prevent False Sharing across CPU cores.
#[repr(C)]
pub struct ShmRingBuffer<const CAPACITY: usize> {
    #[repr(C, align(64))]
    write_index: AtomicUsize,
    
    #[repr(C, align(64))]
    read_index: AtomicUsize,
    
    // UnsafeCell opts-out of Rust's strict mutability guarantees, 
    // as another process will be physically mutating this RAM.
    buffer: [UnsafeCell<SignalPayload>; CAPACITY],
}

pub struct IpcChannel<const CAPACITY: usize> {
    shm_ptr: *mut ShmRingBuffer<CAPACITY>,
    shm_fd: i32,
    size: usize,
}

impl<const CAPACITY: usize> IpcChannel<CAPACITY> {
    // 3. Initialize POSIX Shared Memory via libc
    pub fn new(name: &str, is_producer: bool) -> Self {
        let c_name = CString::new(name).unwrap();
        let size = mem::size_of::<ShmRingBuffer<CAPACITY>>();

        unsafe {
            // Open a file descriptor pointing to RAM (tmpfs), not a disk
            let shm_fd = libc::shm_open(
                c_name.as_ptr(),
                libc::O_CREAT | libc::O_RDWR,
                0o666,
            );
            assert!(shm_fd >= 0, "Failed to open shared memory");

            if is_producer {
                libc::ftruncate(shm_fd, size as libc::off_t);
            }

            // Map the RAM into our process's virtual address space
            let shm_ptr = libc::mmap(
                ptr::null_mut(),
                size,
                libc::PROT_READ | libc::PROT_WRITE,
                libc::MAP_SHARED,
                shm_fd,
                0,
            ) as *mut ShmRingBuffer<CAPACITY>;

            assert!(shm_ptr != libc::MAP_FAILED, "mmap failed");

            // If we are the producer creating this, initialize the atomics
            if is_producer {
                ptr::write_bytes(shm_ptr, 0, 1);
                (*shm_ptr).write_index.store(0, Ordering::Relaxed);
                (*shm_ptr).read_index.store(0, Ordering::Relaxed);
            }

            IpcChannel { shm_ptr, shm_fd, size }
        }
    }

    // 4. The Hot-Path Publisher (Runs on Core 1)
    #[inline(always)]
    pub fn publish(&self, payload: SignalPayload) -> bool {
        unsafe {
            let ring = &*self.shm_ptr;
            let current_write = ring.write_index.load(Ordering::Relaxed);
            let current_read = ring.read_index.load(Ordering::Acquire); // Synchronize with reader

            if current_write.wrapping_sub(current_read) >= CAPACITY {
                return false; // Queue is full, drop signal
            }

            // Write data blindly into the UnsafeCell memory slot
            let slot = ring.buffer[current_write % CAPACITY].get();
            ptr::write_volatile(slot, payload);

            // Release memory ordering guarantees the reader sees the payload *before* seeing the updated index
            ring.write_index.store(current_write.wrapping_add(1), Ordering::Release);
            true
        }
    }

    // 5. The Hot-Path Consumer (Runs on Core 2)
    #[inline(always)]
    pub fn consume(&self) -> Option<SignalPayload> {
        unsafe {
            let ring = &*self.shm_ptr;
            let current_read = ring.read_index.load(Ordering::Relaxed);
            let current_write = ring.write_index.load(Ordering::Acquire); // Synchronize with writer

            if current_read == current_write {
                return None; // Queue is empty
            }

            // Read data blindly from the UnsafeCell memory slot
            let slot = ring.buffer[current_read % CAPACITY].get();
            let payload = ptr::read_volatile(slot);

            // Release memory ordering tells the writer we are officially done with this memory slot
            ring.read_index.store(current_read.wrapping_add(1), Ordering::Release);
            Some(payload)
        }
    }
}
