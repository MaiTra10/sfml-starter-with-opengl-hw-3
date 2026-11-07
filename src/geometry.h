#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "mesh.h"

// A list of vertices and the triangles that connect them.
struct Geometry {

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

};

// Copies the source vertices and triangles onto the end of the destination.
void append(Geometry& destination, const Geometry& source);

// A sphere centered on the origin.
Geometry makeSphere(float radius, unsigned int slices, unsigned int stacks);

// The curved wall of a cylinder standing on the origin, from y = 0 up to y = height.
// When scaleTexture is true the texture repeats along the length so texels stay square.
Geometry makeCylinderSide(float radius, float height, unsigned int slices, bool scaleTexture);

// A flat circle at the given height, facing up or down.
Geometry makeDisk(float radius, float height, bool facingUp, unsigned int slices);

// A cylinder with a wall and a disk on both ends.
Geometry makeCappedCylinder(float radius, float height, unsigned int slices, bool scaleTexture);

// A box centered on the origin.
Geometry makeCuboid(float sizeX, float sizeY, float sizeZ);
