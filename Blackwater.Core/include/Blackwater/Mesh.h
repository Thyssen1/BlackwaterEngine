#pragma once

//
// Geometry on the GPU: a vertex buffer, an index buffer, and the draw call.
//
// Mesh deliberately does not know what a vertex looks like -- only how many
// bytes one occupies. The Shader's input layout is what gives those bytes
// meaning, which is why the two are validated against each other rather than
// against the mesh.
//

#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <span>

namespace bw
{
    using Microsoft::WRL::ComPtr;

    class Mesh final
    {
    public:
        /// vertices     raw bytes of the vertex array
        /// vertexCount  number of vertices
        /// vertexStride size of one vertex, in bytes
        /// indices      triangle indices, three per triangle
        Mesh(ID3D11Device* device,
             const void* vertices,
             size_t vertexCount,
             uint32_t vertexStride,
             std::span<const uint32_t> indices);

        Mesh(const Mesh&)            = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&&)                 = delete;
        Mesh& operator=(Mesh&&)      = delete;
        ~Mesh()                      = default;

        /// Binds the buffers and issues a DrawIndexed.
        void Draw(ID3D11DeviceContext* context) const;

        [[nodiscard]] uint32_t IndexCount() const noexcept { return m_indexCount; }

    private:
        ComPtr<ID3D11Buffer> m_vertexBuffer;
        ComPtr<ID3D11Buffer> m_indexBuffer;

        uint32_t m_vertexStride = 0;
        uint32_t m_indexCount   = 0;
    };
}
