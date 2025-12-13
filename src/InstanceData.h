#pragma once
#include "stdafx.h"

// Instance data stored on GPU, accessible by shaders via buffer device address
struct InstanceData {
    uint64_t vertexBufferAddress;  // Device address of vertex buffer
    uint64_t indexBufferAddress;   // Device address of index buffer
    uint32_t materialType;         // Material type identifier (used for specially rendered objects)
    glm::vec3 color;               // Material color
    float metallic;                // Metallic property (0 = non-metal, 1 = metal)
    float roughness;               // Roughness property
    float transparency;            // Transparency (0 = opaque, 1 = fully transparent)
    float ior;                     // Index of refraction (e.g., 1.5 for glass, 1.33 for water)
    glm::vec3 absorbance;          // Used for translucent objects (Beer-Lambert law)
    float pad;
};

static_assert(sizeof(InstanceData) % 16 == 0, "InstanceData size must be multiple of 16 bytes");
