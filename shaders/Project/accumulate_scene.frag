#version 410

uniform bool has_diffuse_texture;
uniform bool has_specular_texture;
uniform bool has_normals_texture;
uniform bool has_opacity_texture;
uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D normals_texture;
uniform sampler2D opacity_texture;
uniform mat4 normal_model_to_world;

// new
uniform bool is_water;

uniform mat4 view_projection_inverse;
uniform vec3 camera_position;
uniform mat4 shadow_view_projection;

uniform vec3 sun_dir;

uniform vec2 inv_res;
uniform sampler2DShadow shadow_texture;
uniform vec2 shadowmap_texel_size;

uniform sampler2D causticmap_texture;

in VS_OUT {
	vec3 normal;
	vec2 texcoord;
	vec3 tangent;
	vec3 binormal;
	vec4 worldPos;
} fs_in;

out vec4 colour;

void main()
{
	if (has_opacity_texture && texture(opacity_texture, fs_in.texcoord).r < 1.0)
		discard;

	// Diffuse color
	vec4 geometry_diffuse = vec4(0.0f);
	if (has_diffuse_texture)
		geometry_diffuse = texture(diffuse_texture, fs_in.texcoord);

	// Specular color
	vec4 geometry_specular = vec4(0.0f);
	if (has_specular_texture)
		geometry_specular = texture(specular_texture, fs_in.texcoord);

	// Worldspace normal
	vec3 normal;

	if (has_normals_texture && !is_water) {
		vec3 t = normalize(fs_in.tangent);
		vec3 b = normalize(fs_in.binormal);
		vec3 n = normalize(fs_in.normal);
		mat3 tbn = mat3(t, b, n);
		vec3 textureNormal = (texture(normals_texture, fs_in.texcoord).xyz * 2.0) - 1.0;
		normal = normalize(tbn * textureNormal);
	} else {
		normal = fs_in.normal;
	}

	vec3 result = geometry_diffuse.xyz;

	vec3 n = normalize(normal);
	vec3 v = normalize(camera_position - fs_in.worldPos.xyz);
	vec3 l = normalize(-sun_dir);
	vec3 r = normalize(reflect(-l,n));

	float diffuse = dot(n,l);
	result *= diffuse;

	if (is_water) 
	{
		const vec3 deep = vec3(0,0,0.1);
		const vec3 shallow = vec3(0, 0.5, 0.5);
		float facing = max(dot(n,v), 0);
		result = mix(deep, shallow, facing);
	}
	else
	{
		vec2 uv = inv_res * gl_FragCoord.xy;
		float depth = gl_FragCoord.z;
		vec4 screenSpacePos = vec4(uv.x * 2.0 - 1.0, uv.y*2.0 - 1.0, 2.0*depth - 1.0, 1.0);

		// Implement shadow
		vec4 projectedSampler = shadow_view_projection * view_projection_inverse * screenSpacePos;
		projectedSampler /= projectedSampler.w;
		float shadowMultiplier = 0.0;
		vec3 sampler_centre = (projectedSampler.xyz + 1.0) / 2.0;

		int steps = 5;
		int samples = (steps*2 + 1) * (steps*2 + 1);
		vec3 samplerPos;
		for (int i = -steps; i <= steps; i++)
		{
			float dy = i * shadowmap_texel_size.x;
			for (int j = -steps; j <= steps; j++)
			{
				float dx = j * shadowmap_texel_size.y;
				if (sampler_centre.x + dx >= 0 && sampler_centre.x + dx <= 1.0 && sampler_centre.y + dy >= 0 && sampler_centre.y + dy <= 1.0) {
					samplerPos = sampler_centre; samplerPos.x += dx; samplerPos.y += dy;
					shadowMultiplier += texture(shadow_texture, samplerPos);
				} else {
					samples--;
				} 
			}
		}

		shadowMultiplier /= samples;
	
		// bias against shadow acne
		if (shadowMultiplier < 0.05)
			shadowMultiplier = 0;

		result *= shadowMultiplier;
		
		//Caustics term but wrongly done atm.
		//result *= texture(causticmap_texture, uv).xyz;
	}

	colour = vec4(result, 1.0);
}