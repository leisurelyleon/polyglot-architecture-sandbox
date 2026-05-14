package mediamuxer

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
)

const (
	TsPacketSize = 188
	SyncByte     = 0x47
)

// 1. Write the Program Association Table (PAT)
// This is the absolute first packet a media player reads to understand the stream.
func WritePAT(w io.Writer, continuityCounter byte) error {
	packet := make([]byte, TsPacketSize)
	
	// TS Header (4 bytes)
	packet[0] = SyncByte
	packet[1] = 0x40 // Payload Unit Start Indicator (1) + PID High (0)
	packet[2] = 0x00 // PID Low (0x0000 = PAT)
	packet[3] = 0x10 | (continuityCounter & 0x0F) // No adaptation field, payload only

	// Pointer field (Start of PAT)
	packet[4] = 0x00

	// PAT Payload (Table ID 0x00)
	patPayload := []byte{
		0x00,       // Table ID
		0xB0, 0x0D, // Section Length (13 bytes)
		0x00, 0x01, // Transport Stream ID
		0xC1,       // Version number & Current/Next
		0x00, 0x00, // Section number / Last section
		0x00, 0x01, // Program Number (1)
		0xE0, 0x00, // PMT PID (Assigning PID 4096 / 0x1000 to the PMT)
	}

	// Calculate CRC32 for the PAT payload (omitted for brevity, mocked here)
	crc32 := []byte{0xFF, 0xFF, 0xFF, 0xFF}

	// Copy payload into the 188-byte packet, leaving the rest padded with 0xFF
	copy(packet[5:], patPayload)
	copy(packet[5+len(patPayload):], crc32)
	for i := 5 + len(patPayload) + 4; i < TsPacketSize; i++ {
		packet[i] = 0xFF
	}

	_, err := w.Write(packet)
	return err
}

// 2. The Core Multiplexing Engine for PES (Packetized Elementary Stream) data
func MuxH264Frame(w io.Writer, pid uint16, pts uint64, dts uint64, nalUnit []byte, cc *byte) error {
	buf := bytes.NewReader(nalUnit)
	isFirstPacket := true

	for buf.Len() > 0 {
		packet := make([]byte, TsPacketSize)
		packet[0] = SyncByte

		// Set PID (e.g., 0x0100 for Video)
		packet[1] = byte(pid >> 8)
		packet[2] = byte(pid & 0xFF)

		if isFirstPacket {
			packet[1] |= 0x40 // Set Payload Unit Start Indicator (PUSI)
		}

		// Increment and set Continuity Counter
		*cc = (*cc + 1) & 0x0F
		packet[3] = 0x10 | *cc // Payload only initially

		payloadOffset := 4

		// 3. If it's the first packet, construct the PES Header for Audio/Video Sync
		if isFirstPacket {
			pesHeader := make([]byte, 19)
			pesHeader[0], pesHeader[1], pesHeader[2] = 0x00, 0x00, 0x01 // Start Code
			pesHeader[3] = 0xE0 // Stream ID (Video 0)
			
			// PES Packet Length (0 means unbounded for video)
			pesHeader[4], pesHeader[5] = 0x00, 0x00 
			
			// Flags: Data alignment, PTS/DTS present
			pesHeader[6] = 0x80
			pesHeader[7] = 0xC0
			pesHeader[8] = 0x0A // Header data length (10 bytes for PTS + DTS)

			// 4. Bit-shift the 33-bit Presentation Time Stamp (PTS) into the weird MPEG-TS format
			pesHeader[9] = 0x31 | byte((pts>>29)&0x0E)
			binary.BigEndian.PutUint16(pesHeader[10:], uint16((pts>>14)&0xFFFE|1))
			binary.BigEndian.PutUint16(pesHeader[12:], uint16((pts<<1)&0xFFFE|1))

			// Bit-shift the Decode Time Stamp (DTS)
			pesHeader[14] = 0x11 | byte((dts>>29)&0x0E)
			binary.BigEndian.PutUint16(pesHeader[15:], uint16((dts>>14)&0xFFFE|1))
			binary.BigEndian.PutUint16(pesHeader[17:], uint16((dts<<1)&0xFFFE|1))

			copy(packet[payloadOffset:], pesHeader)
			payloadOffset += len(pesHeader)
			isFirstPacket = false
		}

		// 5. Fill the remainder of the 188-byte packet with the actual H.264 Video Data
		bytesToRead := TsPacketSize - payloadOffset
		if buf.Len() < bytesToRead {
			// If this is the end of the frame, we must use an Adaptation Field to pad with 0xFF
			bytesToRead = buf.Len()
			paddingSize := TsPacketSize - payloadOffset - bytesToRead
			
			packet[3] |= 0x20 // Flag that an adaptation field is present
			packet[payloadOffset] = byte(paddingSize - 1) // Adaptation Field Length
			if paddingSize > 1 {
				packet[payloadOffset+1] = 0x00 // Flags
				for i := 2; i < paddingSize; i++ {
					packet[payloadOffset+i] = 0xFF // Stuffing bytes
				}
			}
			payloadOffset += paddingSize
		}

		buf.Read(packet[payloadOffset : payloadOffset+bytesToRead])

		if _, err := w.Write(packet); err != nil {
			return fmt.Errorf("failed to write TS packet: %w", err)
		}
	}
	return nil
}
