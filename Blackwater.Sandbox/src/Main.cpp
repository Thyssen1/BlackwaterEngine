#include <Blackwater/Application.h>
#include <Blackwater/Camera.h>
#include <Blackwater/Check.h>
#include <Blackwater/ConstantBuffer.h>
#include <Blackwater/Mesh.h>
#include <Blackwater/Shader.h>

#include <DirectXMath.h>
#include <windows.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iterator>

namespace
{
    using namespace DirectX;

    // ----------------------------------------------------------------------
    // Vertex format
    //
    // This struct and kInputLayout below must agree exactly. If they drift,
    // CreateInputLayout fails at startup rather than drawing nonsense -- one
    // of the few mistakes D3D11 catches for you.
    // ----------------------------------------------------------------------
    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT4 colour;
    };

    const D3D11_INPUT_ELEMENT_DESC kInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,
          offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },

        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          offsetof(Vertex, colour),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // ----------------------------------------------------------------------
    // Constant buffer payload
    //
    // 64 bytes -- one 4x4 matrix -- so it already satisfies the multiple-of-16
    // rule that ConstantBuffer<T> asserts on.
    // ----------------------------------------------------------------------
    struct Transforms
    {
        XMFLOAT4X4 worldViewProjection;
    };

    // ----------------------------------------------------------------------
    // A unit cube: eight corners, each a different colour, so the rasterizer's
    // interpolation is obvious.
    //
    //   0..3 = top    (y = +1)
    //   4..7 = bottom (y = -1)
    // ----------------------------------------------------------------------
    const Vertex kVertices[] =
    {
        { { -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { {  1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } },
        { { -1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { -1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },
        { {  1.0f, -1.0f,  1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    // Winding order matters. D3D11's default rasterizer culls back faces and
    // treats CLOCKWISE (in screen space) as front-facing. Reverse a triangle
    // here and that face simply vanishes -- a "hole" in the cube that looks
    // like a depth bug but is not.
    const uint32_t kIndices[] =
    {
        3, 1, 0,   2, 1, 3,     // top
        0, 5, 4,   1, 5, 0,     // front
        3, 4, 7,   0, 4, 3,     // left
        1, 6, 5,   2, 6, 1,     // right
        2, 7, 6,   3, 7, 2,     // back
        6, 4, 5,   7, 4, 6,     // bottom
    };

    constexpr double kRotationsPerSecond = 0.15;
    constexpr double kRadiansPerSecond   = kRotationsPerSecond * 2.0 * 3.14159265358979323846;

    // Camera controls. Middle-drag and Left/Right both produce a yaw delta;
    // wheel and Up/Down both produce zoom "notches". One path per axis means
    // the two bindings cannot disagree about direction or feel.
    constexpr float kRotateRadiansPerPixel   = 0.008f;
    constexpr float kRotateRadiansPerSecond  = 2.0f;
    constexpr float kZoomFactorPerNotch      = 0.88f;   // < 1: each notch moves 12% closer
    constexpr float kZoomNotchesPerSecond    = 6.0f;    // Up/Down held

    //
    // M1: a spinning cube.
    //
    class SandboxApp final : public bw::Application
    {
    public:
        explicit SandboxApp(const Config& config)
            : bw::Application(config)
            // Base classes are fully constructed before derived members are,
            // so GetGraphics() is already valid here. That is what lets these
            // be plain members rather than unique_ptrs, despite being
            // non-movable.
            , m_shader(GetGraphics().Device(), L"shaders\\Basic.hlsl",
                       "VSMain", "PSMain", kInputLayout)
            , m_mesh(GetGraphics().Device(),
                     kVertices, std::size(kVertices), sizeof(Vertex),
                     kIndices)
            , m_transforms(GetGraphics().Device())
        {
        }

    protected:
        void OnUpdate(double fixedDeltaSeconds) override
        {
            UpdateCamera(static_cast<float>(fixedDeltaSeconds));

            // Keep the previous angle so OnRender can interpolate between the
            // two. Without this there is nothing to interpolate *from*, and
            // alpha would be useless.
            m_previousAngle = m_currentAngle;
            m_currentAngle += static_cast<float>(kRadiansPerSecond * fixedDeltaSeconds);

            ++m_updates;
            m_secondAccumulator += fixedDeltaSeconds;

            if (m_secondAccumulator >= 1.0)
            {
                wchar_t title[160];
                ::swprintf_s(title,
                             L"Blackwater - M2   |   %u fps   |   %u ups   |   debug layer: %s",
                             m_frames, m_updates,
                             GetGraphics().IsDebugLayerActive() ? L"ON" : L"off");

                GetWindow().SetTitle(title);

                m_secondAccumulator -= 1.0;
                m_frames  = 0;
                m_updates = 0;
            }
        }

        void OnRender(double alpha) override
        {
            ++m_frames;

            // The payoff for the accumulator. The simulation advances 60 times
            // a second; this draws the cube *between* those states, so motion
            // stays smooth at any refresh rate. Replace `angle` with
            // m_currentAngle to see the judder alpha exists to remove.
            const float angle =
                m_previousAngle + (m_currentAngle - m_previousAngle) * static_cast<float>(alpha);

            auto& graphics = GetGraphics();

            const float aspect =
                static_cast<float>(graphics.Width()) / static_cast<float>(graphics.Height());

            const XMMATRIX world = XMMatrixRotationY(angle) * XMMatrixRotationX(angle * 0.5f);

            // The camera interpolates its own pose by alpha, the same way the
            // cube's angle is interpolated above.
            const XMMATRIX view       = m_camera.View(static_cast<float>(alpha));
            const XMMATRIX projection = m_camera.Projection(aspect);

            // Row-vector convention: a vertex flows world -> view -> clip, so
            // the matrices multiply in that order.
            //
            // XMMATRIX is 16-byte-aligned SIMD and belongs on the stack for
            // computation; XMFLOAT4X4 is plain floats and is what gets stored
            // and uploaded. Transposing here is what makes HLSL's column-major
            // packing line up with DirectXMath's row-major storage.
            Transforms transforms{};
            XMStoreFloat4x4(&transforms.worldViewProjection,
                            XMMatrixTranspose(world * view * projection));

            auto* context = graphics.Context();

            m_transforms.Update(context, transforms);
            m_transforms.BindToVertexStage(context, 0);

            m_shader.Bind(context);
            m_mesh.Draw(context);
        }

    private:
        void UpdateCamera(float deltaSeconds)
        {
            const bw::Input& input = GetWindow().GetInput();

            // Previous pose first, then change the current one -- the same
            // order as the cube's angle.
            m_camera.BeginStep();

            // --- Rotate: middle-drag and Left/Right, one yaw delta -----------
            float yawDelta = 0.0f;

            if (input.IsButtonDown(bw::MouseButton::Middle))
            {
                yawDelta += static_cast<float>(input.MouseDeltaX()) * kRotateRadiansPerPixel;
            }
            if (input.IsKeyDown(VK_RIGHT)) { yawDelta += kRotateRadiansPerSecond * deltaSeconds; }
            if (input.IsKeyDown(VK_LEFT))  { yawDelta -= kRotateRadiansPerSecond * deltaSeconds; }

            m_camera.Rotate(yawDelta);

            // --- Zoom: wheel and Up/Down, one notch count --------------------
            float zoomNotches = input.WheelNotches();

            if (input.IsKeyDown(VK_UP))   { zoomNotches += kZoomNotchesPerSecond * deltaSeconds; }
            if (input.IsKeyDown(VK_DOWN)) { zoomNotches -= kZoomNotchesPerSecond * deltaSeconds; }

            // Positive notches (wheel away from you, or Up) zoom in. pow turns
            // a notch count into a ratio, so two notches are exactly one notch
            // applied twice, whatever the current distance.
            if (zoomNotches != 0.0f)
            {
                m_camera.Zoom(std::pow(kZoomFactorPerNotch, zoomNotches));
            }
        }

        bw::Camera                     m_camera;
        bw::Shader                     m_shader;
        bw::Mesh                       m_mesh;
        bw::ConstantBuffer<Transforms> m_transforms;

        float m_previousAngle = 0.0f;
        float m_currentAngle  = 0.0f;

        double   m_secondAccumulator = 0.0;
        uint32_t m_frames            = 0;
        uint32_t m_updates           = 0;
    };
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    try
    {
        SandboxApp app(bw::Application::Config{
            .title  = L"Blackwater - M2",
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
        // HLSL compile errors arrive here, carrying the compiler's own
        // diagnostics with line numbers.
        ::MessageBoxA(nullptr, e.what(), "Blackwater - fatal error",
                      MB_OK | MB_ICONERROR);
        return 1;
    }
}
