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
#include <vector>

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

    // PORT(M5): the one D3D type left in game-side code. Becomes an engine
    // vertex-layout description that each backend translates.
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

    // ----------------------------------------------------------------------
    // Ground grid
    // ----------------------------------------------------------------------
    constexpr int   kGridHalfExtent = 10;   // lines from -10 to +10
    constexpr float kGridY          = 0.0f;

    const XMFLOAT4 kGridColour  = { 0.22f, 0.26f, 0.30f, 1.0f };
    const XMFLOAT4 kAxisXColour = { 0.60f, 0.22f, 0.22f, 1.0f };   // reddish: the X axis
    const XMFLOAT4 kAxisZColour = { 0.22f, 0.32f, 0.60f, 1.0f };   // bluish:  the Z axis

    //
    // Builds the grid as a LINELIST: every two indices are one segment.
    //
    // Returns a Mesh by value even though Mesh is non-copyable and
    // non-movable. Since C++17, `return Mesh(...)` does not create a temporary
    // to be copied or moved -- the object is constructed directly in the
    // caller's storage ("guaranteed copy elision"). That is what lets a
    // factory function produce an immovable type.
    //
    bw::Mesh MakeGrid(ID3D11Device* device)
    {
        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;

        const auto addLine = [&](XMFLOAT3 from, XMFLOAT3 to, XMFLOAT4 colour)
        {
            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({ from, colour });
            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({ to, colour });
        };

        const auto extent = static_cast<float>(kGridHalfExtent);

        for (int i = -kGridHalfExtent; i <= kGridHalfExtent; ++i)
        {
            const auto offset = static_cast<float>(i);

            // Line running along X, at z = offset. The one through the origin
            // is the X axis.
            addLine({ -extent, kGridY, offset }, { extent, kGridY, offset },
                    i == 0 ? kAxisXColour : kGridColour);

            // Line running along Z, at x = offset. The one through the origin
            // is the Z axis.
            addLine({ offset, kGridY, -extent }, { offset, kGridY, extent },
                    i == 0 ? kAxisZColour : kGridColour);
        }

        // The vectors die when this function returns. That is fine: Mesh
        // copied their contents into IMMUTABLE GPU buffers in its constructor.
        return bw::Mesh(device,
                        vertices.data(), vertices.size(), sizeof(Vertex),
                        indices,
                        D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    }

    constexpr float kCubeHalfSize   = 1.0f;    // the cube mesh spans -1..+1
    constexpr float kMarkerScale    = 0.25f;

    //
    // M2: an orbit camera over a ground grid. The cube spins on the ground;
    // right-click places a marker where the cursor's ray meets the ground.
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
            , m_cube(GetGraphics().Device(),
                     kVertices, std::size(kVertices), sizeof(Vertex),
                     kIndices)
            , m_grid(MakeGrid(GetGraphics().Device()))
            , m_transforms(GetGraphics().Device())
        {
        }

    protected:
        void OnUpdate(double fixedDeltaSeconds) override
        {
            UpdateCamera(static_cast<float>(fixedDeltaSeconds));
            UpdateMarker();

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

            // The camera interpolates its own pose by alpha, the same way the
            // cube's angle is interpolated above. View and projection are the
            // same for every object this frame, so combine them once.
            const XMMATRIX viewProjection =
                m_camera.View(static_cast<float>(alpha)) * m_camera.Projection(aspect);

            // Shared by every draw below: one shader, one constant buffer
            // slot. Only the buffer's *contents* change per draw.
            auto* context = graphics.Context();
            m_shader.Bind(context);
            m_transforms.BindToVertexStage(context, 0);

            // Ground: the grid's vertices are already in world space.
            DrawMesh(m_grid, XMMatrixIdentity(), viewProjection);

            // Cube: spin around Y, then lift by its half-height so its bottom
            // face rests on the ground instead of being sunk halfway in.
            DrawMesh(m_cube,
                     XMMatrixRotationY(angle) * XMMatrixTranslation(0.0f, kCubeHalfSize, 0.0f),
                     viewProjection);

            // Marker: the same cube mesh, scaled down and moved to the last
            // right-click. One set of vertices, two objects -- "where and how
            // big" lives in the world matrix, not in the geometry.
            if (m_hasMarker)
            {
                DrawMesh(m_cube,
                         XMMatrixScaling(kMarkerScale, kMarkerScale, kMarkerScale) *
                         XMMatrixTranslation(m_marker.x,
                                             m_marker.y + kCubeHalfSize * kMarkerScale,
                                             m_marker.z),
                         viewProjection);
            }
        }

    private:
        //
        // Uploads one object's transform and draws it.
        //
        // Rewriting the same constant buffer before every draw is safe because
        // ConstantBuffer::Update maps with WRITE_DISCARD: each Map hands back
        // fresh memory ("renaming"), so a draw already queued keeps reading
        // the matrix it was issued with.
        //
        // FXMMATRIX / CXMMATRIX are DirectXMath's parameter types for
        // XM_CALLCONV functions: the first matrix travels in registers, later
        // ones by reference. Using them is the idiom; the compiler would
        // accept plain `const XMMATRIX&` too.
        //
        void XM_CALLCONV DrawMesh(const bw::Mesh& mesh, FXMMATRIX world, CXMMATRIX viewProjection)
        {
            // Row-vector convention: vertex -> world -> view -> clip, so the
            // matrices multiply in that order. Transposed because HLSL packs
            // float4x4 column-major while DirectXMath stores row-major.
            Transforms transforms{};
            XMStoreFloat4x4(&transforms.worldViewProjection,
                            XMMatrixTranspose(world * viewProjection));

            auto* context = GetGraphics().Context();
            m_transforms.Update(context, transforms);
            mesh.Draw(context);
        }

        //
        // Right-click: cast a ray from the cursor and drop the marker where it
        // meets the ground. This is exactly the query right-click-to-move will
        // make at M5 -- only the response changes.
        //
        void UpdateMarker()
        {
            const bw::Input& input = GetWindow().GetInput();

            // WasButtonPressed, not IsButtonDown: one click, one placement.
            // Holding the button should not re-place it every step.
            if (!input.WasButtonPressed(bw::MouseButton::Right))
            {
                return;
            }

            const auto& graphics = GetGraphics();

            // Mouse coordinates are client-area pixels and the viewport covers
            // the whole client area, so the two line up directly.
            const bw::Ray ray = m_camera.ScreenToWorldRay(
                static_cast<float>(input.MouseX()),
                static_cast<float>(input.MouseY()),
                static_cast<float>(graphics.Width()),
                static_cast<float>(graphics.Height()));

            XMFLOAT3 hit{};
            if (bw::IntersectGroundPlane(ray, kGridY, hit))
            {
                m_marker    = hit;
                m_hasMarker = true;
            }
            // No hit -- the ray points at the sky. Leave the marker where it is.
        }

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
        bw::Mesh                       m_cube;
        bw::Mesh                       m_grid;
        bw::ConstantBuffer<Transforms> m_transforms;

        XMFLOAT3 m_marker{};
        bool     m_hasMarker = false;

        float m_previousAngle = 0.0f;
        float m_currentAngle  = 0.0f;

        double   m_secondAccumulator = 0.0;
        uint32_t m_frames            = 0;
        uint32_t m_updates           = 0;
    };
}

// PORT(M5): wWinMain and MessageBoxA are Win32. SDL3 supplies a portable main()
// and SDL_ShowSimpleMessageBox for the error reports below.
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
