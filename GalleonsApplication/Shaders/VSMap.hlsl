
struct RASTER_OUT
{
    float4 posHomog : SV_Position;
    float3 posWorld : WORLD;
    float2 uv : UV;
    float3 normWorld : NORM;
};

struct VERT_IN
{
    float3 _pos : POSITION;
    float3 _uv : UV;
    float3 _norm : NORM;
};

cbuffer SCENE_DATA : register(b1)
{
    float4x4 view;
    float4x4 projection;
    float4 camPos;
    float4 dirLightDir;
    float4 dirLightColor;
    float4 ambientTerm;
    float4 fogColor;
    float fogDensity;
    float fogStartDistance;
    float contrast;
    float saturation;
};

RASTER_OUT main(VERT_IN inputVertex)
{
    RASTER_OUT vertOut;
    
    vertOut.posHomog = float4(inputVertex._pos, 1);
    vertOut.uv = inputVertex._uv.xy;
    
    return vertOut;
}