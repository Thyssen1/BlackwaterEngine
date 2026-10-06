#pragma once

//
// Keyboard and mouse state, sampled for the fixed-timestep simulation.
//
// Two kinds of state live here:
//
//   level  -- "is it held right now?"          IsKeyDown, IsButtonDown
//   edge   -- "did it happen since last step?" WasKeyPressed, WasButtonPressed
//
// Edge state and the mouse/wheel deltas are cleared by EndUpdate(), which the
// Application calls once after *each* OnUpdate -- not once per rendered frame.
// A fast frame can run zero updates; clearing per frame would silently drop a
// click that arrived during it.
//
// Input knows nothing about Win32 messages. Window decodes those and calls the
// On* feeders below, which keeps this a plain state object.
//

#include <array>
#include <cstdint>

namespace bw
{
    enum class MouseButton : uint8_t
    {
        Left,
        Right,
        Middle,
        Count
    };

    class Input final
    {
    public:
        // ------------------------------------------------------------------
        // Queries -- read these from OnUpdate.
        // ------------------------------------------------------------------

        /// virtualKey is a Win32 VK_ code (VK_LEFT, 'Q', ...).
        [[nodiscard]] bool IsKeyDown(uint8_t virtualKey) const noexcept;
        [[nodiscard]] bool WasKeyPressed(uint8_t virtualKey) const noexcept;

        [[nodiscard]] bool IsButtonDown(MouseButton button) const noexcept;
        [[nodiscard]] bool WasButtonPressed(MouseButton button) const noexcept;
        [[nodiscard]] bool WasButtonReleased(MouseButton button) const noexcept;
        [[nodiscard]] bool AnyButtonDown() const noexcept;

        /// Cursor position in client-area pixels, origin top-left.
        [[nodiscard]] int32_t MouseX() const noexcept { return m_mouseX; }
        [[nodiscard]] int32_t MouseY() const noexcept { return m_mouseY; }

        /// Cursor movement since the last update, in pixels.
        [[nodiscard]] int32_t MouseDeltaX() const noexcept { return m_mouseDeltaX; }
        [[nodiscard]] int32_t MouseDeltaY() const noexcept { return m_mouseDeltaY; }

        /// Wheel movement since the last update, in notches. Positive is
        /// away from the user. Fractional on high-resolution wheels.
        [[nodiscard]] float WheelNotches() const noexcept;

        // ------------------------------------------------------------------
        // Feeders -- called by Window as messages arrive.
        // ------------------------------------------------------------------

        void OnKey(uint8_t virtualKey, bool down) noexcept;
        void OnMouseButton(MouseButton button, bool down) noexcept;
        void OnMouseMove(int32_t x, int32_t y) noexcept;
        void OnMouseWheel(int32_t rawDelta) noexcept;

        /// Releases everything. Without it, Alt-Tabbing away while holding a
        /// key sends its WM_KEYUP to another window and the key stays "down".
        void OnFocusLost() noexcept;

        /// Clears edge state and deltas. Called after each simulation step.
        void EndUpdate() noexcept;

    private:
        static constexpr size_t kKeyCount    = 256;
        static constexpr size_t kButtonCount = static_cast<size_t>(MouseButton::Count);

        static constexpr size_t Index(MouseButton button) noexcept
        {
            return static_cast<size_t>(button);
        }

        std::array<bool, kKeyCount> m_keysDown{};
        std::array<bool, kKeyCount> m_keysPressed{};

        std::array<bool, kButtonCount> m_buttonsDown{};
        std::array<bool, kButtonCount> m_buttonsPressed{};
        std::array<bool, kButtonCount> m_buttonsReleased{};

        int32_t m_mouseX      = 0;
        int32_t m_mouseY      = 0;
        int32_t m_mouseDeltaX = 0;
        int32_t m_mouseDeltaY = 0;
        bool    m_hasMousePosition = false;

        int32_t m_wheelRaw = 0;
    };
}
