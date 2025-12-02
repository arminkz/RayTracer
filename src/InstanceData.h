#pragma once
#include "stdafx.h"

// Instance data stored on GPU, accessible by shaders via buffer device address
struct InstanceData {
    uint64_t vertexBufferAddress;  // Device address of vertex buffer
    uint64_t indexBufferAddress;   // Device address of index buffer
    uint32_t materialType;              // Material type identifier
    glm::vec3 color;               // Material color
    float metallic;                // Metallic property
    float roughness;               // Roughness property
    float _pad0;                   // Padding for alignment
    float _pad1;
    //float _pad2;                   // Padding for alignment
};

static_assert(sizeof(InstanceData) == 48, "InstanceData size must be 48 bytes");
