#include "Blackwater/Application.h"

#include <chrono>

namespace
{
    /// Longest frame the accumulator will accept, in seconds.
    ///
    /// Anything slower than this is treated as a stall rather than as real
    /// elapsed time. Without the clamp, pausing on a breakpoint for ten
    /// seconds would demand 600 catch-up updates in a single frame; those
    /// take longer than a frame to run, so the next frame is worse, and the
    /// application never recovers. That is the "spiral of death", and this
    /// single line is the whole defence against it.
    constexpr double kMaxFrameSeconds = 0.25;
}

namespace bw
{
    Application::Application(const Config& config)
        : m_config(config)
        , m_window(config.title, config.width, config.height)
        , m_graphics(m_window.Handle(), m_window.Width(), m_window.Height())
    {
        // Note this reads from `config`, the parameter, rather than from
        // m_config. Both work here, but depending on m_config would mean
        // depending on it having been initialised first -- a fragile thing to
        // rely on when someone later reorders the members.
    }

    int Application::Run()
    {
        // steady_clock, not system_clock: it is monotonic, so changing the
        // system time (or a DST rollover) cannot make it run backwards and
        // hand us a negative frame time. MSVC implements it on
        // QueryPerformanceCounter, so the precision is the same as doing that
        // by hand, without the boilerplate.
        using Clock = std::chrono::steady_clock;

        const double fixedDelta = 1.0 / m_config.updatesPerSecond;

        double accumulator = 0.0;
        auto   previous    = Clock::now();

        while (m_window.PumpMessages())
        {
            const auto current = Clock::now();

            // duration<double> converts the clock's integer ticks into
            // seconds without the truncation that duration_cast would apply.
            double frameSeconds =
                std::chrono::duration<double>(current - previous).count();

            previous = current;

            if (frameSeconds > kMaxFrameSeconds)
            {
                frameSeconds = kMaxFrameSeconds;
            }

            // -------------------------------------------------------------
            // Simulate.
            //
            // Real elapsed time goes into the accumulator; whole fixed steps
            // come out. Depending on how long the last frame took this runs
            // zero times (fast frame), once (typical), or several times
            // (after a hitch) -- but every call sees exactly the same delta.
            // -------------------------------------------------------------
            accumulator += frameSeconds;

            while (accumulator >= fixedDelta)
            {
                OnUpdate(fixedDelta);
                accumulator -= fixedDelta;
            }

            // Resize between simulation and rendering, so the frame we are
            // about to draw already targets the new backbuffer size.
            if (m_window.ConsumeResized())
            {
                m_graphics.Resize(m_window.Width(), m_window.Height());
            }

            // A minimised window has no drawable surface. Keep simulating,
            // but skip presenting and yield rather than spinning: with vsync
            // gone there is nothing left to pace the loop.
            if (m_window.IsMinimized())
            {
                ::Sleep(1);
                continue;
            }

            // -------------------------------------------------------------
            // Render.
            //
            // Whatever time is left in the accumulator is the fraction of a
            // step we are standing past the last simulation state. Rendering
            // positions interpolated by alpha is what stops a 60 Hz
            // simulation from looking stepped on a 144 Hz display.
            // -------------------------------------------------------------
            const double alpha = accumulator / fixedDelta;

            m_graphics.Clear(m_config.clearColour);
            OnRender(alpha);
            m_graphics.Present(m_config.vsync);
        }

        // PumpMessages returned false, meaning WM_QUIT. Everything unwinds
        // from here: m_graphics then m_window, in reverse declaration order,
        // each releasing what it owns.
        return 0;
    }
}
