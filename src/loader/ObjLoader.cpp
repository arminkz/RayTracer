#include "ObjLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>


namespace ObjLoader {

    HostMesh load(const std::string& modelPath) {

        HostMesh mesh;

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        // Load the OBJ file
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) {
            throw std::runtime_error("Failed to load OBJ file: " + warn + " " + err);
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        // Process vertices and indices
        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {

                Vertex vertex{};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Default color (white)

                // if the vertex is not already in the map, add it
                // and assign it a unique index
                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(mesh.vertices.size()); //store the index of the vertex in the mesh vector
                    mesh.vertices.push_back(vertex);
                }

                mesh.indices.push_back(uniqueVertices[vertex]);
            }
        }

        return mesh;
    }
    
}