#include <core.p4>
#include <v1model.p4>

// 1. Define the exact sizes of our Ethernet and IPv4 Headers
typedef bit<48> macAddr_t;
typedef bit<32> ip4Addr_t;

header ethernet_t {
    macAddr_t dstAddr;
    macAddr_t srcAddr;
    bit<16>   etherType;
}

header ipv4_t {
    bit<4>    version;
    bit<4>    ihl;
    bit<8>    diffserv;
    bit<16>   totalLen;
    bit<16>   identification;
    bit<3>    flags;
    bit<13>   fragOffset;
    bit<8>    ttl;
    bit<8>    protocol;
    bit<16>   hdrChecksum;
    ip4Addr_t srcAddr;
    ip4Addr_t dstAddr;
}

struct metadata {
    // Empty for this simple example, but used for deep packet inspection state
}

struct headers {
    ethernet_t ethernet;
    ipv4_t     ipv4;
}

// 2. The Parser State Machine (Decodes the raw bitstream)
parser MyParser(packet_in packet,
                out headers hdr,
                inout metadata meta,
                inout standard_metadata_t standard_metadata) {

    state start {
        // Extract the Ethernet header first
        packet.extract(hdr.ethernet);
        // Look at the etherType to figure out what comes next
        transition select(hdr.ethernet.etherType) {
            0x0800: parse_ipv4;
            default: accept;
        }
    }

    state parse_ipv4 {
        // If it was 0x0800, we know the next bits are IPv4
        packet.extract(hdr.ipv4);
        transition accept;
    }
}

// 3. The Control block (Routing logic)
control MyIngress(inout headers hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {

    // Define a routing table that the router's OS will populate
    table ipv4_lpm {
        key = {
            hdr.ipv4.dstAddr: lpm; // Longest Prefix Match (Subnetting)
        }
        actions = {
            ipv4_forward;
            drop;
            NoAction;
        }
        size = 1024;
        default_action = drop();
    }

    // Define the action to take if we find a match in the table
    action ipv4_forward(macAddr_t dstAddr, bit<9> port) {
        // Standard IP routing rules: decrease Time-To-Live, update MAC, set output port
        standard_metadata.egress_spec = port;
        hdr.ethernet.srcAddr = hdr.ethernet.dstAddr;
        hdr.ethernet.dstAddr = dstAddr;
        hdr.ipv4.ttl = hdr.ipv4.ttl - 1;
    }

    apply {
        if (hdr.ipv4.isValid()) {
            ipv4_lpm.apply();
        }
    }
}
