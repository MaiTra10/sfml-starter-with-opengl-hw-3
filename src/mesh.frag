#version 400

in vec3 fragmentPosition;
in vec3 fragmentNormal;
in vec2 fragmentTexCoord;
in vec4 vertexColor;

uniform vec3 lightPosition;
uniform vec3 materialAmbient;
uniform vec3 materialDiffuse;
uniform vec3 materialSpecular;
uniform float materialShininess;
uniform int useLighting;
uniform sampler2D diffuseTexture;
uniform int useTexture;

out vec4 outputColor;

void main() {
    vec4 texel = useTexture == 1
        ? texture(diffuseTexture, fragmentTexCoord)
        : vec4(1.0);
    vec3 baseColor = vertexColor.rgb * texel.rgb;
    if (useLighting == 0) {
        outputColor = vec4(baseColor, vertexColor.a * texel.a);
        return;
    }

    // Directions are all worked out per fragment, in view space
    vec3 normalDirection = normalize(fragmentNormal);
    vec3 lightDirection = normalize(lightPosition - fragmentPosition);
    vec3 viewDirection = normalize(-fragmentPosition);
    vec3 reflectionDirection = reflect(-lightDirection, normalDirection);

    // Surfaces facing the light are brighter
    float diffuseStrength = max(dot(normalDirection, lightDirection), 0.0);

    // The shiny highlight only shows on surfaces the light actually reaches
    float specularStrength = 0.0;
    if (diffuseStrength > 0.0) {
        specularStrength = pow(max(dot(reflectionDirection, viewDirection), 0.0),
                               materialShininess);
    }

    // The texture color scales the diffuse color, plain parts just use their material
    vec3 litColor = baseColor * (materialAmbient + materialDiffuse * diffuseStrength)
        + materialSpecular * specularStrength;
    outputColor = vec4(litColor, vertexColor.a * texel.a);
}
