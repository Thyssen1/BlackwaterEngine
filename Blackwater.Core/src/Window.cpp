#include "Blackwater/Window.h"
#include "Blackwater/Check.h"

namespace
{
    constexpr const wchar_t* kWindowClassName = L"BlackwaterWindow";

    /// The module handle of the running executable. Passing nullptr to
    /// GetModuleHandleW asks for "the module that started this process".
    HINSTANCE CurrentInstance() noexcept
    {
        return ::GetModuleHandleW(nullptr);
    }
}

namespace bw
{
    void Window::RegisterWindowClass()
    {
        // A "magic static": the initialiser runs exactly once, and the
        // compiler makes that thread-safe for us. It replaces the usual
        // bool-flag dance, and it is guaranteed by the standard rather than
        // by luck.
        static const bool registered = []
        {
            WNDCLASSEXW wc{};
            wc.cbSize        = sizeof(wc);
            wc.style         = CS_HREDRAW | CS_VREDRAW;  // repaint on either axis resize
            wc.lpfnWndProc   = &Window::WndProcThunk;
            wc.hInstance     = CurrentInstance();
            wc.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = kWindowClassName;

            // hbrBackground is deliberately left null. The swap chain will own
            // every pixel of the client area; letting Windows erase to a brush
            // first would only produce flicker on resize.

            BW_CHECK_WIN32(::RegisterClassExW(&wc));
            return true;
        }();

        (void)registered;
    }

    Window::Window(const wchar_t* title, uint32_t width, uint32_t height)
        : m_width(width)
        , m_height(height)
    {
        RegisterWindowClass();

        constexpr DWORD style = WS_OVERLAPPEDWINDOW;

        // width/height describe the *client* area -- the part we render into.
        // CreateWindowExW sizes the whole window, borders and title bar
        // included, so without this the drawable area would come out smaller
        // than asked for and the backbuffer would not match the window.
        RECT rect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        BW_CHECK_WIN32(::AdjustWindowRect(&rect, style, FALSE));

        m_hwnd = ::CreateWindowExW(
            0,
            kWindowClassName,
            title,
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,              // no parent
            nullptr,              // no menu
            CurrentInstance(),
            this);                // -> arrives as CREATESTRUCTW::lpCreateParams

        // Checked by hand rather than through BW_CHECK_WIN32 so the report
        // names the call that actually failed instead of "m_hwnd != nullptr".
        if (m_hwnd == nullptr)
        {
            ThrowLastError("CreateWindowExW", __FILE__, __LINE__);
        }

        // ShowWindow returns the *previous* visibility, not success or
        // failure, so there is nothing here worth checking.
        ::ShowWindow(m_hwnd, SW_SHOW);
    }

    Window::~Window()
    {
        if (m_hwnd == nullptr)
        {
            return;
        }

        // Detach before destroying. DestroyWindow synchronously sends
        // WM_DESTROY, and the thunk would happily route that into an object
        // whose destructor is already running. Clearing the back-pointer
        // first makes the thunk fall through to DefWindowProcW instead.
        ::SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, 0);
        ::DestroyWindow(m_hwnd);
        m_hwnd = nullptr;

        // Nothing is checked here on purpose: destructors are implicitly
        // noexcept, and throwing out of one while an exception is already
        // unwinding calls std::terminate.
    }

    LRESULT CALLBACK Window::WndProcThunk(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        // WM_NCCREATE is the first message a window receives, and it carries
        // the CREATESTRUCTW holding the pointer we passed to CreateWindowExW.
        // This is the one and only chance to connect the HWND to its C++
        // object, because Win32 offers no other way to give a callback
        // context.
        if (msg == WM_NCCREATE)
        {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            auto* self = static_cast<Window*>(create->lpCreateParams);

            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));

            // Messages arrive before CreateWindowExW returns, so the member
            // has to be valid now rather than after the constructor assigns it.
            self->m_hwnd = hwnd;
        }

        if (auto* self = reinterpret_cast<Window*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA)))
        {
            return self->HandleMessage(msg, wparam, lparam);
        }

        // Messages that arrive before WM_NCCREATE, or after the destructor
        // detached us, land here.
        return ::DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    LRESULT Window::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam)
    {
        switch (msg)
        {
        case WM_SIZE:
        {
            m_minimized = (wparam == SIZE_MINIMIZED);

            const auto width  = static_cast<uint32_t>(LOWORD(lparam));
            const auto height = static_cast<uint32_t>(HIWORD(lparam));

            // Minimising reports a 0x0 client area. A swap chain cannot be
            // that size, so remember that we are minimised but keep the last
            // real dimensions.
            if (!m_minimized && (width != m_width || height != m_height))
            {
                m_width   = width;
                m_height  = height;
                m_resized = true;   // acted on by the loop, not here
            }
            return 0;
        }

        case WM_DESTROY:
            // Posts WM_QUIT, which PumpMessages sees and reports as "stop".
            ::PostQuitMessage(0);
            return 0;

        default:
            return ::DefWindowProcW(m_hwnd, msg, wparam, lparam);
        }
    }

    bool Window::PumpMessages()
    {
        MSG msg{};

        // PeekMessage rather than GetMessage: GetMessage blocks until
        // something arrives, which is correct for a document application and
        // fatal for a game loop. We must come back every frame even when the
        // queue is empty.
        //
        // The null HWND matters too -- WM_QUIT is posted to the *thread*, not
        // to any window, so filtering by window would never see it.
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                return false;
            }

            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }

        return true;
    }

    void Window::SetTitle(const wchar_t* title) noexcept
    {
        // Not checked: this is called every second from the render loop, and
        // a failed title update is not worth tearing the frame down over.
        ::SetWindowTextW(m_hwnd, title);
    }

    bool Window::ConsumeResized() noexcept
    {
        const bool resized = m_resized;
        m_resized = false;
        return resized;
    }
}
