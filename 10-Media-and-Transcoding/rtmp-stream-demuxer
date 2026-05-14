use std::io::{self, Read};
use byteorder::{BigEndian, ReadBytesExt};

// 1. Domain Modeling of an RTMP Chunk
#[derive(Debug)]
pub struct RtmpHeader {
    pub format: u8,
    pub chunk_stream_id: u32,
    pub timestamp: u32,
    pub message_length: u32,
    pub message_type_id: u8,
    pub message_stream_id: u32,
}

pub struct RtmpDemuxer<R: Read> {
    stream: R,
    previous_headers: std::collections::HashMap<u32, RtmpHeader>,
}

impl<R: Read> RtmpDemuxer<R> {
    pub fn new(stream: R) -> Self {
        Self {
            stream,
            previous_headers: std::collections::HashMap::new(),
        }
    }

    // 2. The Core Binary Parsing Engine
    pub fn read_next_chunk(&mut self) -> io::Result<(RtmpHeader, Vec<u8>)> {
        // Read the Basic Header (1 byte)
        let basic_header = self.stream.read_u8()?;
        
        // Extract Format Type (top 2 bits) and Chunk Stream ID (bottom 6 bits)
        let fmt = (basic_header >> 6) & 0x03;
        let mut csid = (basic_header & 0x3F) as u32;

        // Handle extended Chunk Stream IDs (if the bottom bits are 0 or 1)
        if csid == 0 {
            csid = self.stream.read_u8()? as u32 + 64;
        } else if csid == 1 {
            let byte1 = self.stream.read_u8()? as u32;
            let byte2 = self.stream.read_u8()? as u32;
            csid = (byte2 << 8) + byte1 + 64;
        }

        // 3. RTMP Header Compression Logic
        // RTMP omits data if it hasn't changed from the previous chunk on the same stream
        let prev_header = self.previous_headers.get(&csid);
        
        let mut timestamp = 0;
        let mut msg_length = 0;
        let mut msg_type_id = 0;
        let mut msg_stream_id = 0;

        match fmt {
            0 => {
                // Type 0: Absolute 11-byte header
                timestamp = self.read_u24()?;
                msg_length = self.read_u24()?;
                msg_type_id = self.stream.read_u8()?;
                msg_stream_id = self.stream.read_u32::<BigEndian>()?;
            }
            1 => {
                // Type 1: 7-byte header (Stream ID remains the same)
                let prev = prev_header.expect("Type 1 chunk without previous header");
                timestamp = prev.timestamp + self.read_u24()?;
                msg_length = self.read_u24()?;
                msg_type_id = self.stream.read_u8()?;
                msg_stream_id = prev.message_stream_id;
            }
            2 => {
                // Type 2: 3-byte header (Length and Stream ID remain the same)
                let prev = prev_header.expect("Type 2 chunk without previous header");
                timestamp = prev.timestamp + self.read_u24()?;
                msg_length = prev.message_length;
                msg_type_id = prev.message_type_id;
                msg_stream_id = prev.message_stream_id;
            }
            3 => {
                // Type 3: 0-byte header (Everything remains the same!)
                let prev = prev_header.expect("Type 3 chunk without previous header");
                timestamp = prev.timestamp; // (In reality, handling deltas is more complex)
                msg_length = prev.message_length;
                msg_type_id = prev.message_type_id;
                msg_stream_id = prev.message_stream_id;
            }
            _ => unreachable!(),
        }

        let header = RtmpHeader {
            format: fmt,
            chunk_stream_id: csid,
            timestamp,
            message_length: msg_length,
            message_type_id: msg_type_id,
            message_stream_id: msg_stream_id,
        };

        // Save state for future compression lookups
        self.previous_headers.insert(csid, RtmpHeader { ..header });

        // 4. Extract the raw payload (H.264 NAL units or AAC Audio data)
        // Note: RTMP enforces a maximum chunk size (default 128 bytes), so large frames are split.
        // This is simplified to read the exact message length assuming chunk_size >= msg_length.
        let mut payload = vec![0u8; msg_length as usize];
        self.stream.read_exact(&mut payload)?;

        Ok((header, payload))
    }

    // Helper to read 24-bit integers (RTMP's favorite weird format)
    fn read_u24(&mut self) -> io::Result<u32> {
        let b1 = self.stream.read_u8()? as u32;
        let b2 = self.stream.read_u8()? as u32;
        let b3 = self.stream.read_u8()? as u32;
        Ok((b1 << 16) | (b2 << 8) | b3)
    }
}
