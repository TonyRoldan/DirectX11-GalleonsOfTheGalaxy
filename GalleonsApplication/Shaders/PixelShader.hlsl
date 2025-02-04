#pragma pack_matrix(row_major)

float4 GetDirectionalLight(float3 lightDir, float4 lightColor, float3 vertNorm, float4 surfaceColor, float4 ambient);
float4 GetSpecularReflect(float4 camPos, float3 lightDir, float3 vertPos, float3 vertNorm, float specExponent, float4 materialReflect);
float4 GetPointLight(float4 lightColor, float3 lightPos, float radius, float3 vertPos, float3 vertNorm);
float4 GetSpotLight(float4 surfaceColor, float4 lightColor, float3 lightPos, float3 lightDir, float3 coneDir, float innerCone, float outerCone, float3 vertPos, float3 vertNorm);
float4 GetTexture(float2 vertUV);
float4 FilterColor(float4 color, float contrastFactor, float saturationFactor);


// ---------- Tex Ids ----------
static const uint LEVEL = 1;
static const uint SHIPS = 2;
static const uint BOMB = 3;

// ---------- Structs ----------
struct LIGHT
{
    float4x4 lightWorld;
    float4 lightPos;
    float4 lightColor;
    float4 lightDir;
    float lightRad;
    float innerCone;
    float outerCone;
};

struct RASTER_IN
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

// ---------- Textures ----------

Texture2D bomb : register(t4);
Texture2D ships : register(t3);
Texture2D level : register(t2);

SamplerState samp : register(s0);

// ---------- Buffers ----------
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


StructuredBuffer<LIGHT> sceneLights : register(t1);

float4 main(RASTER_IN inputVertex) : SV_TARGET
{
    float4 colorOut;
    
    // ----- Colors -----   
    float4 surfaceColor = float4(material.diffReflect.xyz, 0);
    float4 directLight;
    float4 reflectLight;
    float4 pointLight;
    float4 spotLight;
    float4 texColor;   
    float4 lightColor;
    
    // ----- Vertex Data -----
    float3 vertPos = inputVertex.posWorld;
    float3 vertNorm = inputVertex.normWorld;  
    float2 vertUV = inputVertex.uv.xy;
    
    // ----- Material Data -----
    float4 materialReflect = float4(material.specReflect.xyz, 0);
    float specExponent = material.spec;
    
    // ----- Light Properties -----
    float3 _dirlightDir = normalize(dirLightDir.xyz);
    float3 lightPos;
    float radius;    
    float3 lightDir;
    float pointLightRatio;
    float attenuation;
    float3 coneDir;
    float innerCone;
    float outerCone;
    float lightType;
    float distance; 
    uint lightCount;
    uint lightSize;
    
    texColor = GetTexture(vertUV);  
      
    if (vertUV.x > 0 && vertUV.y > 0)
    {
        surfaceColor *= texColor;
    }
    
    directLight = GetDirectionalLight(_dirlightDir, dirLightColor, vertNorm, surfaceColor, ambientTerm);   
    reflectLight = GetSpecularReflect(camPos, _dirlightDir, vertPos, vertNorm, specExponent, materialReflect);
    
    colorOut = directLight + reflectLight;
     
    sceneLights.GetDimensions(lightCount, lightSize);
    
    for (int i = 0; i < lightCount; i++)
    {
        lightType = sceneLights[i].lightPos.w;
        lightColor = sceneLights[i].lightColor;
        lightPos = sceneLights[i].lightPos.xyz;
        coneDir = sceneLights[i].lightDir.xyz;
        innerCone = sceneLights[i].innerCone;
        outerCone = sceneLights[i].outerCone;        
        radius = sceneLights[i].lightRad;
        
        distance = sqrt(pow(lightPos.x - vertPos.x, 2) + pow(lightPos.y - vertPos.y, 2) + pow(lightPos.z - vertPos.z, 2));
               
        if (lightType == 1 && distance <= radius + 1)
        {
            pointLight = GetPointLight(lightColor, lightPos, radius, vertPos, vertNorm);
            colorOut += pointLight;
        }
        else if (lightType == 2)
        {          
            spotLight = GetSpotLight(surfaceColor, lightColor, lightPos, lightDir, coneDir, innerCone, outerCone, vertPos, vertNorm);           
            colorOut += spotLight;
        }      
    }      
    
    float fogBase = 10;
    float fogDistance = length(inputVertex.posWorld.xyz - camPos.xyz) - fogStartDistance;
    float fogFactor = log(fogDistance * fogDensity + 1.0) / log(fogBase);
    fogFactor = saturate(fogFactor);
    colorOut = lerp(colorOut, fogColor, fogFactor);
    
    colorOut = FilterColor(colorOut, contrast, saturation);
    
    return colorOut;
}

// ---------- Functions ----------

float4 GetDirectionalLight(float3 lightDir, float4 lightColor, float3 vertNorm, float4 surfaceColor, float4 ambient)
{  
    float lightRatio = saturate(dot(-lightDir, vertNorm));
    lightRatio = saturate(lightRatio + ambient);
    float4 newColor = lightColor * surfaceColor * lightRatio;
    return newColor;
}

float4 GetSpecularReflect(float4 camPos, float3 lightDir, float3 vertPos, float3 vertNorm, float specExponent, float4 materialReflect)
{
    float3 camXYZ = camPos.xyz;
    float3 viewDir = normalize(camXYZ - vertPos);
    float3 halfVector = normalize(-lightDir + viewDir);   
    float intensity = max(pow(saturate(dot(vertNorm, halfVector)), specExponent + 0.000001f), 0);   
    float4 reflectedLight = dirLightColor * intensity * materialReflect;  
    return reflectedLight;
}

float4 GetPointLight(float4 lightColor, float3 lightPos, float radius, float3 vertPos, float3 vertNorm)
{
    float3 lightDir = normalize(lightPos - vertPos);
    float mag = length(lightPos - vertPos);
    float attenuation = 1 - saturate(mag / (radius + 1));
    attenuation *= attenuation;
    float pointLightRatio = saturate(dot(lightDir, vertNorm));
    pointLightRatio = pointLightRatio * attenuation;           
    float4 pointLight = lerp(float4(0, 0, 0, 0), lightColor, pointLightRatio);                     
    return pointLight;
}

float4 GetSpotLight(float4 surfaceColor, float4 lightColor, float3 lightPos, float3 lightDir, float3 coneDir, float innerCone, float outerCone, float3 vertPos, float3 vertNorm)
{          
    lightDir = normalize(lightPos - vertPos);                        
    float surfaceRatio = saturate(dot(-lightDir, normalize(coneDir)));
    float spotFactor;
    if (surfaceRatio > outerCone)    
        spotFactor = 1;  
    else    
        spotFactor = 0;                
    float attenuation = 1 - saturate((innerCone - surfaceRatio) / (innerCone - outerCone));
    attenuation *= attenuation;            
    float spotLightRatio = saturate(dot(lightDir, vertNorm));            
    spotLightRatio = attenuation * spotLightRatio * spotFactor;           
    float4 spotLight = lightColor * surfaceColor * spotLightRatio;           
    return spotLight;
}

float4 GetTexture(float2 vertUV)
{
    float4 texColor;
    switch (texID)
    {
        case LEVEL:
            texColor = level.Sample(samp, vertUV);
            break;
        case SHIPS:
            texColor = ships.Sample(samp, vertUV);
            break;
        case BOMB:
            texColor = bomb.Sample(samp, vertUV);
            break;
        case 0:
            break;
        default:
            break;
    }
    return texColor;
}

float4 FilterColor(float4 color, float contrastFactor, float saturationFactor)
{    
    // Adjust contrast
    color = (color - 0.5f) * contrastFactor + 0.5f;
    
    // Adjust saturation
    float intensity = dot(color.xyz, float3(0.299f, 0.587f, 0.114f));
    color.xyz = lerp(float3(intensity, intensity, intensity), color.xyz, saturationFactor);
    
    return color;
}

