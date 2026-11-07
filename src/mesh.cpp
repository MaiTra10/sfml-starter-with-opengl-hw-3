#include "mesh.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<Vertex> makePositionVertices(const std::vector<glm::vec3>& positions)
{

    std::vector<Vertex> vertices;
    vertices.reserve(positions.size());

    // Every position becomes a vertex with the same gray color
    for (const glm::vec3& position : positions) {

        Vertex vertex;
        vertex.position = position;
        vertex.color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        vertices.push_back(vertex);

    }

    return vertices;

}

void flipImageVertically(unsigned char* pixels, int width, int height)
{

    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    std::vector<unsigned char> rowCopy(rowBytes);

    // Swap each row in the top half with its mirror row in the bottom half
    for (int rowIndex = 0; rowIndex < height / 2; ++rowIndex) {

        const int mirrorRowIndex = height - 1 - rowIndex;
        unsigned char* topRow = pixels + static_cast<std::size_t>(rowIndex) * rowBytes;
        unsigned char* bottomRow = pixels + static_cast<std::size_t>(mirrorRowIndex) * rowBytes;

        std::copy_n(topRow, rowBytes, rowCopy.data());
        std::copy_n(bottomRow, rowBytes, topRow);
        std::copy_n(rowCopy.data(), rowBytes, bottomRow);

    }

}

void bindAttribute(
    GLuint program,
    const char* name,
    GLint componentCount,
    GLsizei stride,
    std::size_t offset
) {

    // Find where the shader expects this attribute
    const GLint location = glGetAttribLocation(program, name);
    if (location < 0) {

        throw std::runtime_error(
            std::string("Mesh vertex shader has no '") + name + "' attribute"
        );

    }

    // Tell OpenGL where the attribute sits inside each vertex
    const GLuint attribute = static_cast<GLuint>(location);
    glEnableVertexAttribArray(attribute);
    glVertexAttribPointer(
        attribute,
        componentCount,
        GL_FLOAT,
        GL_FALSE,
        stride,
        reinterpret_cast<const void*>(offset)
    );

}

}

Mesh::Mesh(
    const std::vector<Vertex>& vertices,
    const std::vector<unsigned int>& indices,
    const std::string& vertexShaderPath,
    const std::string& fragmentShaderPath,
    const std::string& texturePath,
    const Material& material
)
    : shader(vertexShaderPath, fragmentShaderPath),
        material(material)
{

    // A mesh needs vertices, and an index count that fits in a draw call
    const std::size_t largestIndexCount = static_cast<std::size_t>(std::numeric_limits<GLsizei>::max());
    const bool hasTooManyIndices = indices.size() > largestIndexCount;
    if (vertices.empty() || indices.empty() || hasTooManyIndices) {
        throw std::invalid_argument("A triangle mesh requires vertices and a valid index count");
    }

    // The lighting math needs a shininess above zero
    if (material.shininess <= 0.0f) {
        throw std::invalid_argument("Material shininess must be greater than zero");
    }

    indexCount = static_cast<GLsizei>(indices.size());

    // Every index has to point at a real vertex
    for (const unsigned int index : indices) {
        if (index >= vertices.size()) {
            throw std::invalid_argument("Mesh index is outside the vertex list");
        }
    }

    // Load the texture image if there is one, and flip it so it faces up in OpenGL
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> image(nullptr, stbi_image_free);
    int imageWidth = 0;
    int imageHeight = 0;

    if (!texturePath.empty()) {

        image.reset(stbi_load(texturePath.c_str(), &imageWidth, &imageHeight, nullptr, STBI_rgb_alpha));
        if (!image) {

            const char* reason = stbi_failure_reason();
            const std::string reasonText = (reason != nullptr) ? reason : "unknown image error";

            throw std::runtime_error("Unable to load texture '" + texturePath + "': " + reasonText);

        }

        flipImageVertically(image.get(), imageWidth, imageHeight);

    }

    // Check that the shader has every attribute before making any OpenGL objects
    const GLuint program = shader.id();
    const GLint positionLocation = glGetAttribLocation(program, "position");
    if (positionLocation < 0) {
        throw std::runtime_error("Mesh vertex shader has no 'position' attribute");
    }

    for (const char* attributeName : {"normal", "texCoord", "color"}) {

        const GLint attributeLocation = glGetAttribLocation(program, attributeName);
        if (attributeLocation < 0) {

            throw std::runtime_error(
                std::string("Mesh vertex shader has no '") + attributeName + "' attribute"
            );

        }

    }

    // Upload the vertices to the graphics card
    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    const GLsizeiptr vertexBytes = static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex));
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertexBytes, vertices.data(), GL_STATIC_DRAW);

    // Connect each part of a vertex to its shader input
    const GLsizei vertexStride = static_cast<GLsizei>(sizeof(Vertex));
    bindAttribute(program, "position", 3, vertexStride, offsetof(Vertex, position));
    bindAttribute(program, "normal", 3, vertexStride, offsetof(Vertex, normal));
    bindAttribute(program, "texCoord", 2, vertexStride, offsetof(Vertex, texCoord));
    bindAttribute(program, "color", 4, vertexStride, offsetof(Vertex, color));

    // The element buffer stores the vertex indices in triangle order
    const GLsizeiptr indexBytes = static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int));
    glGenBuffers(1, &indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBytes, indices.data(), GL_STATIC_DRAW);

    // Upload the texture and set how it repeats and gets smoothed
    if (image) {

        glGenTextures(1, &texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            imageWidth,
            imageHeight,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            image.get()
        );
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

    }

    // Unbind everything so later code can't change this mesh by accident
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

}

Mesh::Mesh(
    const std::vector<glm::vec3>& positions,
    const std::vector<unsigned int>& indices,
    const std::string& vertexShaderPath,
    const std::string& fragmentShaderPath,
    const std::string& texturePath,
    const Material& material
)
    : Mesh(
        makePositionVertices(positions),
        indices,
        vertexShaderPath,
        fragmentShaderPath,
        texturePath,
        material
    )
{
}

Mesh::~Mesh()
{

    // Give back everything this mesh asked the graphics card for
    glDeleteVertexArrays(1, &vertexArray);
    glDeleteBuffers(1, &vertexBuffer);
    glDeleteBuffers(1, &indexBuffer);

    if (texture != 0) {
        glDeleteTextures(1, &texture);
    }

}

void Mesh::applyTransform(const glm::mat4& transform)
{

    model *= transform;

}

void Mesh::setTransform(const glm::mat4& transform)
{

    model = transform;

}

void Mesh::draw(
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& lightWorldPosition
) const {

    // The fragment shader works in view space, so move the light there first
    const glm::vec3 lightViewPosition = glm::vec3(view * glm::vec4(lightWorldPosition, 1.0f));

    // Send the matrices, light and material to the shader
    shader.use();
    shader.setMatrix4("model", model);
    shader.setMatrix4("view", view);
    shader.setMatrix4("projection", projection);
    shader.setVector3("lightPosition", lightViewPosition);
    shader.setVector3("materialAmbient", material.ambient);
    shader.setVector3("materialDiffuse", material.diffuse);
    shader.setVector3("materialSpecular", material.specular);
    shader.setFloat("materialShininess", material.shininess);

    // Tell the shader whether to light the mesh and whether to use the texture
    const int lightingFlag = material.lightingEnabled ? 1 : 0;
    const int textureFlag = (texture == 0) ? 0 : 1;
    shader.setInt("useLighting", lightingFlag);
    shader.setInt("useTexture", textureFlag);
    shader.setInt("diffuseTexture", 0);

    // Draw the triangles with the texture bound, then unbind it again
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(vertexArray);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

}
