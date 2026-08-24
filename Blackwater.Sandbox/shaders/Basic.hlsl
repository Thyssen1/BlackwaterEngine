//
// Basic.hlsl -- position + vertex colour, transformed by one combined matrix.
//
// Compiled at runtime by bw::Shader (vs_5_0 / ps_5_0).
//

// register(b0) matches ConstantBuffer::BindToVertexStage(context, 0).
//
// The matrix arrives already transposed. DirectXMath stores matrices
// row-major; HLSL packs float4x4 constants column-major. Transposing on the
// CPU makes those two conventions cancel out, so mul(vector, matrix) below
// does what it reads like.
cbuffer Transforms : register(b0)
{
    float4x4 WorldViewProjection;
};

struct VSInput
{
    float3 position : POSITION;   // matches DXGI_FORMAT_R32G32B32_FLOAT
    float4 colour   : COLOR;      // matches DXGI_FORMAT_R32G32B32A32_FLOAT
};

struct VSOutput
{
    // SV_POSITION is a system value: the rasterizer requires it, and it must
    // be in clip space (w not yet divided out). Everything else is
    // interpolated across the triangle on the way to the pixel shader.
    float4 position : SV_POSITION;
    float4 colour   : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    // w = 1 makes this a point rather than a direction, so the matrix's
    // translation column actually applies.
    output.position = mul(float4(input.position, 1.0f), WorldViewProjection);
    output.colour   = input.colour;

    return output;
}

// SV_TARGET means "this is the colour for render target 0".
float4 PSMain(VSOutput input) : SV_TARGET
{
    // The colour here is not the vertex colour -- it is the barycentric
    // interpolation of the three corner colours of this triangle, done by
    // the rasterizer for free.
    return input.colour;
}
