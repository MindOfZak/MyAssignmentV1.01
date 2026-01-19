#version 410

in vec3 fragPos;
in vec3 normal;

uniform vec3 lightPos;
uniform vec3 viewPos;

// Base colour for the floor (grey by default)
uniform vec3 baseColor = vec3(0.6, 0.6, 0.6);

out vec4 colour_out;

void main()
{
    vec3 colour = baseColor;

    // 1. ambient
    vec3 ambient = 0.08 * colour;

    // 2. diffuse
    vec3 lightDir = normalize(lightPos - fragPos);
    vec3 norm = normalize(normal);
    float diff = max(dot(lightDir, norm), 0.0);
    vec3 diffuse = diff * colour;

    // 3. specular (Blinn-Phong)
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 16.0);
    vec3 specular = vec3(0.2) * spec;

    colour_out = vec4(ambient + diffuse + specular, 1.0);
}
