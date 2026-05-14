// --- ARM64 NEON 4x4 Matrix Transpose ---
// Input: x0 = pointer to source matrix, x1 = pointer to destination matrix

.global neon_matrix_transpose
.p2align 4
neon_matrix_transpose:
    // 1. Load the 4x4 matrix into four 128-bit vector registers (v0, v1, v2, v3)
    // Each register holds four 32-but floats (.4s)
    ld1     {v0.4s, v1.4s, v2.4s, v3.4s}, [x0]

    // 2. First Pass: Transpose 2x2 blocks
    // trn1 takes the lower halves of the elements, trn2 take the upper halves
    trn1    v4.4s, v0.4s, v1.4s    // v4 = [ a00, a10, a02, a12 ]
    trn2    v5.4s, v0.4s, v1.4s    // v5 = [ a01, a11, a03, a13 ]
    trn1    v6.4s, v2.4s, v3.4s    // v6 = [ a20, a30, a22, a32 ]
    trn2    v7.4s, v2.4s, v3.4s    // v7 = [ a21, a31, a23, a33 ]

    // 3. Second Pass: Transpose the larger 4x4 structure
    // We treat the registers as containing two 64-bit doubles (.2d) to swap chunks
    trn1    v0.2d, v4.2d, v6.2d    // Final Row 0: [ a00, a10, a20, a30 ]
    trn2    v2.2d, v4.2d, v6.2d    // Final Row 2: [ a02, a12, a22, a32 ]
    trn1    v1.2d, v5.2d, v7.2d    // Final Row 1: [ a01, a11, a21, a31 ]
    trn2    v3.2d, v5.2d, v7.2d    // Final Row 3: [ a03, a13, a23, a33 ]

    // 4. Store the newly transposed registers back into main memory
    st1     {v0.4s, v1.4s, v2.4s, v3.4s}, [x1]

    // Return to caller
    ret 
