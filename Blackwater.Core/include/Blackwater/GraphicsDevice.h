#pragma once

//
// The D3D11 device and everything tied to the window's backbuffer.
//
// Every COM object here is held in a ComPtr, so there is not a single
// Release() call in the implementation and no cleanup path to get wrong.
//

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>      // IDXGIFactory2 / IDXGISwapChain1 - the flip model
#include <wrl/client.h>   // Microsoft::WRL::ComPtr
#include <cstdint>

namespace bw
{
    // A using-declaration for one name inside our own namespace -- not a
    // `using namespace`. It cannot leak into the global namespace, and it
    // keeps the member declarations below readable.
    using Microsoft::WRL::ComPtr;

    class GraphicsDevice final
    {
    public:
        /// Creates the device, swap chain and backbuffer views for a window.
        /// Throws HResultError if any D3D or DXGI call fails.
        GraphicsDevice(HWND hwnd, uint32_t width, uint32_t height);
        ~GraphicsDevice();

        GraphicsDevice(const GraphicsDevice&)            = delete;
        GraphicsDevice& operator=(const GraphicsDevice&) = delete;
        GraphicsDevice(GraphicsDevice&&)                 = delete;
        GraphicsDevice& operator=(GraphicsDevice&&)      = delete;

        /// Recreates the backbuffer at a new size. Cheap to call with an
        /// unchanged size, and safely ignores a zero dimension (minimising).
        void Resize(uint32_t width, uint32_t height);

        /// Binds the backbuffer and clears colour and depth.
        void Clear(const float colourRGBA[4]);

        /// Shows the finished frame. vsync=true waits for the next refresh.
        void Present(bool vsync);

        [[nodiscard]] ID3D11Device*        Device()  const noexcept { return m_device.Get(); }
        [[nodiscard]] ID3D11DeviceContext* Context() const noexcept { return m_context.Get(); }
        [[nodiscard]] uint32_t             Width()   const noexcept { return m_width; }
        [[nodiscard]] uint32_t             Height()  const noexcept { return m_height; }

    private:
        void CreateDevice();
        void CreateSwapChain(HWND hwnd);
        void CreateBackBufferViews();

        ComPtr<ID3D11Device>           m_device;
        ComPtr<ID3D11DeviceContext>    m_context;
        ComPtr<IDXGISwapChain1>        m_swapChain;
        ComPtr<ID3D11RenderTargetView> m_renderTargetView;
        ComPtr<ID3D11DepthStencilView> m_depthStencilView;

        uint32_t m_width  = 0;
        uint32_t m_height = 0;
    };
}
