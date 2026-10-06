#include "Blackwater/Mesh.h"
#include "Blackwater/Check.h"

namespace bw
{
    Mesh::Mesh(ID3D11Device* device,
               const void* vertices,
               size_t vertexCount,
               uint32_t vertexStride,
               std::span<const uint32_t> indices,
               D3D11_PRIMITIVE_TOPOLOGY topology)
        : m_vertexStride(vertexStride)
        , m_indexCount(static_cast<uint32_t>(indices.size()))
        , m_topology(topology)
    {
        // ------------------------------------------------------------------
        // Vertex buffer
        //
        // IMMUTABLE is the fastest usage the GPU offers, and the trade is that
        // the contents can never change: the data must be supplied at creation
        // and the CPU can never map it again. Correct for static geometry.
        // ------------------------------------------------------------------
        D3D11_BUFFER_DESC vertexDesc{};
        vertexDesc.ByteWidth = static_cast<UINT>(vertexCount * vertexStride);
        vertexDesc.Usage     = D3D11_USAGE_IMMUTABLE;
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexData{};
        vertexData.pSysMem = vertices;

        BW_CHECK(device->CreateBuffer(&vertexDesc, &vertexData,
                                      m_vertexBuffer.ReleaseAndGetAddressOf()));

        // ------------------------------------------------------------------
        // Index buffer
        //
        // Indices let vertices be shared between triangles. A cube has 8
        // corners but 12 triangles needing 36 vertex references -- without
        // indexing you would upload all 36.
        // ------------------------------------------------------------------
        D3D11_BUFFER_DESC indexDesc{};
        indexDesc.ByteWidth = static_cast<UINT>(indices.size_bytes());
        indexDesc.Usage     = D3D11_USAGE_IMMUTABLE;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexData{};
        indexData.pSysMem = indices.data();

        BW_CHECK(device->CreateBuffer(&indexDesc, &indexData,
                                      m_indexBuffer.ReleaseAndGetAddressOf()));
    }

    void Mesh::Draw(ID3D11DeviceContext* context) const
    {
        // The Input Assembler is the first pipeline stage: it reads these
        // buffers and feeds vertices to the vertex shader.
        ID3D11Buffer* const vertexBuffers[] = { m_vertexBuffer.Get() };
        const UINT strides[] = { m_vertexStride };
        const UINT offsets[] = { 0 };

        context->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);

        // R32_UINT must match the uint32_t indices the constructor took. Using
        // R16_UINT here with 32-bit data reads half of each index and produces
        // spectacular garbage.
        context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        // How to group the indices -- chosen at construction. Topology is
        // pipeline state like everything else here, so it is set per draw:
        // the previous mesh may have left a different one bound.
        context->IASetPrimitiveTopology(m_topology);

        context->DrawIndexed(m_indexCount, 0, 0);
    }
}
