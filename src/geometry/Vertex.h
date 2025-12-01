#pragma once
#include "../stdafx.h"

struct Vertex {
    glm::vec3 pos; float pad0; // Padding to align to 16 bytes
    glm::vec3 normal; float pad1; // Padding to align to 16 bytes

    bool operator==(const Vertex& other) const;
};


// Hash function for Vertex struct
namespace std {
    // Specialize std::hash for Vertex to allow it to be used in unordered_map and unordered_set
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            size_t h1 = hash<glm::vec3>()(vertex.pos);
            size_t h2 = hash<glm::vec3>()(vertex.normal);

            // Combine hashes (a common pattern for combining multiple hashes)
            size_t combined = h1;
            combined ^= h2 + 0x9e3779b9 + (combined << 6) + (combined >> 2);

            return combined;
        }
    };
}