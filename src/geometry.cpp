#include "geometry.h"

#include <cmath>

namespace {

constexpr float pi = 3.14159265358979323846f;

void addCuboidFace(
    Geometry& cuboid,
    const glm::vec3& faceCenter,
    const glm::vec3& halfRight,
    const glm::vec3& halfUp
) {

    // The face points away from the middle of the cuboid
    const glm::vec3 normal = glm::normalize(faceCenter);

    // The four corners, going around the face
    const glm::vec3 corners[4] = {

        faceCenter - halfRight - halfUp,
        faceCenter + halfRight - halfUp,
        faceCenter + halfRight + halfUp,
        faceCenter - halfRight + halfUp

    };
    const glm::vec2 textureCoords[4] = {

        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec2(0.0f, 1.0f)

    };

    // Every face keeps its own vertices so the edges stay sharp
    const unsigned int firstIndex = static_cast<unsigned int>(cuboid.vertices.size());
    for (int corner = 0; corner < 4; ++corner) {

        Vertex vertex;
        vertex.position = corners[corner];
        vertex.normal = normal;
        vertex.texCoord = textureCoords[corner];
        cuboid.vertices.push_back(vertex);

    }

    // Two triangles make up the face
    cuboid.indices.insert(
        cuboid.indices.end(),
        {firstIndex, firstIndex + 1, firstIndex + 2, firstIndex, firstIndex + 2, firstIndex + 3}
    );

}

}

void append(Geometry& destination, const Geometry& source)
{

    // The new triangles need to skip past the vertices already stored
    const unsigned int indexOffset = static_cast<unsigned int>(destination.vertices.size());

    // Copy the vertices as they are
    destination.vertices.insert(
        destination.vertices.end(),
        source.vertices.begin(),
        source.vertices.end()
    );

    // Copy the triangles with the shifted indices
    for (const unsigned int index : source.indices) {
        destination.indices.push_back(index + indexOffset);
    }

}

Geometry makeSphere(float radius, unsigned int slices, unsigned int stacks)
{

    Geometry sphere;

    // One ring of vertices for each stack, from the top pole to the bottom pole
    for (unsigned int stack = 0; stack <= stacks; ++stack) {
        const float verticalFraction = static_cast<float>(stack) / static_cast<float>(stacks);
        const float theta = pi * verticalFraction;
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);

        for (unsigned int slice = 0; slice <= slices; ++slice) {
            const float horizontalFraction = static_cast<float>(slice) / static_cast<float>(slices);
            const float phi = 2.0f * pi * horizontalFraction;

            // On a sphere the normal is just the direction away from the center
            const glm::vec3 normal(
                sinTheta * std::cos(phi),
                cosTheta,
                sinTheta * std::sin(phi)
            );

            Vertex vertex;
            vertex.position = normal * radius;
            vertex.normal = normal;
            vertex.texCoord = glm::vec2(horizontalFraction, 1.0f - verticalFraction);
            sphere.vertices.push_back(vertex);
        }
    }

    // Two triangles for every square between neighbouring rings
    const unsigned int rowLength = slices + 1;
    for (unsigned int stack = 0; stack < stacks; ++stack) {
        for (unsigned int slice = 0; slice < slices; ++slice) {

            const unsigned int topLeft = stack * rowLength + slice;
            const unsigned int bottomLeft = topLeft + rowLength;
            const unsigned int topRight = topLeft + 1;
            const unsigned int bottomRight = bottomLeft + 1;

            sphere.indices.insert(
                sphere.indices.end(),
                {topLeft, topRight, bottomLeft, topRight, bottomRight, bottomLeft}
            );
        }
    }

    return sphere;

}

Geometry makeCylinderSide(float radius, float height, unsigned int slices, bool scaleTexture)
{

    Geometry side;

    // Distance around the cylinder, used to keep the texture from stretching
    const float circumference = 2.0f * pi * radius;

    // Two rings of vertices, one at the bottom and one at the top
    for (unsigned int ring = 0; ring <= 1; ++ring) {
        const float y = static_cast<float>(ring) * height;

        // The first and last vertex of a ring share a position but not a texture coordinate
        for (unsigned int slice = 0; slice <= slices; ++slice) {

            const float horizontalFraction = static_cast<float>(slice) / static_cast<float>(slices);
            const float angle = 2.0f * pi * horizontalFraction;
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);

            // Either stretch the texture over the height or repeat it with square texels
            const float textureHeight = scaleTexture ? y / circumference : static_cast<float>(ring);

            Vertex vertex;
            vertex.position = glm::vec3(radius * cosine, y, radius * sine);
            vertex.normal = glm::vec3(cosine, 0.0f, sine);
            vertex.texCoord = glm::vec2(horizontalFraction, textureHeight);
            side.vertices.push_back(vertex);

        }
    }

    // Two triangles for every slice
    const unsigned int rowLength = slices + 1;
    for (unsigned int slice = 0; slice < slices; ++slice) {

        const unsigned int bottomLeft = slice;
        const unsigned int bottomRight = slice + 1;
        const unsigned int topLeft = slice + rowLength;
        const unsigned int topRight = slice + 1 + rowLength;

        side.indices.insert(
            side.indices.end(),
            {bottomLeft, bottomRight, topLeft, bottomRight, topRight, topLeft}
        );

    }

    return side;

}

Geometry makeDisk(float radius, float height, bool facingUp, unsigned int slices)
{

    Geometry disk;
    const glm::vec3 normal(0.0f, facingUp ? 1.0f : -1.0f, 0.0f);

    // The center vertex comes first
    Vertex center;
    center.position = glm::vec3(0.0f, height, 0.0f);
    center.normal = normal;
    center.texCoord = glm::vec2(0.5f, 0.5f);
    disk.vertices.push_back(center);

    // Then the vertices around the edge, mapped onto a circle inside the texture
    for (unsigned int slice = 0; slice <= slices; ++slice) {

        const float angle = 2.0f * pi * static_cast<float>(slice) / static_cast<float>(slices);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);

        Vertex vertex;
        vertex.position = glm::vec3(radius * cosine, height, radius * sine);
        vertex.normal = normal;
        vertex.texCoord = glm::vec2(0.5f + 0.5f * cosine, 0.5f + 0.5f * sine);
        disk.vertices.push_back(vertex);

    }

    // One triangle for every slice, fanning out from the center
    for (unsigned int slice = 0; slice < slices; ++slice) {

        disk.indices.insert(disk.indices.end(), {0, slice + 1, slice + 2});

    }

    return disk;

}

Geometry makeCappedCylinder(float radius, float height, unsigned int slices, bool scaleTexture)
{

    // The wall first, then a disk to close each end
    Geometry cylinder = makeCylinderSide(radius, height, slices, scaleTexture);
    append(cylinder, makeDisk(radius, 0.0f, false, slices));
    append(cylinder, makeDisk(radius, height, true, slices));

    return cylinder;

}

Geometry makeCuboid(float sizeX, float sizeY, float sizeZ)
{

    Geometry cuboid;

    // Half of each size, as a vector pointing along its axis
    const glm::vec3 halfX(sizeX * 0.5f, 0.0f, 0.0f);
    const glm::vec3 halfY(0.0f, sizeY * 0.5f, 0.0f);
    const glm::vec3 halfZ(0.0f, 0.0f, sizeZ * 0.5f);

    // Six faces, each one sits a half size away from the center
    addCuboidFace(cuboid, halfX, halfZ, halfY);
    addCuboidFace(cuboid, -halfX, halfZ, halfY);
    addCuboidFace(cuboid, halfY, halfX, halfZ);
    addCuboidFace(cuboid, -halfY, halfX, halfZ);
    addCuboidFace(cuboid, halfZ, halfX, halfY);
    addCuboidFace(cuboid, -halfZ, halfX, halfY);

    return cuboid;

}
