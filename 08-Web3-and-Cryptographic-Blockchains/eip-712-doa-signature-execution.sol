// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

contract EIP712DaoGovernance {
    // 1. EIP-712 Domain Separator Configuration (Prevents cross-contract replay attacks)
    bytes32 private constant DOMAIN_TYPEHASH = keccak256("EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)");
    bytes32 private constant VOTE_TYPEHASH = keccak256("Vote(address voter,uint256 proposalId,bool support,uint256 nonce)");
    
    bytes32 public immutable DOMAIN_SEPARATOR;
    
    mapping(address => uint256) public nonces;
    mapping(uint256 => uint256) public proposalVotes;

    error InvalidSignature();
    error NonceMismatch();

    constructor() {
        DOMAIN_SEPARATOR = keccak256(
            abi.encode(
                DOMAIN_TYPEHASH,
                keccak256(bytes("GlobalDAO")),
                keccak256(bytes("1")),
                block.chainid,
                address(this)
            )
        );
    }

    // 2. The Meta-Transaction Relayer Entry Point
    function castVoteBySignature(
        address voter,
        uint256 proposalId,
        bool support,
        uint256 deadline,
        uint8 v, bytes32 r, bytes32 s
    ) external {
        require(block.timestamp <= deadline, "Signature expired");

        uint256 currentNonce = nonces[voter];

        // 3. Reconstruct the Typed Data Hash according to the EIP-712 standard
        bytes32 structHash = keccak256(
            abi.encode(
                VOTE_TYPEHASH,
                voter,
                proposalId,
                support,
                currentNonce
            )
        );

        // Standard Ethereum signed message prefix combining the Domain and Struct
        bytes32 digest = keccak256(
            abi.encodePacked(
                "\x19\x01",
                DOMAIN_SEPARATOR,
                structHash
            )
        );

        // 4. ECDSA Cryptographic Verification
        // Recover the Ethereum address from the mathematical signature
        address recoveredAddress = ecrecover(digest, v, r, s);

        if (recoveredAddress == address(0) || recoveredAddress != voter) {
            revert InvalidSignature();
        }

        // 5. State Mutation
        nonces[voter]++; // Invalidate the signature so it cannot be replayed
        
        if (support) {
            proposalVotes[proposalId]++;
        }
    }
}
