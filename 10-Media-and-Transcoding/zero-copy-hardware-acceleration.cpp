#include <iostream>
#include <cuda.h>
#include <nvEncodeAPI.h>

// Helper macro to catch raw hardware errors
#define NVENC_API_CALL(nvencAPI)                                                                   \
    do {                                                                                           \
        NVENCSTATUS status = nvencAPI;                                                             \
        if (status != NV_ENC_SUCCESS) {                                                            \
            std::cerr << "NVENC Hardware Error Code: " << status << " at line " << __LINE__ << '\n';\
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

class NvencHardwareEncoder {
private:
    void* hEncoder;
    NV_ENCODE_API_FUNCTION_LIST nvencAPI;
    CUcontext cudaContext;

public:
    NvencHardwareEncoder(CUcontext ctx, uint32_t width, uint32_t height) : cudaContext(ctx), hEncoder(nullptr) {
        // 1. Load the NVENC API function pointers directly from the driver
        uint32_t version = 0;
        uint32_t currentVersion = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
        NvEncodeAPIGetMaxSupportedVersion(&version);
        if (currentVersion > version) {
            throw std::runtime_error("NVIDIA Display Driver does not support this NVENC SDK version.");
        }

        nvencAPI.version = NV_ENCODE_API_FUNCTION_LIST_VER;
        NvEncodeAPICreateInstance(&nvencAPI);

        // 2. Open an encoding session on the specific CUDA context (The RTX GPU)
        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS sessionParams = { NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER };
        sessionParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
        sessionParams.device = (void*)cudaContext;
        sessionParams.apiVersion = NVENCAPI_VERSION;
        NVENC_API_CALL(nvencAPI.nvEncOpenEncodeSessionEx(&sessionParams, &hEncoder));

        // 3. Configure the Hardware Silicon (The most critical and complex step)
        NV_ENC_INITIALIZE_PARAMS initParams = { NV_ENC_INITIALIZE_PARAMS_VER };
        initParams.encodeGUID = NV_ENC_CODEC_HEVC_GUID; // H.265 Hardware Encoding
        initParams.presetGUID = NV_ENC_PRESET_P4_GUID;  // P4 is the optimal balance of quality and speed
        initParams.encodeWidth = width;
        initParams.encodeHeight = height;
        initParams.darWidth = width;
        initParams.darHeight = height;
        initParams.frameRateNum = 60;
        initParams.frameRateDen = 1;
        initParams.enablePTD = 1; // Picture Type Decision (Let the GPU decide I/B/P frames)

        // Setup custom HEVC config targeting high-fidelity gaming footage
        NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
        initParams.encodeConfig = &encodeConfig;
        NVENC_API_CALL(nvencAPI.nvEncGetEncodePresetConfig(hEncoder, initParams.encodeGUID, initParams.presetGUID, &encodeConfig));
        
        // Force Rate Control to Constant Bitrate (CBR) for smooth streaming
        encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
        encodeConfig.rcParams.averageBitRate = 8000000; // 8 Mbps

        // Lock the hardware configuration into the silicon
        NVENC_API_CALL(nvencAPI.nvEncInitializeEncoder(hEncoder, &initParams));
        std::cout << "[NVENC] Hardware Encoder Silicon Successfully Initialized for HEVC." << std::endl;
    }

    ~NvencHardwareEncoder() {
        if (hEncoder) {
            nvencAPI.nvEncDestroyEncoder(hEncoder);
        }
    }

    // 4. Zero-Copy Execution (Frames are passed directly from VRAM to the Encoder)
    void encodeFrame(CUdeviceptr d_frameBuffer, void* outputBitstreamBuffer) {
        NV_ENC_PIC_PARAMS picParams = { NV_ENC_PIC_PARAMS_VER };
        
        // Map the CUDA memory pointer to the NVENC input format (YUV420)
        picParams.inputBuffer = (void*)d_frameBuffer;
        picParams.bufferFmt = NV_ENC_BUFFER_FORMAT_IYUV;
        picParams.inputWidth = 1920;
        picParams.inputHeight = 1080;
        
        // Output destination for the encoded NAL units
        picParams.outputBitstream = outputBitstreamBuffer;
        picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;

        // Command the silicon to execute the matrix transformations
        NVENC_API_CALL(nvencAPI.nvEncEncodePicture(hEncoder, &picParams));
    }
};
