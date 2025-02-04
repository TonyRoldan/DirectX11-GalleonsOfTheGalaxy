// Pulled directly from the "VulkanDescriptorSets" sample.
// Removing the arrays & using HLSL StructuredBuffer<> would be better.
#define MAX_INSTANCE_PER_DRAW 240
cbuffer INSTANCE_UNIFORMS
{
	matrix instance_transforms[MAX_INSTANCE_PER_DRAW];
	vector instance_colors[MAX_INSTANCE_PER_DRAW];
};
float4 main(uint pixelInstanceID : INSTANCE) : SV_TARGET 
{	
	return instance_colors[pixelInstanceID]; 
}