#pragma pack_matrix(row_major)

// ---------- Structs ----------
struct RASTER_OUT
{
    float4 posHomog : SV_Position;
    float3 posWorld : WORLD;
    float2 uv : UV;
    float3 normWorld : NORM;
};

struct ATTRIBUTE
{
    float3 diffReflect;
    float dissolve;
    float3 specReflect;
    float spec;
    float3 ambReflect;
    float sharpness;
    float3 transFilter;
    float optDens;
    float3 emmReflect;
    uint illum;
};

struct VERT_IN
{
    float3 _pos : POSITION;
    float3 _uv : UV;
    float3 _norm : NORM;
};

// ---------- Buffers ----------

StructuredBuffer<float4x4> instanceTransforms : register(t0);

cbuffer INSTANCE_DATA : register(b2)
{
    uint transformStart;
    uint materialStart;
    uint pad1[2];
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

cbuffer MESH_DATA : register(b0)
{
    float4x4 world;
    ATTRIBUTE material;
    uint texID;
    float offset;
    uint pad2[2];
};


RASTER_OUT main(VERT_IN inputVertex, uint id : SV_InstanceID)
{
    RASTER_OUT output = (RASTER_OUT) 0;
    
    output.posHomog = float4(inputVertex._pos, 1);
    output.normWorld = inputVertex._norm;
    output.uv = inputVertex._uv.xy;
    float4x4 curTransform;
     
    curTransform = transpose(instanceTransforms[transformStart + id]);
     
    output.normWorld = mul(float4(output.normWorld, 0), curTransform);
    output.normWorld = normalize(output.normWorld);
    
    output.posHomog = mul(output.posHomog, curTransform);
    output.posHomog.x += offset;
    output.posWorld = output.posHomog;
    output.posHomog = mul(output.posHomog, view);
    output.posHomog = mul(output.posHomog, projection);
    
    output.uv = inputVertex._uv;
       
    return output;
}