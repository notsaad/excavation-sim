#version 330 core
in vec3 Normal;
out vec4 FragColour;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 soilColour = vec3(0.55, 0.36, 0.2);
    
    float ambient = 0.15;
    float diffuse = max(dot(normalize(Normal), lightDir), 0.0);
    FragColour = vec4((ambient + diffuse) * soilColour, 1.0);
}
