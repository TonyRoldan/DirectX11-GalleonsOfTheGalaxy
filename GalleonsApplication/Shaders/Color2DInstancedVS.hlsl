// Pulled directly from the "VulkanDescriptorSets" sample.
// Removing the arrays & using HLSL StructuredBuffer<> would be better.
#define MAX_INSTANCE_PER_DRAW 240
cbuffer INSTANCE_UNIFORMS
{
	matrix instance_transforms[MAX_INSTANCE_PER_DRAW];
	vector instance_colors[MAX_INSTANCE_PER_DRAW];
};
struct V_OUT { 
	float4 hpos : SV_POSITION;
	nointerpolation uint pixelInstanceID : INSTANCE;
}; 
V_OUT main(	float2 inputVertex : POSITION, 
			uint vertexInstanceID : SV_INSTANCEID)
{
	V_OUT send = (V_OUT)0;
	send.hpos = mul(instance_transforms[vertexInstanceID], 
					float4(inputVertex,0,1));
	send.pixelInstanceID = vertexInstanceID;
	return send;
}