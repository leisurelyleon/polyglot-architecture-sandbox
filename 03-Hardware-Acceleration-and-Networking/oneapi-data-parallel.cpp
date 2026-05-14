#include <sycl/sycl.hpp>
#include <iostream>
#include <vector>

using namespace sycl;

// A highly complex custom reduction kernel using SYCL Sub-Groups
void executeIntelHeterogeneousCompute(queue& q, const std::vector<float>& input, std::vector<float>& inputData, float* finalResult) {
    size_t dataSize = inputData.size();

    // 1. Allocate Shared Unified Memory (USM) accessible by both CPU and Intel GPU
    float* deviceData = malloc_shared<float>(dataSize, q);
    float* deviceResult = malloc_shared<float>(1, q);

    // Copy data to the unified pointer
    std::copy(inputData.begin(), inputData.end(), deviceData);
    *deviceResult = 0.0f;

    // Define the work-group sizing for the ND-Range
    const size_t workGroupSize = 256;
    const size_t numWorkItems = ((dataSize + workGroupSize - 1) / workGroupSize) * workGroupSize;

    std::cout << "[Intel oneAPI] Offloading to: " << q.get_device().get_info<info::device::name>() << "\n";

    // 2. Submit the command group to the hardware queue
    q.submit([&](handler& h)) {
        //Create a local accessor *L1/L2 cache shared among the work-group)
        local_accessor<float, 1> localCache(range<1>(workGroupSize), h);

        // 3. Launch the Parallel For loop mapping to hardware threads
        h.parallel_for(nd_range<1>(range<1>(numWorkItems)), range<1>(workGroupSize)), [=](nd_item<1> item) {
            size_t globalId = item.get_global_linear_id(0);
            size_t localId = item.get_local_linear_id(0);

            // Load data into fast local memory or pad with zero
            localCache[localId] = (globalId < dataSize) ? deviceData[globalId] : 0.0f;
            item.barrier(access::fence_space::local_space);

            // 4. Utilize hardware Sub-Groups (Intel's SIMD lanes) for an ultra-fast reduction
            sub_group sg = item.get_sub_group();
            float sum = localCache[localId];

            // Perform a tree-reduction algorithm directly inside the sub-group registers
            sum = reduce_over_group(sg, sum, std::plus<float>());

            // The leader thread of the group writes to global memory using an atomic lock
            if (sg.leader()) {
                atomic_ref<float, memory_order::relaxed, memory_scope::device, access::address_space::global_space> 
                    atomicRes(*deviceResult);
                atomicRes.fetch_add(sum);
            }
        };
    }.wait(); // 5. Block the host until the device finishes

    *finalResult = *deviceResult;

    // Clean up USM
    free(deviceData, q);
    free(deviceResult, q);
}
