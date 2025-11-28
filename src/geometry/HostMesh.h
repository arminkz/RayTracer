#pragma once
#include "../stdafx.h"
#include "Vertex.h"

// Mesh representation on CPU
class HostMesh {
public:
    std::vector<Vertex> vertices;  // Vertex data
    std::vector<uint32_t> indices; // Index data
    
    HostMesh() = default;
    HostMesh(const HostMesh& other): vertices(other.vertices), indices(other.indices) {} // copy constructor
};