#include "Blackwater/Shader.h"
#include "Blackwater/Check.h"

#include <d3dcompiler.h>
#include <stdexcept>

namespace
{
    /// The folder the running executable sits in.
    ///
    /// Runtime shader compilation needs the .hlsl file to exist when the app
    /// starts, and the working directory is not dependable -- Visual Studio,
    /// Rider and Explorer all pick different ones. The executable's own
    /// location is the one thing that stays put.
    std::wstring ExecutableDirectory()
    {
        wchar_t buffer[MAX_PATH]{};

        const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);

        // Returning MAX_PATH means the path was truncated rather than copied.
        if (length == 0 || length == MAX_PATH)
        {
            bw::ThrowLastError("GetModuleFileNameW", __FILE__, __LINE__);
        }

        const std::wstring path(buffer, length);
        const size_t separator = path.find_last_of(L'\\');

        return (separator == std::wstring::npos) ? path : path.substr(0, separator);
    }
}

namespace bw
{
    ComPtr<ID3DBlob> Shader::CompileStage(const std::wstring& path,
                                          const char* entryPoint,
                                          const char* targetProfile)
    {
        // ENABLE_STRICTNESS rejects deprecated syntax rather than quietly
        // accepting it -- worth having on from the first shader.
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef BW_DEBUG
        // Embeds source and disables optimisation so a graphics debugger can
        // step the shader line by line.
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        ComPtr<ID3DBlob> bytecode;
        ComPtr<ID3DBlob> errors;

        const HRESULT hr = ::D3DCompileFromFile(
            path.c_str(),
            nullptr,                            // no preprocessor defines
            D3D_COMPILE_STANDARD_FILE_INCLUDE,  // lets HLSL #include work
            entryPoint,
            targetProfile,
            flags,
            0,
            bytecode.GetAddressOf(),
            errors.GetAddressOf());

        if (FAILED(hr))
        {
            // The compiler's own diagnostics are far more useful than the
            // HRESULT: line numbers, the offending token, what was expected.
            // Only an actual compile error produces this blob -- a missing
            // file does not.
            if (errors)
            {
                throw std::runtime_error(
                    std::string("HLSL compilation failed:\n\n") +
                    static_cast<const char*>(errors->GetBufferPointer()));
            }

            // No diagnostics: file not found, access denied, and similar.
            BW_CHECK(hr);
        }

        return bytecode;
    }

    Shader::Shader(ID3D11Device* device,
                   const wchar_t* hlslRelativePath,
                   const char* vertexEntryPoint,
                   const char* pixelEntryPoint,
                   std::span<const D3D11_INPUT_ELEMENT_DESC> inputLayout)
    {
        const std::wstring path = ExecutableDirectory() + L"\\" + hlslRelativePath;

        // vs_5_0 / ps_5_0 are the Shader Model 5.0 profiles, which is what
        // D3D11 feature level 11_0 hardware supports.
        const ComPtr<ID3DBlob> vertexCode = CompileStage(path, vertexEntryPoint, "vs_5_0");
        const ComPtr<ID3DBlob> pixelCode  = CompileStage(path, pixelEntryPoint,  "ps_5_0");

        BW_CHECK(device->CreateVertexShader(
            vertexCode->GetBufferPointer(),
            vertexCode->GetBufferSize(),
            nullptr,                            // no class linkage
            m_vertexShader.ReleaseAndGetAddressOf()));

        BW_CHECK(device->CreatePixelShader(
            pixelCode->GetBufferPointer(),
            pixelCode->GetBufferSize(),
            nullptr,
            m_pixelShader.ReleaseAndGetAddressOf()));

        // The vertex shader bytecode is required here, not just the element
        // descriptions: D3D compares the layout against the shader's declared
        // inputs and fails if they disagree. This is one of the few places
        // D3D11 catches the mistake at creation time instead of silently
        // drawing nothing, so it is worth letting it.
        BW_CHECK(device->CreateInputLayout(
            inputLayout.data(),
            static_cast<UINT>(inputLayout.size()),
            vertexCode->GetBufferPointer(),
            vertexCode->GetBufferSize(),
            m_inputLayout.ReleaseAndGetAddressOf()));
    }

    void Shader::Bind(ID3D11DeviceContext* context) const
    {
        // D3D11 is a state machine: these stay bound until something else
        // replaces them, and every subsequent draw uses them implicitly.
        context->IASetInputLayout(m_inputLayout.Get());
        context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    }
}
