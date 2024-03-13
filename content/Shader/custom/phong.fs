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
uniform sampler2D z_buffer;
uniform sampler2D shadowMap;
uniform samplerCube shadowCubeMap;

uniform vec2 screen_size;

uniform Texture2D albedo_map;
uniform Texture2D normal_map;
uniform Texture2D parallax_map;

uniform vec3 color;
uniform float normalStrength;
uniform float aoStrength;
uniform float shadowStrength;

uniform bool pointLight;
uniform vec3 lightPos;
uniform vec3 lightDir;
uniform vec3 viewPos;
uniform vec3 lightColor;

uniform float heightScale;

void ZTest(){
    float currentDepth = gl_FragCoord.z * 2 - 1;
    float closestDepth = texture(z_buffer, gl_FragCoord.xy / screen_size).r; 

    if(currentDepth > closestDepth)
        discard;
}

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
{ 
    // number of depth layers
    const float minLayers = 8;
    const float maxLayers = 32;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));  
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy / viewDir.z * heightScale; 
    vec2 deltaTexCoords = P / numLayers;
  
    // get initial values
    vec2  currentTexCoords     = texCoords;
    float currentDepthMapValue = SampleTexture(parallax_map, currentTexCoords).r;
      
    while(currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = SampleTexture(parallax_map, currentTexCoords).r;  
        // get depth of next layer
        currentLayerDepth += layerDepth;  
    }
    
    return currentTexCoords;
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
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


// array of offset direction for sampling
vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);
float PointShadowCalculation(vec3 fragPos)
{
    float far_plane = 10000.0;
    // // get vector between fragment position and light position
    // vec3 fragToLight = fragPos - lightPos;
    // float currentDepth = length(fragToLight);
    // float shadow = 0.0;
    // float bias = 0.15;
    // int samples = 20;
    // float viewDistance = length(viewPos - fragPos);
    // float diskRadius = (1.0 + (viewDistance / far_plane)) / 25.0;
    // for(int i = 0; i < samples; ++i)
    // {
    //     float closestDepth = texture(shadowCubeMap, fragToLight + gridSamplingDisk[i] * diskRadius).r;
    //     closestDepth *= far_plane;   // undo mapping [0;1]
    //     if(currentDepth - bias > closestDepth)
    //         shadow += 1.0;
    // }
    // shadow /= float(samples);
        
    // return shadow;

    // get vector between fragment position and light position
    vec3 fragToLight = fragPos - lightPos;
    // ise the fragment to light vector to sample from the depth map    
    float closestDepth = texture(shadowCubeMap, fragToLight).r;
    // it is currently in linear range between [0,1], let's re-transform it back to original depth value
    closestDepth *= far_plane;
    // now get current linear depth as the length between the fragment and light position
    float currentDepth = length(fragToLight);
    // test for shadows
    float bias = 0.05; // we use a much larger bias since depth is now in [near_plane, far_plane] range
    float shadow = currentDepth -  bias > closestDepth ? 1.0 : 0.0;        
    // display closestDepth as debug (to visualize depth cubemap)
    // FragColor = vec4(vec3(closestDepth / far_plane), 1.0);    
        
    return shadow;
}


void main()
{
    // Parallax map
    vec3 tangentViewDir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);
    vec2 texCoords = ParallaxMapping(fs_in.TexCoords, tangentViewDir);
    if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)
        discard;
    // albedo
    vec3 albedo = SampleTexture(albedo_map, texCoords).rgb * color;
    // Normal map
    vec3 tangentNormal = SampleTexture(normal_map, texCoords).rgb;
    tangentNormal = normalize(tangentNormal * 2.0 - 1.0);

    // ambient
    vec3 ambient = 0.1 * albedo;
    // diffuse & shadow
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

    float diff = max(dot(tangentFrag2LightDir, tangentNormal), 0.0);
    vec3 diffuse = diff * lightColor;
    // specular
    vec3 reflectDir = reflect(-tangentFrag2LightDir, tangentNormal);
    vec3 halfwayDir = normalize(tangentFrag2LightDir + tangentViewDir);  
    float spec = pow(max(dot(tangentNormal, halfwayDir), 0.0), 32.0);
    vec3 specular = vec3(0.2) * spec * lightColor; // assuming bright white light color

    // float shadow = pointLight ? PointShadowCalculation(fs_in.FragPos) :
    //     ShadowCalculation(fs_in.FragPosLightSpace, tangentNormal, tangentFrag2LightDir);
        
    // shadow = pointLight ? 0 :
    //     ShadowCalculation(fs_in.FragPosLightSpace, tangentNormal, tangentFrag2LightDir);

    vec3 lighting = vec3(ambient + (1.0 - shadow) * (diffuse + specular)) * albedo;
    FragColor = vec4(lighting, 1.0);
}