cbuffer SceneConstantBuffer : register(b0)
{
    float time;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;

    result.position = position;
    result.color = color * (2 + sin(time)) / 2;
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}