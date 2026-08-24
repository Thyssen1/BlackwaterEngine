#pragma once

//
// A typed constant buffer: a CPU struct on one side, an HLSL cbuffer on the
// other.
//
// This is a template, so the whole thing lives in the header. The compiler can
// only emit code for ConstantBuffer<Transforms> if it can see this definition
// at the point of use -- moving it into a .cpp produces unresolved externals.
// C# generics do not work this way; they are resolved by the runtime.
//

#include "Blackwater/Check.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <cstring>
#include <type_traits>

namespace bw
{
    using Microsoft::WRL::ComPtr;

    template <typename T>
    class ConstantBuffer final
    {
        // D3D requires constant buffer sizes to be a multiple of 16 bytes.
        // Catching it here turns a confusing CreateBuffer failure at startup
        // into a compile error naming the offending type.
        static_assert(sizeof(T) % 16 == 0,
                      "Constant buffer payloads must be a multiple of 16 bytes. "
                      "Add explicit padding members to T.");

        // The payload is memcpy'd straight into GPU-visible memory, so it must
        // be flat data: no std::string, no virtuals, no owned pointers.
        static_assert(std::is_trivially_copyable_v<T>,
                      "Constant buffer payloads are memcpy'd to the GPU, so T "
                      "must be trivially copyable.");

    public:
        explicit ConstantBuffer(ID3D11Device* device)
        {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth      = sizeof(T);
            desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;

            // DYNAMIC + CPU_ACCESS_WRITE: rewritten by the CPU every frame.
            // (Mesh data uses IMMUTABLE instead, which is faster but fixed.)
            desc.Usage          = D3D11_USAGE_DYNAMIC;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            BW_CHECK(device->CreateBuffer(&desc, nullptr,
                                          m_buffer.ReleaseAndGetAddressOf()));
        }

        ConstantBuffer(const ConstantBuffer&)            = delete;
        ConstantBuffer& operator=(const ConstantBuffer&) = delete;
        ConstantBuffer(ConstantBuffer&&)                 = delete;
        ConstantBuffer& operator=(ConstantBuffer&&)      = delete;
        ~ConstantBuffer()                                = default;

        /// Uploads a new value.
        ///
        /// WRITE_DISCARD means "the old contents are worthless". That lets the
        /// driver hand back a fresh block of memory immediately rather than
        /// stalling until the GPU has finished reading the previous frame's
        /// copy -- which it may still be doing.
        void Update(ID3D11DeviceContext* context, const T& value)
        {
            D3D11_MAPPED_SUBRESOURCE mapped{};

            BW_CHECK(context->Map(m_buffer.Get(), 0,
                                  D3D11_MAP_WRITE_DISCARD, 0, &mapped));

            std::memcpy(mapped.pData, &value, sizeof(T));

            context->Unmap(m_buffer.Get(), 0);
        }

        /// Binds to a vertex shader constant slot (register b<slot>).
        void BindToVertexStage(ID3D11DeviceContext* context, UINT slot = 0) const
        {
            ID3D11Buffer* const buffers[] = { m_buffer.Get() };
            context->VSSetConstantBuffers(slot, 1, buffers);
        }

        /// Binds to a pixel shader constant slot (register b<slot>).
        void BindToPixelStage(ID3D11DeviceContext* context, UINT slot = 0) const
        {
            ID3D11Buffer* const buffers[] = { m_buffer.Get() };
            context->PSSetConstantBuffers(slot, 1, buffers);
        }

    private:
        ComPtr<ID3D11Buffer> m_buffer;
    };
}
