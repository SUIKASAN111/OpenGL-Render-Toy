#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec3 TangentFragPos;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentLightDir;
    vec4 FragPosLightSpace;
    mat3 TBN;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform vec3 viewPos;
uniform mat4 light_view;
uniform mat4 light_projection;

uniform bool pointLight;
uniform vec3 lightPos;
uniform vec3 lightDir;
uniform vec3 lightColor;

uniform vec3 tangent;
uniform vec3 bitangent;

mat3 getTBN(vec3 tangent, vec3 bitangent, vec3 normal){
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 T = normalize(normalMatrix * tangent);
    vec3 N = normalize(normalMatrix * normal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    mat3 TBN = transpose(mat3(T, B, N)); 
    return TBN;
}

void main(){
    vs_out.FragPos = vec3(model * vec4(aPos, 1.0));
    vs_out.Normal = aNormal;
    vs_out.TexCoords = aTexCoords;
    mat3 TBN = getTBN(aTangent, aBitangent, aNormal);
    vs_out.TBN = TBN;
    vs_out.TangentFragPos  = TBN * vs_out.FragPos;
    vs_out.TangentLightPos = TBN * lightPos;
    vs_out.TangentViewPos  = TBN * viewPos;
    vs_out.TangentLightDir = TBN * lightDir;
    vs_out.FragPosLightSpace = light_projection * light_view * vec4(vs_out.FragPos, 1.0);
    gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
}