#pragma once
#include "../stdafx.h"

struct Vertex {
    glm::vec3 pos;

    bool operator==(const Vertex& other) const;
};


// Hash function for Vertex struct
namespace std {
    // Specialize std::hash for Vertex to allow it to be used in unordered_map and unordered_set
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            size_t h1 = hash<glm::vec3>()(vertex.pos);

            // Combine hashes (a common pattern for combining multiple hashes)
            size_t combined = h1;

            return combined;
        }
    };
}