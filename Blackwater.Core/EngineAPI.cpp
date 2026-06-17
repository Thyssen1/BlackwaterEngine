#include "EngineAPI.h"
#include <windows.h>

extern "C" {
    BLACKWATERCORE_API int InitializeEngine()
    {
        return 1;
    }

    BLACKWATERCORE_API int ShutdownEngine()
    {
        return 1;
    }
}