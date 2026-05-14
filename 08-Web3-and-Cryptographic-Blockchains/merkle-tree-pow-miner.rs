use ring::digest::{Context, SHA256};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};

// 1. Core Data Structures mapped strictly to memory
#[derive(Debug, Clone)]
pub struct Transaction {
    pub sender: [u8; 32],
    pub receiver: [u8; 32],
    pub amount: u64,
    pub signature: [u8; 64],
}

#[derive(Debug)]
pub struct BlockHeader {
    pub previous_hash: [u8; 32],
    pub merkle_root: [u8; 32],
    pub timestamp: u64,
    pub difficulty_target: u32,
    pub nonce: u64,
}

// 2. Cryptographic Hashing Utility
fn sha256_double(data: &[u8]) -> [u8; 32] {
    let mut ctx1 = Context::new(&SHA256);
    ctx1.update(data);
    let first_pass = ctx1.finish();

    let mut ctx2 = Context::new(&SHA256);
    ctx2.update(first_pass.as_ref());
    let mut result = [0u8; 32];
    result.copy_from_slice(ctx2.finish().as_ref());
    result
}

// 3. Recursive Merkle Tree Calculation
pub fn calculate_merkle_root(transactions: &[Transaction]) -> [u8; 32] {
    if transactions.is_empty() {
        return [0; 32];
    }

    // Convert transactions to their raw byte hashes
    let mut current_level: Vec<[u8; 32]> = transactions
        .iter()
        .map(|tx| {
            let mut bytes = Vec::new();
            bytes.extend_from_slice(&tx.sender);
            bytes.extend_from_slice(&tx.receiver);
            bytes.extend_from_slice(&tx.amount.to_be_bytes());
            sha256_double(&bytes)
        })
        .collect();

    // Recursively hash pairs until only the root remains
    while current_level.len() > 1 {
        if current_level.len() % 2 != 0 {
            current_level.push(current_level.last().unwrap().clone()); // Duplicate last if odd
        }

        current_level = current_level
            .chunks(2)
            .map(|pair| {
                let mut combined = pair[0].to_vec();
                combined.extend_from_slice(&pair[1]);
                sha256_double(&combined)
            })
            .collect();
    }

    current_level[0]
}

// 4. Multi-Threaded Proof of Work Miner
pub fn mine_block_parallel(
    header_template: &mut BlockHeader,
    difficulty_prefix_zeros: usize,
) -> bool {
    let found = Arc::new(AtomicBool::new(false));
    let winning_nonce = Arc::new(AtomicU64::new(0));

    // Parallel iterator spanning up to u64::MAX across all CPU cores
    (0..u64::MAX).into_par_iter().find_any(|&current_nonce| {
        if found.load(Ordering::Relaxed) {
            return true; // Another thread found it, abort
        }

        // Serialize the header for hashing
        let mut buffer = Vec::with_capacity(80);
        buffer.extend_from_slice(&header_template.previous_hash);
        buffer.extend_from_slice(&header_template.merkle_root);
        buffer.extend_from_slice(&header_template.timestamp.to_be_bytes());
        buffer.extend_from_slice(&header_template.difficulty_target.to_be_bytes());
        buffer.extend_from_slice(&current_nonce.to_be_bytes());

        let hash = sha256_double(&buffer);

        // Check if the hash meets the difficulty target (leading zeros)
        let mut zero_count = 0;
        for &byte in &hash {
            if byte == 0 {
                zero_count += 2;
            } else if byte < 16 {
                zero_count += 1;
                break;
            } else {
                break;
            }
        }

        if zero_count >= difficulty_prefix_zeros {
            found.store(true, Ordering::Relaxed);
            winning_nonce.store(current_nonce, Ordering::Relaxed);
            return true;
        }
        false
    });

    if found.load(Ordering::SeqCst) {
        header_template.nonce = winning_nonce.load(Ordering::SeqCst);
        return true;
    }
    false
}
