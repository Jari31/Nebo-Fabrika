shader_type spatial;

//render_mode wireframe;// cull_disabled;  //depth_test_disabled;//unshaded

uniform sampler2D IndexTexture   : filter_nearest;
uniform sampler2D VertexTexture  : filter_nearest;
uniform sampler2D NormalTexture  : filter_nearest;

uniform sampler2DArray AlbedoTextures : source_color;
uniform sampler2DArray NormalTextures;

uniform int GridSizeIndex;
uniform int GridSizeVertex;

varying vec3 Color;
varying float G_MaterialID;
varying vec3 G_WorldCoordinates;
varying vec3 G_WorldNormals;
varying mat3 G_TBN;

vec3 sampleAlbedo(vec3 WorldPosition, vec3 Normals, float MaterialID)
{
	vec3 TextureX = texture(AlbedoTextures, vec3(WorldPosition.yz, MaterialID)).rgb;
	vec3 TextureY = texture(AlbedoTextures, vec3(WorldPosition.xz, MaterialID)).rgb;
	vec3 TextureZ = texture(AlbedoTextures, vec3(WorldPosition.xy, MaterialID)).rgb;
	
	vec3 BlendWeight = abs(Normals);
	BlendWeight /= (BlendWeight.x + BlendWeight.y + BlendWeight.z);
	
	return TextureX * BlendWeight.x + TextureY * BlendWeight.y + TextureZ * BlendWeight.z;
}

vec3 dsampleNormals(vec3 WorldPosition, vec3 Normals, float MaterialID)
{
	vec3 n = abs(Normals);
	vec3 NormalWeight = n / (dot(n, vec3(1.0)) + 1e-8);
	
	vec3 tX = texture(NormalTextures, vec3(WorldPosition.yz, MaterialID)).rgb * 2.0 - 1.0;
	vec3 tY = texture(NormalTextures, vec3(WorldPosition.xz, MaterialID)).rgb * 2.0 - 1.0;
	vec3 tZ = texture(NormalTextures, vec3(WorldPosition.xy, MaterialID)).rgb * 2.0 - 1.0;
	
	//tX.y = -tX.y;
	//tY.y = -tY.y;
	//tZ.y = -tZ.y;
	
	tX = vec3(tX.xy + Normals.zy, tX.z * Normals.x);
	tY = vec3(tY.xy + Normals.xz, tY.z * Normals.y);
	tZ = vec3(tZ.xy + Normals.xy, tZ.z * Normals.z);
	
	return normalize(tX.zyx * NormalWeight.x + tY.xzy * NormalWeight.y + tZ.xyz * NormalWeight.z);
}

vec3 sampleNormals(vec3 WorldPosition, vec3 Normals, float MaterialID)
{
	vec3 NormalWeights = pow(abs(Normals), vec3(1));
	NormalWeights /= (NormalWeights.x + NormalWeights.y + NormalWeights.z);
	
	vec3 tx = texture(NormalTextures, vec3(WorldPosition.zy, MaterialID)).rgb * 2.0 - 1.0;
	vec3 ty = texture(NormalTextures, vec3(WorldPosition.xz, MaterialID)).rgb * 2.0 - 1.0;
	vec3 tz = texture(NormalTextures, vec3(WorldPosition.xy, MaterialID)).rgb * 2.0 - 1.0;
	
	tx = vec3(0.0, tx.y, tx.x) + vec3(Normals.x, 0.0, 0.0);
    ty = vec3(ty.x, 0.0, ty.y) + vec3(0.0, Normals.y, 0.0);
    tz = vec3(tz.x, tz.y, 0.0) + vec3(0.0, 0.0, Normals.z);
	
	vec3 Blend = tx * NormalWeights.x + ty * NormalWeights.y + tz * NormalWeights.z;
	return normalize(Blend);
}

vec3 sampleTex_triplanar(vec3 WorldPosition, vec3 Normals, float TextureScale, float MaterialID, uint SamplerType)
{
	WorldPosition = WorldPosition * TextureScale;
	vec3 Result;
	switch(SamplerType){
		case 0u:
			Result = sampleAlbedo(WorldPosition, Normals, MaterialID);
			break;
		case 1u:
			Result = sampleNormals(WorldPosition, Normals, MaterialID);
			break;
	}
	
	return Result;
}

void vertex()
{
	float ScaleOffset = 0.1;
	float so = 0.000001f;
	//POINT_SIZE = 10.0f;
	ivec2 IndiceIndex = ivec2(VERTEX_ID % GridSizeIndex, VERTEX_ID/GridSizeIndex); 
	int VertexPointer = int(round(texelFetch(IndexTexture, IndiceIndex, 0).r));
	
	int vx = VertexPointer % GridSizeVertex;//VERTEX_ID % GridSizeVertex;//
	int vy = VertexPointer / GridSizeVertex;//VERTEX_ID / GridSizeVertex;//
	ivec2 VertexIndex = ivec2(vx, vy);

	vec4 Vertices = texelFetch(VertexTexture, VertexIndex, 0);
	vec4 Normals = texelFetch(NormalTexture, VertexIndex, 0);
	
	VERTEX = (vec3(Vertices.x, Vertices.y, Vertices.z) * ScaleOffset);
	G_WorldCoordinates = vec4(MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	NORMAL = Normals.xyz;
	G_WorldNormals = normalize(MODEL_NORMAL_MATRIX * NORMAL);
	
	float MaterialID = 0.0f;//VERTEX.w;
	G_MaterialID = MaterialID;
	
	//Color = NORMAL * 0.5 + 0.5;
	ROUGHNESS = 1.0;
	NORMAL = sampleTex_triplanar(G_WorldCoordinates, G_WorldNormals, 1, G_MaterialID, 1u);
}

void fragment() {
	//vec3 TangentSpaceNormal = normalize(transpose(G_TBN) * WorldSpaceNormal);
	Color = mix(sampleTex_triplanar(G_WorldCoordinates, G_WorldNormals, 0.5, G_MaterialID, 0u), 
				sampleTex_triplanar(G_WorldCoordinates, G_WorldNormals, 2, G_MaterialID, 0u), 0.5);
	//if (dot(TangentSpaceNormal, vec3(0.0, 0.0, 0.0)) < 0.0) {
	//	TangentSpaceNormal = TangentSpaceNormal;
	//}
	
	
	//Color = vec3(0.1, 0.1, 0.1);
	
    ALBEDO = Color;
	METALLIC = 0.0;
	if (!FRONT_FACING) {
        ALBEDO = vec3(1.0, 0.0, 1.0); 
    }
}

//void light() {
//	// Called for every pixel for every light affecting the material.
//	// Uncomment to replace the default light processing function with this one.
//}
