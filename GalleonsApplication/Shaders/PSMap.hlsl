struct RASTER_IN
{
    float4 posHomog : SV_Position;
    float3 posWorld : WORLD;
    float2 uv : UV;
    float3 normWorld : NORM;
};

Texture2D miniMap : register(t2);

SamplerState samp : register(s0);


float4 main(RASTER_IN inputVertex) : SV_TARGET
{
    float4 colorOut = miniMap.Sample(samp, inputVertex.uv);
    
    return colorOut;
    
}