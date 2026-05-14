#include <iostream>
#include <vector>
#include <unordered_map>
#include "DlSystem/DlError.hpp"
#include "DlSystem/RuntimeList.hpp"
#include "DlSystem/UserBufferMap.hpp"
#include "DlSystem/IUserBufferFactory.hpp"
#include "DlContainer/IDlContainer.hpp"
#include "SNPE/SNPE.hpp"
#include "SNPE/SNPEBuilder.hpp"

// A custom memory allocator aligned to 128 bytes, required for zero-copy DSP execution
void* allocateAlignedMemory(size_t size) {
    void* ptr = nullptr;
    if (posix_memalign(&ptr, 128, size) != 0) {
        std::cerr << "Failed to allocate 128-byte aligned memory for DSP." << std::endl;
        return nullptr;
    }
    return ptr;
}

std::unique_ptr<zdl::SNPE::SNPE> buildNeuralNet(const std::string& containerPath) {
    std::unique_ptr<zdl::DlContainer::IDlContainer> container = 
        zdl::DlContainer::IDlContainer::open(zdl::DlSystem::String(containerPath.c_str()));
        
    if (!container) {
        std::cerr << "Error parsing DLC: " << zdl::DlSystem::getLastErrorString() << std::endl;
        return nullptr;
    }

    zdl::DlSystem::RuntimeList runtimeList;
    zdl::DlSystem::Runtime_t targetRuntime = zdl::DlSystem::Runtime_t::DSP;

    // Fallback logic: Try Hexagon DSP, then GPU, then CPU
    if (zdl::SNPE::SNPEFactory::isRuntimeAvailable(targetRuntime)) {
        runtimeList.add(targetRuntime);
    } else if (zdl::SNPE::SNPEFactory::isRuntimeAvailable(zdl::DlSystem::Runtime_t::GPU)) {
        runtimeList.add(zdl::DlSystem::Runtime_t::GPU);
    } else {
        runtimeList.add(zdl::DlSystem::Runtime_t::CPU);
    }

    zdl::SNPE::SNPEBuilder snpeBuilder(container.get());
    
    // Construct the execution engine with strict hardware constraints
    std::unique_ptr<zdl::SNPE::SNPE> snpe = snpeBuilder.setOutputLayers({})
        .setRuntimeProcessorOrder(runtimeList)
        .setUseUserSuppliedBuffers(true) // Crucial for maximum performance
        .setPerformanceProfile(zdl::DlSystem::PerformanceProfile_t::BURST)
        .build();

    return snpe;
}

// Execution block mapping physical memory to the AI Tensor pipeline
bool executeInference(zdl::SNPE::SNPE* snpe, float* rawInputData, size_t inputSize) {
    zdl::DlSystem::UserBufferMap inputMap, outputMap;
    std::vector<std::unique_ptr<zdl::DlSystem::IUserBuffer>> bufferPointers;

    // Bind input buffer
    const auto& inputNames = snpe->getInputTensorNames();
    const char* inputName = inputNames->at(0);
    
    // Create an SDK-aware buffer pointer wrapping our raw memory
    zdl::DlSystem::UserBufferEncodingFloat userBufferEncodingFloat;
    bufferPointers.push_back(
        zdl::SNPE::SNPEFactory::getUserBufferFactory().createUserBuffer(
            rawInputData, inputSize * sizeof(float),
            snpe->getInputDimensions(inputName)->getStrides(),
            &userBufferEncodingFloat
        )
    );
    inputMap.add(inputName, bufferPointers.back().get());

    // Execute the neural network graph synchronously
    if (!snpe->execute(inputMap, outputMap)) {
        std::cerr << "Inference engine failed: " << zdl::DlSystem::getLastErrorString() << std::endl;
        return false;
    }
    
    std::cout << "Hardware acceleration sequence completed successfully." << std::endl;
    return true;
}
