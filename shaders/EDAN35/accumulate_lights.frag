#version 410

uniform sampler2D depth_texture;
uniform sampler2D normal_texture;
uniform sampler2DShadow shadow_texture;

uniform vec2 inv_res;

uniform mat4 view_projection_inverse;
uniform vec3 camera_position;
uniform mat4 shadow_view_projection;

uniform vec3 light_color;
uniform vec3 light_direction;
uniform float light_intensity;

uniform vec2 shadowmap_texel_size;

layout (location = 0) out vec4 light_diffuse_contribution;
layout (location = 1) out vec4 light_specular_contribution;

void main()
{
	vec2 uv = inv_res * gl_FragCoord.xy;
	vec3 normal_im = texture(normal_texture, uv).xyz;
	vec3 normal = normal_im * 2.0 - 1.0; // convert from [0,1] to [-1, 1]

	float depth = texture(depth_texture, uv).r;

	// Convert xyz from [0,1] to [-1,1] to use view projection inverse
	vec4 screenSpacePos = vec4(uv.x * 2.0 - 1.0, uv.y*2.0 - 1.0, 2.0*depth - 1.0, 1.0);
	vec4 worldPos = view_projection_inverse * screenSpacePos;
	//worldPos /= worldPos.w;

	
	vec3 p = worldPos.xyz;

	// Where direction only matters.
	vec3 n = normalize(normal);
	vec3 v = normalize(camera_position - p);
	vec3 l = normalize(-light_direction);
	vec3 r = normalize(reflect(-l, n));

	float diffuse = dot(n, l);
	float specular = max(pow(dot(r,v), 100), 0); // QUESTION: Where is the specular map integrated?

	// Implement shadow
	vec4 projectedSampler = shadow_view_projection * view_projection_inverse * screenSpacePos;
	projectedSampler /= projectedSampler.w;
	float shadowMultiplier = 0.0;
	vec3 sampler_centre = (projectedSampler.xyz + 1.0) / 2.0;

	
	//if (sampler_centre.x >= 0 && sampler_centre.x <= 1.0 && sampler_centre.y >= 0 && sampler_centre.y <= 1.0) {
	//	shadowMultiplier = texture(shadow_texture, sampler_centre);
	//}
	
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
	
	vec4 lightColour = shadowMultiplier * vec4(light_color, 1.0);

	light_diffuse_contribution  = diffuse * lightColour;
	light_specular_contribution = specular * lightColour;
}
