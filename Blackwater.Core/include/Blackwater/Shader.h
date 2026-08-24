#pragma once

//
// A vertex + pixel shader pair, compiled from HLSL at runtime, together with
// the input layout describing the vertex format they expect.
//
// The three are one object because D3D validates them against each other:
// the input layout is checked against the vertex shader's declared inputs
// when it is created, so they are only meaningful together.
//

#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <span>
#include <string>

namespace bw
{
    // Same using-declaration as GraphicsDevice.h. Repeating it is legal and
    // keeps this header from having to include that one just for a name.
    using Microsoft::WRL::ComPtr;

    class Shader final
    {
    public:
        /// Compiles both stages from a single .hlsl file.
        ///
        /// hlslRelativePath is resolved against the executable's own folder,
        /// because the working directory differs between Visual Studio,
        /// Rider, and double-clicking the exe.
        ///
        /// Throws std::runtime_error carrying the HLSL compiler's diagnostics
        /// on a compile error, or HResultError for anything else.
        Shader(ID3D11Device* device,
               const wchar_t* hlslRelativePath,
               const char* vertexEntryPoint,
               const char* pixelEntryPoint,
               std::span<const D3D11_INPUT_ELEMENT_DESC> inputLayout);

        Shader(const Shader&)            = delete;
        Shader& operator=(const Shader&) = delete;
        Shader(Shader&&)                 = delete;
        Shader& operator=(Shader&&)      = delete;
        ~Shader()                        = default;

        /// Binds the layout and both stages to the pipeline.
        void Bind(ID3D11DeviceContext* context) const;

    private:
        static ComPtr<ID3DBlob> CompileStage(const std::wstring& path,
                                             const char* entryPoint,
                                             const char* targetProfile);

        ComPtr<ID3D11VertexShader> m_vertexShader;
        ComPtr<ID3D11PixelShader>  m_pixelShader;
        ComPtr<ID3D11InputLayout>  m_inputLayout;
    };
}
