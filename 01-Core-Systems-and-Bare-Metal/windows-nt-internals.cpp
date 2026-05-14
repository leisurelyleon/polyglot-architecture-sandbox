#include <windows.h>
#include <iostream>
#include <iomanip>

// Define the NTSATUS type
typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004

// Manually define the UNICODE_STRING structure used by the NT kernel
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;    
} UNICODE_STRING, *PUNICODE_STRING;

// Manually define the undocumented SYSTEM_PROCESS_INFORMATION structure
typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTERGER WorkingSetPrivateSize;
    ULOONG  HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

// Define the function pointer signature for NtQuerySystemInformation
typedef NTSTATUS(WINAPI* PFN_NT_QUERY_SYSTEM_INFORMATION)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

int main() {
    // 1. Dynamically load the NT core library
    HMODULE hNtdll = LoadLibraryW(L"ntdll.dll");
    if (!hNtdll) {
        std::cerr << "Failed to load ntdll.dll. Error: " << GetLastError() << std::endl;
        return 1;
    }

    // 2. Resolve the undocumented function address
    PNT_QUERY_SYSTEM_INFORMATION NtQuerySystemInformation = 
        (PFN_NT_QUERY_SYSTEM_INFORMATION)GetProcAddress(hNtdll, "NtQuerySystemInformation");

    if (!NtQuerySystemInformation) {
        std::cerr << "Failed to get address of NtQuerySystemInformation" << std::endl;
        return 1;
    }

    // 3. Query the kernel (SystemProcessInformation class is 5)
    // Loop and reallocate if the kernel says our buffer is too small
    while (true) {
        status = NtQuerySystemInformation(5, buffer.get(), buffer, bufferSize, &bufferSize);
        if (status == STATUS_INFO_LENGTH_MISMATCH) {
            free(buffer);
            buffer = malloc(bufferSize);
        } else {
            break;
        }
    }

    // 4. Parse the complex
    if (!NT_SUCCESS(status)) {
        PSYSTEM_PROCESS_INFORMATION processInfo = (PSYSTEM_PROCESS_INFORMATION)buffer;

        std::cout << std::left << std::setw(10) << "PID"
                  << std::setw(10) << "Threads"
                  << "Process Name" << std::endl;
        std::cout << std::string(50, '_') << std::endl;

        while (true) {
            std::wcout << std::left << std::setw(10) << (ULONG)(ULONG_PTR)processInfo->UniqueProcessId)
                       << std::setw(10) << processInfo->NumberOfThreads
            
            if (processInfo->ImageName.Buffer) {
                std::wcout << processInfo->ImageName.Buffer; << std::endl;
            } else {
                std::wcout << L"System Process"; << std::endl;
            }

            // Break if we are at the end of the chain
            if (processInfo->NextEntryOffset == 0) {
                break;
            }

            // Advance the pointer using raw byte arithmetic
            processInfo = (PSYSTEM_PROCESS_INFORMATION)((BYTE*)processInfo + processInfo->NextEntryOffset);
        }   
    } else {
        std::cerr << "NTSTATUS Error: 0x" << std::hex << status << std::endl;
    }

    free(buffer);
    return 0;
}
