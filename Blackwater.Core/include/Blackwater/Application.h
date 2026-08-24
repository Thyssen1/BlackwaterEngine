#pragma once

//
// The engine's entry point: a window, a device, and a fixed-timestep loop.
//
// Derive from this, override OnUpdate/OnRender, call Run().
//

#include "Blackwater/GraphicsDevice.h"
#include "Blackwater/Window.h"

#include <cstdint>

namespace bw
{
    class Application
    {
    public:
        struct Config
        {
            const wchar_t* title  = L"Blackwater";
            uint32_t       width  = 1280;
            uint32_t       height = 720;

            /// Simulation rate. Fixed and independent of the display's
            /// refresh rate or how fast frames actually render.
            double updatesPerSecond = 60.0;

            bool vsync = true;

            float clearColour[4] = { 0.05f, 0.07f, 0.10f, 1.0f };
        };

        explicit Application(const Config& config = Config{});

        // Virtual, because this class is meant to be derived from and will be
        // deleted through a base pointer. A non-virtual destructor there is
        // undefined behaviour -- the derived part simply never gets destroyed,
        // silently, with no warning from the compiler.
        virtual ~Application() = default;

        Application(const Application&)            = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&)                 = delete;
        Application& operator=(Application&&)      = delete;

        /// Runs until the window closes. Returns the process exit code.
        int Run();

    protected:
        /// Fixed-rate simulation step.
        ///
        /// Called zero or more times per frame, always with exactly the same
        /// delta. That constancy is the whole point: simulation results stop
        /// depending on how fast the machine happens to be running.
        virtual void OnUpdate(double /*fixedDeltaSeconds*/) {}

        /// Variable-rate rendering.
        ///
        /// alpha is in [0, 1): how far the current moment sits between the
        /// previous simulation state and the latest one. Interpolating
        /// positions by alpha is what keeps a 60 Hz simulation looking smooth
        /// on a 144 Hz display.
        virtual void OnRender(double /*alpha*/) {}

        [[nodiscard]] Window&         GetWindow()   noexcept { return m_window; }
        [[nodiscard]] GraphicsDevice& GetGraphics() noexcept { return m_graphics; }

    private:
        Config m_config;

        // Declaration order is load-bearing. Members are constructed in the
        // order they are declared here -- NOT the order of the constructor's
        // initialiser list -- and GraphicsDevice needs a window that already
        // exists. Swap these two lines and it gets a null HWND, while the
        // constructor still reads as though it were correct.
        Window         m_window;
        GraphicsDevice m_graphics;
    };
}
