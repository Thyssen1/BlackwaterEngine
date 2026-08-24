#include "Blackwater/GraphicsDevice.h"
#include "Blackwater/Check.h"

namespace
{
    // The flip model requires a plain (non-sRGB) format on the swap chain
    // itself. If gamma-correct output is wanted later, the buffer stays
    // R8G8B8A8_UNORM and an _SRGB render-target *view* is created over it.
    constexpr DXGI_FORMAT kBackBufferFormat  = DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr DXGI_FORMAT kDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
}

namespace bw
{
    GraphicsDevice::GraphicsDevice(HWND hwnd, uint32_t width, uint32_t height)
        : m_width(width)
        , m_height(height)
    {
        CreateDevice();
        CreateSwapChain(hwnd);
        CreateBackBufferViews();
    }

    GraphicsDevice::~GraphicsDevice()
    {
        // Not strictly required -- the ComPtrs below release everything on
        // their own. But the immediate context can hold references to
        // resources it still has bound, and unbinding first makes the debug
        // layer's live-object report at shutdown mean what it says.
        if (m_context)
        {
            m_context->ClearState();
            m_context->Flush();
        }

        // No Release() calls. Five ComPtr members go out of scope here and
        // each one releases exactly once, in reverse declaration order.
    }

    void GraphicsDevice::CreateDevice()
    {
        UINT flags = 0;

#ifdef BW_DEBUG
        // Turns on D3D11's validation: every invalid argument, every leaked
        // object, reported to the debugger output window. Costs performance,
        // which is why it is gated on the Debug configuration.
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        // 11_1 first, 11_0 as fallback. D3D picks the highest the hardware
        // actually supports.
        constexpr D3D_FEATURE_LEVEL levels[] =
        {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };

        const auto create = [&](UINT createFlags)
        {
            return ::D3D11CreateDevice(
                nullptr,                    // default adapter
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,                    // no software rasteriser module
                createFlags,
                levels,
                ARRAYSIZE(levels),
                D3D11_SDK_VERSION,
                m_device.ReleaseAndGetAddressOf(),
                nullptr,                    // obtained feature level: not needed yet
                m_context.ReleaseAndGetAddressOf());
        };

        HRESULT hr = create(flags);

#ifdef BW_DEBUG
        // The debug layer lives in the optional "Graphics Tools" Windows
        // feature. On a machine without it, device creation fails outright --
        // which would be a baffling way for the engine to refuse to start.
        // Fall back to a non-debug device instead.
        if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG))
        {
            ::OutputDebugStringW(
                L"[Blackwater] D3D11 debug layer unavailable "
                L"(install the 'Graphics Tools' optional feature). "
                L"Continuing without it.\n");

            flags &= ~static_cast<UINT>(D3D11_CREATE_DEVICE_DEBUG);
            hr = create(flags);
        }
#endif

        BW_CHECK(hr);
    }

    void GraphicsDevice::CreateSwapChain(HWND hwnd)
    {
        // The swap chain is a DXGI object, not a D3D one, and DXGI is reached
        // by walking up from the device it will feed:
        //     device -> IDXGIDevice -> adapter -> factory
        // As() is ComPtr's QueryInterface: same object, different interface,
        // with the refcount handled for us.
        ComPtr<IDXGIDevice> dxgiDevice;
        BW_CHECK(m_device.As(&dxgiDevice));

        ComPtr<IDXGIAdapter> adapter;
        BW_CHECK(dxgiDevice->GetAdapter(adapter.GetAddressOf()));

        ComPtr<IDXGIFactory2> factory;
        BW_CHECK(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf())));

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width       = m_width;
        desc.Height      = m_height;
        desc.Format      = kBackBufferFormat;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

        // Flip model constraints, all mandatory:
        //   - at least two buffers
        //   - no multisampling on the swap chain itself
        //   - a non-sRGB, flip-compatible format (see kBackBufferFormat)
        desc.BufferCount      = 2;
        desc.SampleDesc.Count = 1;
        desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Scaling          = DXGI_SCALING_STRETCH;
        desc.AlphaMode        = DXGI_ALPHA_MODE_UNSPECIFIED;

        BW_CHECK(factory->CreateSwapChainForHwnd(
            m_device.Get(),
            hwnd,
            &desc,
            nullptr,     // no fullscreen description: windowed only
            nullptr,     // no output restriction
            m_swapChain.ReleaseAndGetAddressOf()));

        // By default DXGI intercepts Alt+Enter and performs its own fullscreen
        // transition behind the engine's back. Switching modes is the engine's
        // decision, so take that over. (This does not affect Alt+F4.)
        BW_CHECK(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    }

    void GraphicsDevice::CreateBackBufferViews()
    {
        // --- Render target over the swap chain's backbuffer ----------------
        ComPtr<ID3D11Texture2D> backBuffer;
        BW_CHECK(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())));

        BW_CHECK(m_device->CreateRenderTargetView(
            backBuffer.Get(),
            nullptr,     // null desc = inherit the buffer's own format
            m_renderTargetView.ReleaseAndGetAddressOf()));

        // --- Depth-stencil buffer -----------------------------------------
        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width            = m_width;
        depthDesc.Height           = m_height;
        depthDesc.MipLevels        = 1;
        depthDesc.ArraySize        = 1;
        depthDesc.Format           = kDepthStencilFormat;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage            = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

        ComPtr<ID3D11Texture2D> depthTexture;
        BW_CHECK(m_device->CreateTexture2D(&depthDesc, nullptr, depthTexture.GetAddressOf()));

        BW_CHECK(m_device->CreateDepthStencilView(
            depthTexture.Get(),
            nullptr,
            m_depthStencilView.ReleaseAndGetAddressOf()));

        // backBuffer and depthTexture are local and die at the closing brace.
        // The textures themselves survive: the views hold their own
        // references. This is refcounting doing exactly what it should, and
        // it is why local ComPtrs here are correct rather than a leak.
    }

    void GraphicsDevice::Resize(uint32_t width, uint32_t height)
    {
        // Minimising reports 0x0, which is not a legal buffer size.
        if (width == 0 || height == 0)
        {
            return;
        }

        if (width == m_width && height == m_height)
        {
            return;
        }

        // ResizeBuffers fails with DXGI_ERROR_INVALID_CALL while anything
        // still references the backbuffer -- including the context's own
        // bound render target. Unbind, drop our views, then flush so the
        // driver has genuinely let go.
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_renderTargetView.Reset();
        m_depthStencilView.Reset();
        m_context->Flush();

        BW_CHECK(m_swapChain->ResizeBuffers(
            0,                     // keep the existing buffer count
            width,
            height,
            DXGI_FORMAT_UNKNOWN,   // keep the existing format
            0));

        m_width  = width;
        m_height = height;

        CreateBackBufferViews();
    }

    void GraphicsDevice::Clear(const float colourRGBA[4])
    {
        ID3D11RenderTargetView* const targets[] = { m_renderTargetView.Get() };
        m_context->OMSetRenderTargets(1, targets, m_depthStencilView.Get());

        // The viewport maps normalised device coordinates onto pixels. It is
        // reset whenever the backbuffer is recreated, so it is set per frame
        // rather than once at startup.
        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width    = static_cast<float>(m_width);
        viewport.Height   = static_cast<float>(m_height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &viewport);

        m_context->ClearRenderTargetView(m_renderTargetView.Get(), colourRGBA);

        // Depth clears to 1.0 -- the far plane -- so that any geometry drawn
        // later is nearer than "nothing" and passes the depth test.
        m_context->ClearDepthStencilView(
            m_depthStencilView.Get(),
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
            1.0f,
            0);
    }

    void GraphicsDevice::Present(bool vsync)
    {
        const HRESULT hr = m_swapChain->Present(vsync ? 1u : 0u, 0);

        // A removed or reset device means the GPU went away -- driver update,
        // TDR, hardware change. Recovering means rebuilding everything, which
        // is beyond M0; what matters here is that it is reported rather than
        // silently swallowed.
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            BW_CHECK(m_device->GetDeviceRemovedReason());
        }

        BW_CHECK(hr);
    }
}
