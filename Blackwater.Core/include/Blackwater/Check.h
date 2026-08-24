#pragma once

//
// HRESULT checking.
//
// Almost every D3D11 and DXGI entry point reports failure by returning an
// HRESULT rather than throwing. Ignoring one does not stop the program -- it
// leaves you holding a null interface pointer, and the crash surfaces several
// calls later somewhere unrelated to the actual mistake.
//
// So: every call is checked, and a failure names the exact call that failed.
//

#include <windows.h>    // HRESULT, FAILED()
#include <stdexcept>    // std::runtime_error

namespace bw
{
    /// Thrown by BW_CHECK when a call returns a failing HRESULT.
    ///
    /// Derives from std::runtime_error so the formatted, human-readable
    /// message is reachable through the standard what(). The raw code is kept
    /// alongside it because a few failures are worth handling specifically --
    /// DXGI_ERROR_DEVICE_REMOVED being the one you will actually meet.
    class HResultError final : public std::runtime_error
    {
    public:
        HResultError(HRESULT hr, const char* expression, const char* file, int line);

        [[nodiscard]] HRESULT code() const noexcept { return m_hr; }

    private:
        HRESULT m_hr;
    };

    /// Implementation detail of BW_CHECK.
    ///
    /// Throws HResultError when hr indicates failure; returns normally
    /// otherwise. Call it through the macro so the source context is filled
    /// in for you.
    void ThrowIfFailed(HRESULT hr, const char* expression, const char* file, int line);

    /// Implementation detail of BW_CHECK_WIN32.
    ///
    /// Reads the calling thread's last-error code, converts it into the
    /// HRESULT space, and throws. Always throws.
    [[noreturn]] void ThrowLastError(const char* expression, const char* file, int line);
}

///
/// Check an HRESULT-returning call:
///
///     BW_CHECK(device->CreateTexture2D(&desc, nullptr, &texture));
///
/// On failure this throws, reporting the failing expression verbatim along
/// with its file, line, and the system's description of the error code.
///
/// This has to be a macro: #expr, __FILE__ and __LINE__ are resolved by the
/// preprocessor at the call site. A plain function would only ever see the
/// resulting HRESULT value, with no idea what produced it.
///
/// Macros have no namespace -- they are raw text substitution -- so the BW_
/// prefix is what keeps this from colliding with every other library's CHECK.
///
#define BW_CHECK(expr) ::bw::ThrowIfFailed((expr), #expr, __FILE__, __LINE__)

///
/// Check a classic Win32 call -- one that reports failure by returning zero,
/// NULL or FALSE and leaving the reason in the thread's last-error code:
///
///     BW_CHECK_WIN32(::RegisterClassExW(&wc));
///
/// The do/while(false) wrapper is not decoration. Without it,
///
///     if (cond) BW_CHECK_WIN32(x); else ...
///
/// would break, because the macro expands to more than one statement and the
/// `else` would bind to the wrong `if`. Wrapping in do/while makes the whole
/// thing a single statement that still requires its trailing semicolon.
///
#define BW_CHECK_WIN32(expr)                                        \
    do {                                                            \
        if (!(expr))                                                \
        {                                                           \
            ::bw::ThrowLastError(#expr, __FILE__, __LINE__);        \
        }                                                           \
    } while (false)
