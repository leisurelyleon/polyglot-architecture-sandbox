#include <string>
#include <string_view>
#include "proxy_wasm_intrinsics.h"

// 1. Define the custom WebAssembly Filter Context
// This object is instantiated for every single HTTP request that passes through the gateway.
class SecurityGatewayContext : public Context {
public:
    explicit SecurityGatewayContext(uint32_t id, RootContext* root) : Context(id, root) {}

    // Override the hook that fires exactly when HTTP Headers are parsed
    FilterHeadersStatus onRequestHeaders(uint32_t headers, bool end_of_stream) override;
};

// 2. Define the Root Context (Manages the lifecycle of the filter plugin)
class SecurityGatewayRootContext : public RootContext {
public:
    explicit SecurityGatewayRootContext(uint32_t id) : RootContext(id) {}
    
    // Factory method required by the WASM ABI to spawn request contexts
    Context* createContext(uint32_t context_id) override {
        return new SecurityGatewayContext(context_id, this);
    }
};

// 3. Register the WASM ABI entrypoint so Envoy can load this compiled binary
static RegisterContextFactory register_SecurityGatewayContext(
    CONTEXT_FACTORY(SecurityGatewayContext),
    ROOT_FACTORY(SecurityGatewayRootContext),
    "enterprise_security_filter"
);

// 4. The Core Request Interceptor Logic
FilterHeadersStatus SecurityGatewayContext::onRequestHeaders(uint32_t headers, bool end_of_stream) {
    
    // Extract a specific header from the raw HTTP stream
    auto user_agent = getRequestHeader("user-agent");
    auto x_api_key = getRequestHeader("x-api-key");

    // --- Threat Protection Logic ---
    if (user_agent != nullptr && user_agent->value().find("Nmap") != std::string::npos) {
        // We detected a malicious port scanner. 
        // Immediately halt the request and return a 403 Forbidden directly from the Edge Gateway.
        sendLocalResponse(403, "Forbidden", "Malicious scanning tools are prohibited.", {});
        return FilterHeadersStatus::StopIteration;
    }

    if (x_api_key == nullptr || x_api_key->value().empty()) {
        sendLocalResponse(401, "Unauthorized", "Missing mandatory X-API-Key header.", {});
        return FilterHeadersStatus::StopIteration;
    }

    // --- Header Manipulation ---
    // Inject a cryptographically secure span ID for Distributed Tracing (Jaeger/Zipkin)
    // In a real WASM filter, we would call the proxy_get_random_bytes intrinsic here
    addRequestHeader("x-internal-span-id", "spn-78a9c-wasm-generated");

    // Remove headers we don't want the backend to see to save bandwidth
    removeRequestHeader("x-forwarded-for"); 

    // Continue the proxying process. Allow the request to reach the internal gRPC service.
    return FilterHeadersStatus::Continue;
}
