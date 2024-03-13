#version 330 core
layout (location = 0) out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec3 TangentFragPos;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentLightDir;
    vec4 FragPosLightSpace;
    mat3 TBN;
} fs_in;

struct Texture2D
{
    sampler2D texture;
    vec2 tilling;
    vec2 offset;
};

vec4 SampleTexture(Texture2D tex, vec2 uv)
{
    return texture(tex.texture, vec2(uv.xy * tex.tilling) + tex.offset);
}

uniform sampler2D depthTexture;
uniform sampler2D shadowMap;
uniform samplerCube shadowCubeMap;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUTTexture;

uniform Texture2D albedo_map;
uniform Texture2D normal_map;
uniform Texture2D metallic_map;
uniform Texture2D roughness_map;
uniform Texture2D ao_map;

uniform vec3 color;
uniform float roughnessStrength;
uniform float metallicStrength;
uniform float aoStrength;
uniform float shadowStrength;

uniform bool skybox_enabled;
uniform bool pointLight;
uniform vec3 lightPos;
uniform vec3 lightDir;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform float lightIntensity;

uniform float heightScale;

const float PI = 3.14159265359;
// ----------------------------------------------------------------------------
vec3 getNormalFromMap()
{
    vec3 tangentNormal = SampleTexture(normal_map, fs_in.TexCoords).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(fs_in.FragPos);
    vec3 Q2  = dFdy(fs_in.FragPos);
    vec2 st1 = dFdx(fs_in.TexCoords);
    vec2 st2 = dFdy(fs_in.TexCoords);

    vec3 N = normalize(fs_in.Normal);
    vec3 T = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   
// ----------------------------------------------------------------------------
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float currentDepth = projCoords.z;
    float bias = max(0.0001 * (1.0 - dot(normal, lightDir)), 0.00001);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    if(projCoords.z > 1.0)
        shadow = 0.0;
    return shadow * shadowStrength;
}

void main(){
    vec3 albedo = pow(SampleTexture(albedo_map, fs_in.TexCoords).rgb * color, vec3(2.2));
    float metallic  = SampleTexture(metallic_map, fs_in.TexCoords).r * metallicStrength;
    float roughness = SampleTexture(roughness_map, fs_in.TexCoords).r * roughnessStrength;
    float ao = SampleTexture(ao_map, fs_in.TexCoords).r * aoStrength;
    metallic = clamp (metallic, 0.0, 1.0);
    roughness = clamp (roughness, 0.0, 1.0);
    ao = clamp(ao, 0.0, 1.0);

    vec3 N = getNormalFromMap();
    vec3 V = normalize(viewPos - fs_in.FragPos);
    vec3 R = reflect(-V, N);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);
    
    // calculate per-light radiance
    vec3 L = lightDir;
    if(pointLight){
        L = normalize(lightPos - fs_in.FragPos);
    }
    vec3 H = normalize(V + L);

    // consider attenuation for point light
    float attenuation = 1.0;
    if(pointLight){
        float distance = length(lightPos - fs_in.FragPos);
        attenuation = 1.0 / (distance * distance);
    }

    vec3 radiance = lightColor * lightIntensity * attenuation;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
    vec3 numerator = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;
    
    // kS is equal to Fresnel
    vec3 kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD = vec3(1.0) - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;

    // scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);        

    // add to outgoing radiance Lo
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again 
    
    vec3 ambient = vec3(0.03) * albedo * ao;
    if(skybox_enabled){
        vec3 kS_ambient = fresnelSchlick(max(dot(N, V), 0.0), F0);
        vec3 kD_ambient = 1.0 - kS;
        kD_ambient *= 1 - metallic;
        vec3 irradiance = texture(irradianceMap, N).rgb;
        vec3 diffuse_ambient = irradiance * albedo;

        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;    
        vec2 brdf = texture(brdfLUTTexture, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular_ambient = prefilteredColor * (F * brdf.x + brdf.y);

        // ambient lighting
        // vec3 ambient = vec3(0.03) * albedo * ao;
        ambient = (kD_ambient * diffuse_ambient + specular_ambient) * ao;
    }

    // Shadow
    vec3 tangentNormal = SampleTexture(normal_map, fs_in.TexCoords).rgb;
    tangentNormal = normalize(tangentNormal * 2.0 - 1.0);
    vec3 tangentFrag2LightDir;
    float shadow = 0.0;
    if(pointLight){
        tangentFrag2LightDir = normalize(fs_in.TangentLightPos - fs_in.TangentFragPos);
        // shadow = PointShadowCalculation(fs_in.FragPos);
    }
    else{
        tangentFrag2LightDir = fs_in.TangentLightDir;
        shadow = ShadowCalculation(fs_in.FragPosLightSpace, tangentNormal, tangentFrag2LightDir);
    }

    vec3 outcolor = (Lo + ambient) * (1.0 - shadow) + ambient * shadow;

    // Since the renderer implemented gamma correction in postprocessing
    // No Need for us to do gamma correction here
    // // HDR tonemapping
    // outcolor = outcolor / (outcolor + vec3(1.0));
    // // gamma correct
    // outcolor = pow(outcolor, vec3(1.0/2.2)); 

    FragColor = vec4(outcolor, 1.0);
}