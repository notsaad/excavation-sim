#version 330 core
in vec3 Normal;
out vec4 FragColour;

uniform vec3 objectColour;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    
    float ambient = 0.15;
    float diffuse = max(dot(normalize(Normal), lightDir), 0.0);
    FragColour = vec4((ambient + diffuse) * objectColour, 1.0);
}
