#pragma once

//
// A WC3-style orbit camera: looks down at a target from a fixed-ish pitch,
// rotates around it (yaw), and zooms in and out (distance).
//
// The camera stores a *pose* -- yaw and distance -- rather than a matrix.
// Matrices are rebuilt from the pose on demand. That matters for the fixed
// timestep: two view matrices cannot be meaningfully interpolated, but two
// angles and two distances can, so the pose gets the same previous/current +
// alpha treatment as everything else that moves.
//

#include <DirectXMath.h>

namespace bw
{
    /// A half-line in world space. direction is unit length.
    struct Ray
    {
        DirectX::XMFLOAT3 origin;
        DirectX::XMFLOAT3 direction;
    };

    /// Intersects a ray with the horizontal plane y = height.
    ///
    /// Returns false when the ray runs parallel to the plane or points away
    /// from it -- clicking the sky, in practice.
    [[nodiscard]] bool IntersectGroundPlane(const Ray& ray, float height,
                                            DirectX::XMFLOAT3& hit) noexcept;

    class Camera final
    {
    public:
        struct Settings
        {
            float pitchRadians    = DirectX::XMConvertToRadians(55.0f);  // down from horizontal
            float distance        = 12.0f;
            float minDistance     = 4.0f;
            float maxDistance     = 40.0f;
            float fovRadians      = DirectX::XM_PIDIV4;                  // 45 degrees vertical
            float nearPlane       = 0.1f;
            float farPlane        = 200.0f;
        };

        explicit Camera(const Settings& settings = Settings{});

        // ------------------------------------------------------------------
        // Simulation side -- call from OnUpdate.
        // ------------------------------------------------------------------

        /// Snapshots the current pose as "previous". Call once at the start of
        /// every OnUpdate, before Rotate/Zoom, exactly like the cube's
        /// m_previousAngle = m_currentAngle.
        void BeginStep() noexcept;

        /// Orbits around the target by deltaRadians. Callers decide which
        /// input maps to which sign; what matters is that every rotate input
        /// goes through here so they all agree.
        void Rotate(float deltaRadians) noexcept;

        /// Multiplies the distance by factor, clamped to the settings' range.
        /// factor < 1 zooms in. Multiplicative because zoom feels linear to
        /// the eye only when each step is a fixed *ratio* of the distance.
        void Zoom(float factor) noexcept;

        // ------------------------------------------------------------------
        // Render side -- matrices for the pose interpolated by alpha.
        // ------------------------------------------------------------------

        /// XM_CALLCONV selects __vectorcall on x64, so XMMATRIX values travel
        /// in SIMD registers instead of through memory. An optimisation only,
        /// but it is the DirectXMath idiom for functions returning matrices.
        [[nodiscard]] DirectX::XMMATRIX XM_CALLCONV View(float alpha) const noexcept;
        [[nodiscard]] DirectX::XMMATRIX XM_CALLCONV Projection(float aspectRatio) const noexcept;

        /// The world-space ray under a pixel, for the *current* pose.
        ///
        /// Called from OnUpdate, so it uses the latest simulation state rather
        /// than an interpolated one; the difference is under one simulation
        /// step and invisible in practice.
        [[nodiscard]] Ray ScreenToWorldRay(float pixelX, float pixelY,
                                           float viewportWidth,
                                           float viewportHeight) const noexcept;

    private:
        [[nodiscard]] DirectX::XMVECTOR XM_CALLCONV EyePosition(float yaw, float distance) const noexcept;
        [[nodiscard]] DirectX::XMMATRIX XM_CALLCONV ViewFor(float yaw, float distance) const noexcept;

        Settings m_settings;

        // Storage types, not SIMD types. XMFLOAT3 is three plain floats;
        // XMVECTOR is a 16-byte-aligned register type that belongs on the
        // stack during computation, not inside a heap-allocatable object.
        DirectX::XMFLOAT3 m_target{ 0.0f, 0.0f, 0.0f };

        float m_yaw              = 0.0f;
        float m_previousYaw      = 0.0f;
        float m_distance         = 0.0f;
        float m_previousDistance = 0.0f;
    };
}
