#version 410

uniform bool has_diffuse_texture;
uniform bool has_specular_texture;
uniform bool has_normals_texture;
uniform bool has_opacity_texture;
uniform bool has_cubemap_texture;
uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D normals_texture;
uniform sampler2D opacity_texture;

uniform sampler2D underwater_texture;
uniform samplerCube cubemap_texture;
uniform sampler2D underwater_depth_texture;
uniform sampler2D reflection_texture;
uniform mat4 normal_model_to_world;
uniform vec3 camera_position;
uniform mat4 shadow_view_projection;

uniform vec3 sun_dir;

uniform vec2 inv_res;
uniform float t;

uniform bool IN_WATER;

in VS_OUT {
    vec3 refractedDir[3];
    float reflectionFactor;
    float renderFromBelow;
    vec3 reflected;
    vec3 extra;
    float waveHeight;
} fs_in;


float lineariseDepth(float value)
{
	return (2.0 * 0.01) / (30 + 0.01 - value * (30 - 0.01));
}

out vec4 colour;

uniform vec3 atmosphereColour;
uniform vec3 underwaterColour;

void main()
{
    vec3 result;
    vec3 reflectedColor = vec3(1.);
    vec3 refractedColor = vec3(1.);
    // Rendering bottom side of water
    if (fs_in.renderFromBelow < 0) 
    {
        float dotA = dot(fs_in.refractedDir[0], fs_in.refractedDir[0]);
        float dotB = dot(fs_in.refractedDir[1], fs_in.refractedDir[1]);
        float dotC = dot(fs_in.refractedDir[2], fs_in.refractedDir[2]);

        // sample relfection based on wave heuristic
        vec2 refUV = gl_FragCoord.xy * inv_res;
        reflectedColor = texture2D(reflection_texture, vec2(1 - refUV.x, refUV.y) + fs_in.waveHeight * normalize(fs_in.reflected.xz)).rgb;

        // sample from cubemap
        refractedColor.r = texture(cubemap_texture, fs_in.refractedDir[0]).r;
        refractedColor.g = texture(cubemap_texture, fs_in.refractedDir[1]).g;
        refractedColor.b = texture(cubemap_texture, fs_in.refractedDir[2]).b;
        
        // when faced with total reflection.
        if (dotA == 0)
            refractedColor.r = reflectedColor.r;
        if (dotB == 0)
            refractedColor.g = reflectedColor.g;
        if (dotC == 0)
            refractedColor.b = reflectedColor.b;

        refractedColor = mix(refractedColor, underwaterColour, IN_WATER ? 0.2 : 0.4);
    }
    else // Rendering top side of water
    {
        //sample reflection in cubemap
        reflectedColor = texture(cubemap_texture, fs_in.reflected).xyz;

        // Color coming from the environment refraction, applying chromatic aberration
        refractedColor.r = texture2D(underwater_texture, fs_in.refractedDir[0].xy * 0.5 + 0.5).r;
        refractedColor.g = texture2D(underwater_texture, fs_in.refractedDir[1].xy * 0.5 + 0.5).g;
        refractedColor.b = texture2D(underwater_texture, fs_in.refractedDir[2].xy * 0.5 + 0.5).b;
    }

    result = mix(refractedColor, reflectedColor, clamp(fs_in.reflectionFactor, 0., 1.));

    if (IN_WATER) {
        result = mix(result, underwaterColour, 0.2);
    }
    //result = fs_in.refractedDir[0];
    colour = vec4(result, 1);
}
