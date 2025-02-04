static const uint PLAYER = 0;
static const uint ENEMY = 1;
static const uint PROJECTILE = 2;
static const uint PICKUP = 3;

struct RASTER_IN
{
    float4 posHomog : SV_Position;
    float3 posWorld : WORLD;
    float2 uv : UV;
    float3 normWorld : NORM;
};

cbuffer MAP_MODEL_TEX : register(b0)
{
    uint modelType;
    uint pad[3];
};

float4 main(RASTER_IN inputVertex) : SV_TARGET
{
    float4 modelColor;
    
    if (modelType == PLAYER)
    {
        modelColor = float4(0.0f, 1.0f, 0.0f, 1.0f);
    }
    
    if (modelType == ENEMY)
    {
        modelColor = float4(1.0f, 0.0f, 0.0f, 1.0f);
    }
    
    if (modelType == PROJECTILE)
    {
        modelColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    if (modelType == PICKUP)
    {
        modelColor = float4(0.0f, 0.2f, 1.0f, 1.0f);
    }
	
    return modelColor;
    
}