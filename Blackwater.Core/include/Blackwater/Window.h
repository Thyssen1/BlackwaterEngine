#pragma once

//
// A Win32 window, owned as a resource.
//
// The window exists for as long as this object does. There is no Close() to
// forget to call and no Dispose() to skip -- destruction is the destructor.
//

#include <windows.h>
#include <cstdint>

namespace bw
{
    class Window final
    {
    public:
        /// Creates and shows a window whose *client area* is width x height.
        /// Throws HResultError if any Win32 call fails.
        Window(const wchar_t* title, uint32_t width, uint32_t height);
        ~Window();

        // An HWND is owned, not shared. Copying would give two objects the
        // same handle and destroy it twice; moving would leave the window's
        // GWLP_USERDATA pointing at the object we moved from. Neither is
        // worth supporting, so both are deleted -- and deleting them makes
        // the mistake a compile error rather than a crash.
        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&)                 = delete;
        Window& operator=(Window&&)      = delete;

        /// Drains every message currently queued for this thread.
        ///
        /// Returns false once WM_QUIT has been seen, which is the signal to
        /// leave the game loop. Never blocks: an empty queue returns true
        /// immediately so the caller can go and render a frame.
        [[nodiscard]] bool PumpMessages();

        /// True exactly once after the client area changed size. Reading it
        /// clears it, so the loop reacts to a burst of WM_SIZE messages a
        /// single time per frame instead of rebuilding the swap chain on
        /// every mouse move during a border drag.
        [[nodiscard]] bool ConsumeResized() noexcept;

        /// Replaces the text in the title bar.
        void SetTitle(const wchar_t* title) noexcept;

        [[nodiscard]] HWND     Handle()      const noexcept { return m_hwnd; }
        [[nodiscard]] uint32_t Width()       const noexcept { return m_width; }
        [[nodiscard]] uint32_t Height()      const noexcept { return m_height; }
        [[nodiscard]] bool     IsMinimized() const noexcept { return m_minimized; }

    private:
        static void RegisterWindowClass();

        /// The raw Win32 window procedure.
        ///
        /// This is a plain C function pointer: no `this`, no capture, no
        /// context of any kind. Its whole job is to recover the Window* that
        /// owns this HWND and forward to HandleMessage.
        static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

        /// The real message handler, with a `this` to work against.
        LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

        HWND     m_hwnd      = nullptr;
        uint32_t m_width     = 0;
        uint32_t m_height    = 0;
        bool     m_minimized = false;
        bool     m_resized   = false;
    };
}
