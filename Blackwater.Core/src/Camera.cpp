#include "Blackwater/Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;   // fine in a .cpp -- never in a header

namespace
{
    float Lerp(float a, float b, float t) noexcept
    {
        return a + (b - a) * t;
    }
}

namespace bw
{
    bool IntersectGroundPlane(const Ray& ray, float height, XMFLOAT3& hit) noexcept
    {
        // A point on the ray is origin + t * direction. Solve for the t at
        // which its y equals the plane height:
        //
        //     origin.y + t * direction.y = height
        //     t = (height - origin.y) / direction.y
        //
        // A near-zero direction.y means the ray is (almost) parallel to the
        // ground and would hit it absurdly far away, or never.
        constexpr float kParallelEpsilon = 1e-6f;

        if (std::fabs(ray.direction.y) < kParallelEpsilon)
        {
            return false;
        }

        const float t = (height - ray.origin.y) / ray.direction.y;

        // Negative t means the plane is *behind* the ray's origin.
        if (t < 0.0f)
        {
            return false;
        }

        hit.x = ray.origin.x + t * ray.direction.x;
        hit.y = height;
        hit.z = ray.origin.z + t * ray.direction.z;
        return true;
    }

    Camera::Camera(const Settings& settings)
        : m_settings(settings)
        , m_distance(settings.distance)
        , m_previousDistance(settings.distance)
    {
    }

    void Camera::BeginStep() noexcept
    {
        m_previousYaw      = m_yaw;
        m_previousDistance = m_distance;

        // Yaw is never wrapped into [0, 2pi) on its own. If it were, a step
        // from 359 to 1 degree would interpolate the long way round -- through
        // 180 -- and the camera would whip around for one frame.
        //
        // Instead, once a full turn has built up, shift *both* values by the
        // same amount. Their difference -- the only thing interpolation looks
        // at -- is untouched, and the numbers stay small enough for float
        // precision.
        constexpr float kFullTurn = XM_2PI;

        if (m_yaw > kFullTurn || m_yaw < -kFullTurn)
        {
            const float shift = std::floor(m_yaw / kFullTurn) * kFullTurn;
            m_yaw         -= shift;
            m_previousYaw -= shift;
        }
    }

    void Camera::Rotate(float deltaRadians) noexcept
    {
        m_yaw += deltaRadians;
    }

    void Camera::Zoom(float factor) noexcept
    {
        m_distance = std::clamp(m_distance * factor,
                                m_settings.minDistance,
                                m_settings.maxDistance);
    }

    XMVECTOR XM_CALLCONV Camera::EyePosition(float yaw, float distance) const noexcept
    {
        // Spherical coordinates around the target, left-handed (+Z forward):
        // at yaw 0 the camera sits on the -Z side looking towards +Z, which
        // matches where the M1 sandbox put its hardcoded eye.
        const float pitch      = m_settings.pitchRadians;
        const float horizontal = distance * std::cos(pitch);

        const XMVECTOR offset = XMVectorSet(
            -horizontal * std::sin(yaw),
             distance   * std::sin(pitch),
            -horizontal * std::cos(yaw),
             0.0f);

        return XMVectorAdd(XMLoadFloat3(&m_target), offset);
    }

    XMMATRIX XM_CALLCONV Camera::ViewFor(float yaw, float distance) const noexcept
    {
        return XMMatrixLookAtLH(
            EyePosition(yaw, distance),
            XMLoadFloat3(&m_target),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    }

    XMMATRIX XM_CALLCONV Camera::View(float alpha) const noexcept
    {
        return ViewFor(Lerp(m_previousYaw,      m_yaw,      alpha),
                       Lerp(m_previousDistance, m_distance, alpha));
    }

    XMMATRIX XM_CALLCONV Camera::Projection(float aspectRatio) const noexcept
    {
        return XMMatrixPerspectiveFovLH(m_settings.fovRadians,
                                        aspectRatio,
                                        m_settings.nearPlane,
                                        m_settings.farPlane);
    }

    Ray Camera::ScreenToWorldRay(float pixelX, float pixelY,
                                 float viewportWidth,
                                 float viewportHeight) const noexcept
    {
        const XMMATRIX view       = ViewFor(m_yaw, m_distance);
        const XMMATRIX projection = Projection(viewportWidth / viewportHeight);

        // Unproject runs the rendering transform backwards: pixel -> NDC ->
        // inverse projection -> inverse view. Doing it at depth 0 (the near
        // plane) and depth 1 (the far plane) gives two world points; every
        // point under this pixel lies on the line between them.
        //
        // World matrix is identity because we want world space itself.
        const auto unproject = [&](float depth)
        {
            return XMVector3Unproject(
                XMVectorSet(pixelX, pixelY, depth, 0.0f),
                0.0f, 0.0f, viewportWidth, viewportHeight,
                0.0f, 1.0f,
                projection, view, XMMatrixIdentity());
        };

        const XMVECTOR nearPoint = unproject(0.0f);
        const XMVECTOR farPoint  = unproject(1.0f);

        Ray ray{};
        XMStoreFloat3(&ray.origin, nearPoint);
        XMStoreFloat3(&ray.direction, XMVector3Normalize(XMVectorSubtract(farPoint, nearPoint)));
        return ray;
    }
}
