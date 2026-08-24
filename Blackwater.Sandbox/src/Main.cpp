#include <Blackwater/Application.h>
#include <Blackwater/Check.h>

#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <exception>

namespace
{
    //
    // M0 sandbox.
    //
    // Nothing is drawn yet -- the point here is to make the loop's behaviour
    // visible. The title bar reports frames per second and updates per
    // second separately, which is the whole argument for a fixed timestep:
    // ups should sit rock-steady at 60 while fps follows the display, the
    // window size, and whatever else the machine is doing.
    //
    class SandboxApp final : public bw::Application
    {
    public:
        using bw::Application::Application;   // reuse the base constructors

    protected:
        void OnUpdate(double fixedDeltaSeconds) override
        {
            ++m_updates;

            // Summing the fixed delta rather than reading a clock: these
            // steps are exactly 1/60 s each by construction, so this is an
            // exact one-second window with no drift.
            m_secondAccumulator += fixedDeltaSeconds;

            if (m_secondAccumulator >= 1.0)
            {
                wchar_t title[128];
                ::swprintf_s(title,
                             L"Blackwater - M0   |   %u fps   |   %u ups",
                             m_frames, m_updates);

                GetWindow().SetTitle(title);

                m_secondAccumulator -= 1.0;
                m_frames  = 0;
                m_updates = 0;
            }
        }

        void OnRender(double /*alpha*/) override
        {
            ++m_frames;

            // M1 puts geometry here, and alpha starts being used to
            // interpolate between the last two simulation states.
        }

    private:
        double   m_secondAccumulator = 0.0;
        uint32_t m_frames            = 0;
        uint32_t m_updates           = 0;
    };
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    try
    {
        // C++20 designated initialisers -- the closest thing C++ has to a C#
        // object initialiser. Fields must appear in declaration order.
        SandboxApp app(bw::Application::Config{
            .title  = L"Blackwater - M0",
            .width  = 1280,
            .height = 720,
        });

        return app.Run();
    }
    catch (const bw::HResultError& e)
    {
        ::MessageBoxA(nullptr, e.what(), "Blackwater - HRESULT failure",
                      MB_OK | MB_ICONERROR);
        return 1;
    }
    catch (const std::exception& e)
    {
        ::MessageBoxA(nullptr, e.what(), "Blackwater - fatal error",
                      MB_OK | MB_ICONERROR);
        return 1;
    }
}
