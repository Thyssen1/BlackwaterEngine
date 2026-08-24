#include "Blackwater/Check.h"

#include <sstream>
#include <string>

// An anonymous namespace gives these internal linkage: they exist only inside
// this translation unit and are invisible to the linker elsewhere. It is the
// closest C++ has to C#'s `internal`, except the scope is this one file rather
// than the whole assembly.
namespace
{
    /// Ask the system for a human-readable description of an HRESULT.
    std::string DescribeHResult(HRESULT hr)
    {
        char* buffer = nullptr;

        // FormatMessageA, not FormatMessage: UNICODE is defined project-wide,
        // so the unsuffixed name would resolve to the wide version. We want
        // narrow characters here because the message ends up in what(), and
        // std::runtime_error only speaks const char*.
        //
        // FORMAT_MESSAGE_ALLOCATE_BUFFER makes the system allocate for us --
        // which is why `buffer` is passed by address through a cast that looks
        // wrong but is exactly what the API documents.
        const DWORD length = ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            static_cast<DWORD>(hr),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&buffer),
            0,
            nullptr);

        if (length == 0 || buffer == nullptr)
        {
            // Plenty of D3D and DXGI codes have no system description.
            return "no system description for this code";
        }

        std::string message(buffer, length);

        // The buffer came from the system allocator, so it must go back to it.
        // There is no RAII wrapper here yet, so this is the one place in the
        // engine doing a manual free -- keep it directly under the allocation.
        ::LocalFree(buffer);

        // System messages arrive with a trailing CR/LF. Strip it so the text
        // composes cleanly into our own layout.
        while (!message.empty() && (message.back() == '\r' || message.back() == '\n'))
        {
            message.pop_back();
        }

        return message;
    }

    std::string BuildMessage(HRESULT hr, const char* expression, const char* file, int line)
    {
        std::ostringstream out;
        out << "HRESULT failure\n"
            << "  call : " << expression << '\n'
            << "  code : 0x" << std::hex << std::uppercase
                             << static_cast<unsigned long>(hr) << '\n'
            << "  what : " << DescribeHResult(hr) << '\n'
            << "  at   : " << file << '(' << std::dec << line << ')';

        return out.str();
    }
}

namespace bw
{
    // The message is built before the base class is constructed: the member
    // initialiser list runs std::runtime_error's constructor with the finished
    // string. There is no way to "set the message later" -- std::runtime_error
    // takes it once, at construction.
    HResultError::HResultError(HRESULT hr, const char* expression, const char* file, int line)
        : std::runtime_error(BuildMessage(hr, expression, file, line))
        , m_hr(hr)
    {
    }

    void ThrowLastError(const char* expression, const char* file, int line)
    {
        const DWORD error = ::GetLastError();

        // A call can fail while leaving the last-error code at zero -- rare,
        // but HRESULT_FROM_WIN32(0) is S_OK, and reporting "failure: success"
        // is worse than useless. Substitute a generic failure instead.
        const HRESULT hr = (error == ERROR_SUCCESS)
            ? E_FAIL
            : HRESULT_FROM_WIN32(error);

        throw HResultError(hr, expression, file, line);
    }

    void ThrowIfFailed(HRESULT hr, const char* expression, const char* file, int line)
    {
        // FAILED() tests the sign bit. Note that success is not only S_OK --
        // S_FALSE (1) is a *success* code, and treating it as failure is a
        // classic D3D bug.
        if (FAILED(hr))
        {
            throw HResultError(hr, expression, file, line);
        }
    }
}
