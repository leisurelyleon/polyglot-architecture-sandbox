#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include <iostream>

// 1. The ultra-fast, branchless integer parser
// This completely bypasses standard library overhead by exploiting ASCII bit manipulation
inline uint64_t fast_parse_uint(const char* str, size_t len) noexcept {
    uint64_t result = 0;
    // Loop unrolling for small expected lengths (e.g., price and volume fields)
    for (size_t i = 0; i < len; ++i) {
        // ASCII '0' is 0x30. Subtracting '0' is the same as bitwise XOR with 0x30
        result = result * 10 + (str[i] ^ '0');
    }
    return result;
}

// 2. The NASDAQ ITCH-style Add Order Message Structure
// Pack the struct to exactly match the network byte sequence (no padding)
#pragma pack(push, 1)
struct AddOrderMessage {
    char message_type;      // 'A' for Add Order
    uint16_t stock_locate;  // Exchange specific stock ID
    uint16_t tracking_number;
    uint64_t timestamp_ns;  // Nanoseconds since midnight
    uint64_t order_ref;     // Unique order ID
    char buy_sell_indicator;// 'B' or 'S'
    uint32_t shares;
    char stock[8];          // Ticker symbol, right-padded with spaces
    uint32_t price;         // Integer price (e.g., 123.45 becomes 12345)
};
#pragma pack(pop)

// 3. The Hot-Path Network Buffer Handler
// We use std::span to create a zero-copy view over the raw network socket buffer
void process_market_data_buffer(std::span<const uint8_t> network_buffer) noexcept {
    const uint8_t* current_ptr = network_buffer.data();
    const uint8_t* const end_ptr = network_buffer.data() + network_buffer.size();

    while (current_ptr < end_ptr) {
        // Read the message length (assuming a 2-byte header framing)
        uint16_t msg_length = *reinterpret_cast<const uint16_t*>(current_ptr);
        current_ptr += sizeof(uint16_t);

        // Branch prediction optimization: We mostly expect 'A' (Add Order) messages
        if (*current_ptr == 'A') [[likely]] {
            // Direct memory cast. ZERO copying. ZERO heap allocation.
            const AddOrderMessage* order = reinterpret_cast<const AddOrderMessage*>(current_ptr);
            
            // Immediately dispatch to the Strategy Engine (inline to avoid function call overhead)
            // StrategyEngine::on_add_order(order);
        } 
        else if (*current_ptr == 'E') [[unlikely]] {
            // Process Execution message...
        }

        // Jump exactly to the next message in the byte stream
        current_ptr += msg_length;
    }
}
