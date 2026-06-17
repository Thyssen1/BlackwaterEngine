#pragma once

#ifdef BLACKWATERCORE_EXPORTS
    #define BLACKWATERCORE_API __declspec(dllexport)
#else
    #define BLACKWATERCORE_API __declspec(dllimport)
#endif

extern "C" {
    BLACKWATERCORE_API int InitializeEngine();
    BLACKWATERCORE_API int ShutdownEngine();
}
