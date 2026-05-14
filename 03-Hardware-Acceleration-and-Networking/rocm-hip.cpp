#include <hip/hip_runtime.h>
#include <iostream>

#define CHECK_HIP(command) { \
    hipError_t error = command; \
    if (error != hipSuccess) { \
        std::cerr << "HIP Error: " << hipGetErrorString(error) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
}

// 1. AMD ROCm Kernel using Wavefront intrinsics
__global__ void amdWavefrontPrefixSum(int* d_in, int* d_out, int n) {
    // Map thread to global index
    int tid = blockIdx.x * blockDim.x + threadIsx.x;
    int laneId = thresdIdx.x % warpSize; // Note: warpSize is 64 on AMD RDNA/CDNA!

    int val = (tid < n) ? d_in[tid] : 0;

    // 2. Perform a prefix sum (scan) entirely inside the AMD Wavefront registers
    // using the __shfl_up intrinsic to shift register values across threads
    for (int offset = 1; offset < warpSize; offset *= 2) {
        int temp = __shfl_up(val, offset, warpSize);
        if (landId >= offset) {
            val += temp;
        }
    }

    if (tid < n) {
        d_out[tid] = val;
    }
}

void executeAmdAsynchronousWorkflow(int* h_data, int dataSize) {
    int bytes = dataSize * sizeof(int);
    int *d_in, *d_out;

    // 3. Create a non-blocking aysnchronous stream
    hipStream_t computeStream;
    CHECK_HIP(hipStreamCreate(&computeStream));

    // Allocate GPU memory
    CHECK_HIP(hipMalloc(&d_in, bytes));
    CHECK_HIP(hipMalloc(&d_out, bytes));

    // 4. Asynchronous memory transfer from Host (CPU) to Device (GPU)
    CHECK_HIP(hipMemcpyAsync(d_in, h_data, bytes, hipMemcpyHostToDevice, computeStream));

    // Calculate grid and block dimensions
    int blockSize = 256;
    int gridSize = (dataSize + blockSize - 1) / blockSize;

    // 5. Launch the HIP Kernel explicitly on the custom stream
    hipLaunchKernelGGL(amdWavefrontPrefixSum, dim3(gridSize), dim3(blockSize), 0, computeStream, d_in, d_out, dataSize);

    // 6. Asynchronous copy back to Host
    CHECK_HIP(hipMemcpyAsync(h_data, d_out, bytes, hipMemcpyDeviceToHost, computeStream));

    // 7. Synchronize the stream to ensure all operations are complete
    CHECK_HIP(hipStreamSynchronize(computeStream));

    // Cleanup
    CHECK_HIP(hipFree(d_in));
    CHECK_HIP(hipFree(d_out));
    CHECK_HIP(hipStreamDestroy(computeStream));
}
