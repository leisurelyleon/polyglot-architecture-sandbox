#include <mma.h>
#include <cuba_fp16.h>
#include <cuda_runtime.h>

using namespace nvcuda;

// Define the dimensions of the Tensor Core operation (16x16x16 is a standard for FP16)
const int WMMA_M = 16;
const int WMMA_N = 16;
const int WMMA_K = 16;

__global__ void tensorCoreMatMulKernel(half *a, half *b, float *c, int M, int N, int K) {
    // 1. Declare the WMMA fragments (registers sitting directly on the Tensor Cores)
    wmma::fragment<wmma::matrix_a, WMMA_M, WMMA_N, WMMA_K, half, wmma::row_major> a_frag;
    wmma::fragment<wmma::matrix_b, WMMA_M, WMMA_N, WMMA_K, half, wmma::col_major> b_frag;
    wmma::fragment<wmma::accumulator, WMMA_M, WMMA_N, WMMA_K, float> c_frag;

    // 2. Initialize the accumulator fragment to zero
    wmma::fill_fragment(c_frag, 0.0f);

    // Calculate the warp's global position in the grid
    int warpM = (blockIdx.x * blockDim.x + threadIdx.x) / warpSize;
    int warpN = (blockIdx.y * blockDim.y + threadIdx.y);

    // 3. Loop over the K-dimension to accumulate the dot products
    for (int i = 0; i < K; i += WMMA_K) {
        int aRow = warpM * WMMA_M;
        int aCol = i;
        int bRow = i;
        int bCol = warpN * WMMA_N;

        // Bounds checking
        if (aRow < M && aCol < K && bRow < K && bCol < N) {
            // 4. Synchronously load data from global GPU memory into the Tensor Core registers
            wmma::load_matrix_sync(a_frag, a + aRow * K + aCol, K);
            wmma::load_matrix_sync(b_frag, b + bRow * N + bCol, N);

            // 5. Execute the hardware-accelerated matrix multiplication
            wmma::mma_sync(c_frag, a_frag, b_frag, c_frag);
        }
    }

    // 6. Store the final accumulated result back to global memory
    int cRow = warpM * WMMA_M;
    int cCol = warpN * WMMA_N;
    if (cRow < M && cCol < N) {
        wmma::store_matrix_sync(c + cRow * N + cCol, c_frag, N, wmma::mem_row_major);
    }
}
