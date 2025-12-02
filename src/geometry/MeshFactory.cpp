#include "MeshFactory.h"
#include "Vertex.h"
#include "HostMesh.h"

namespace MeshFactory {

    HostMesh createSphereMesh(float radius, int segments, int rings, bool skySphere)
    {
        HostMesh mesh;

        // Add north pole vertex (single vertex at top)
        Vertex northPole;
        northPole.pos = { 0.0f, 0.0f, radius };
        northPole.normal = glm::normalize(northPole.pos);
        // northPole.color = glm::vec4(1.f);
        // northPole.texCoord = { 0.5f, 0.0f };
        // northPole.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        mesh.vertices.push_back(northPole);

        // Generate intermediate ring vertices (excluding poles)
        for (int y = 1; y < rings; ++y) {
            float v = (float)y / rings;
            float theta = v * glm::pi<float>();

            for (int x = 0; x <= segments; ++x) {
                float u = (float)x / segments;
                float phi = u * 2.0f * glm::pi<float>();

                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);
                float sinPhi = std::sin(phi);
                float cosPhi = std::cos(phi);

                Vertex vert;
                vert.pos = {
                    radius * sinTheta * cosPhi,
                    radius * sinTheta * sinPhi,
                    radius * cosTheta
                };
                vert.normal = glm::normalize(vert.pos);
                // vert.color = glm::vec4(1.f);
                // vert.texCoord = { u, v };
                // vert.tangent = glm::normalize(glm::vec3(-sinPhi, cosPhi, 0.0));
                mesh.vertices.push_back(vert);
            }
        }

        // Add south pole vertex (single vertex at bottom)
        Vertex southPole;
        southPole.pos = { 0.0f, 0.0f, -radius };
        southPole.normal = glm::normalize(southPole.pos);
        // southPole.color = glm::vec4(1.f);
        // southPole.texCoord = { 0.5f, 1.0f };
        // southPole.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        mesh.vertices.push_back(southPole);

        // Generate indices

        // North pole triangles (triangle fan)
        for (int x = 0; x < segments; ++x) {
            int i0 = 0; // North pole
            int i1 = 1 + x;
            int i2 = 1 + x + 1;

            if (skySphere) {
                mesh.indices.push_back(i0);
                mesh.indices.push_back(i1);
                mesh.indices.push_back(i2);
            } else {
                mesh.indices.push_back(i0);
                mesh.indices.push_back(i2);
                mesh.indices.push_back(i1);
            }
        }

        // Middle rings (quads split into two triangles)
        for (int y = 0; y < rings - 2; ++y) {
            for (int x = 0; x < segments; ++x) {
                int i0 = 1 + y * (segments + 1) + x;
                int i1 = i0 + 1;
                int i2 = i0 + (segments + 1);
                int i3 = i2 + 1;

                if (skySphere) {
                    mesh.indices.push_back(i0);
                    mesh.indices.push_back(i1);
                    mesh.indices.push_back(i2);

                    mesh.indices.push_back(i1);
                    mesh.indices.push_back(i3);
                    mesh.indices.push_back(i2);
                } else {
                    mesh.indices.push_back(i0);
                    mesh.indices.push_back(i2);
                    mesh.indices.push_back(i1);

                    mesh.indices.push_back(i1);
                    mesh.indices.push_back(i2);
                    mesh.indices.push_back(i3);
                }
            }
        }

        // South pole triangles (triangle fan)
        int southPoleIndex = static_cast<int>(mesh.vertices.size()) - 1;
        int lastRingStart = 1 + (rings - 2) * (segments + 1);

        for (int x = 0; x < segments; ++x) {
            int i0 = southPoleIndex; // South pole
            int i1 = lastRingStart + x;
            int i2 = lastRingStart + x + 1;

            if (skySphere) {
                mesh.indices.push_back(i0);
                mesh.indices.push_back(i2);
                mesh.indices.push_back(i1);
            } else {
                mesh.indices.push_back(i0);
                mesh.indices.push_back(i1);
                mesh.indices.push_back(i2);
            }
        }

        return mesh;
    }

    HostMesh createBoxMesh(float width, float height, float depth) {
        HostMesh mesh;

        float hw = width / 2.0f;   // half width
        float hh = height / 2.0f;  // half height
        float hd = depth / 2.0f;   // half depth

        // Each face needs its own vertices because normals differ per face
        // Front face (+Z)
        mesh.vertices.push_back({{ -hw, -hh,  hd }, 0.0f, { 0.0f, 0.0f, 1.0f }, 0.0f});
        mesh.vertices.push_back({{  hw, -hh,  hd }, 0.0f, { 0.0f, 0.0f, 1.0f }, 0.0f});
        mesh.vertices.push_back({{  hw,  hh,  hd }, 0.0f, { 0.0f, 0.0f, 1.0f }, 0.0f});
        mesh.vertices.push_back({{ -hw,  hh,  hd }, 0.0f, { 0.0f, 0.0f, 1.0f }, 0.0f});

        // Back face (-Z)
        mesh.vertices.push_back({{  hw, -hh, -hd }, 0.0f, { 0.0f, 0.0f, -1.0f }, 0.0f});
        mesh.vertices.push_back({{ -hw, -hh, -hd }, 0.0f, { 0.0f, 0.0f, -1.0f }, 0.0f});
        mesh.vertices.push_back({{ -hw,  hh, -hd }, 0.0f, { 0.0f, 0.0f, -1.0f }, 0.0f});
        mesh.vertices.push_back({{  hw,  hh, -hd }, 0.0f, { 0.0f, 0.0f, -1.0f }, 0.0f});

        // Left face (-X)
        mesh.vertices.push_back({{ -hw, -hh, -hd }, 0.0f, { -1.0f, 0.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{ -hw, -hh,  hd }, 0.0f, { -1.0f, 0.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{ -hw,  hh,  hd }, 0.0f, { -1.0f, 0.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{ -hw,  hh, -hd }, 0.0f, { -1.0f, 0.0f, 0.0f }, 0.0f});

        // Right face (+X)
        mesh.vertices.push_back({{  hw, -hh,  hd }, 0.0f, { 1.0f, 0.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{  hw, -hh, -hd }, 0.0f, { 1.0f, 0.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{  hw,  hh, -hd }, 0.0f, { 1.0f, 0.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{  hw,  hh,  hd }, 0.0f, { 1.0f, 0.0f, 0.0f }, 0.0f});

        // Top face (+Y)
        mesh.vertices.push_back({{ -hw,  hh,  hd }, 0.0f, { 0.0f, 1.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{  hw,  hh,  hd }, 0.0f, { 0.0f, 1.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{  hw,  hh, -hd }, 0.0f, { 0.0f, 1.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{ -hw,  hh, -hd }, 0.0f, { 0.0f, 1.0f, 0.0f }, 0.0f});

        // Bottom face (-Y)
        mesh.vertices.push_back({{ -hw, -hh, -hd }, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{  hw, -hh, -hd }, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{  hw, -hh,  hd }, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f});
        mesh.vertices.push_back({{ -hw, -hh,  hd }, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f});

        // Indices - each face is 2 triangles (counter-clockwise winding)
        for (uint32_t i = 0; i < 6; i++) {
            uint32_t base = i * 4;
            // First triangle
            mesh.indices.push_back(base + 0);
            mesh.indices.push_back(base + 1);
            mesh.indices.push_back(base + 2);
            // Second triangle
            mesh.indices.push_back(base + 0);
            mesh.indices.push_back(base + 2);
            mesh.indices.push_back(base + 3);
        }

        return mesh;
    }

    HostMesh createPyramidMesh(float baseWidth, float baseDepth, float height) {
        HostMesh mesh;

        float hw = baseWidth * 0.5f;
        float hd = baseDepth * 0.5f;

        // Base vertices (flat normal)
        mesh.vertices.push_back({{-hw, 0, -hd}, 0.0f, {0, -1, 0}, 0.0f}); // 0
        mesh.vertices.push_back({{ hw, 0, -hd}, 0.0f, {0, -1, 0}, 0.0f}); // 1
        mesh.vertices.push_back({{ hw, 0,  hd}, 0.0f, {0, -1, 0}, 0.0f}); // 2
        mesh.vertices.push_back({{-hw, 0,  hd}, 0.0f, {0, -1, 0}, 0.0f}); // 3

        // Base triangles
        mesh.indices.insert(mesh.indices.end(), {0,1,2, 0,2,3});

        // Side faces (each with own normal)
        auto addSide = [&](glm::vec3 a, glm::vec3 b, glm::vec3 apex) {
            glm::vec3 n = -glm::normalize(glm::cross(b - a, apex - a));

            uint32_t i0 = mesh.vertices.size();
            mesh.vertices.push_back({a, 0.0f, n, 0.0f});

            uint32_t i1 = mesh.vertices.size();
            mesh.vertices.push_back({b, 0.0f, n, 0.0f});

            uint32_t i2 = mesh.vertices.size();
            mesh.vertices.push_back({apex, 0.0f, n, 0.0f});

            mesh.indices.insert(mesh.indices.end(), {i0, i1, i2});
        };

        glm::vec3 v0{-hw, 0, -hd};
        glm::vec3 v1{ hw, 0, -hd};
        glm::vec3 v2{ hw, 0,  hd};
        glm::vec3 v3{-hw, 0,  hd};
        glm::vec3 apex{0, height, 0};

        // Create 4 correctly-normaled side faces
        addSide(v0, v1, apex);
        addSide(v1, v2, apex);
        addSide(v2, v3, apex);
        addSide(v3, v0, apex);

        return mesh;
    }

    HostMesh createDoughnutMesh(float innerRadius, float outerRadius, int segments, int tubeSegments) {
        HostMesh mesh;

        float segmentStep = 2.0f * glm::pi<float>() / static_cast<float>(segments);
        float tubeStep = 2.0f * glm::pi<float>() / static_cast<float>(tubeSegments);
        float tubeRadius = (outerRadius - innerRadius) / 2.0f;
        float centerRadius = innerRadius + tubeRadius;

        for (int i = 0; i <= segments; ++i) {
            float segmentAngle = i * segmentStep;
            glm::vec3 segmentCenter = {
                centerRadius * std::cos(segmentAngle),
                0.0f,
                centerRadius * std::sin(segmentAngle)
            };

            for (int j = 0; j <= tubeSegments; ++j) {
                float tubeAngle = j * tubeStep;
                glm::vec3 offset = {
                    tubeRadius * std::cos(tubeAngle) * std::cos(segmentAngle),
                    tubeRadius * std::sin(tubeAngle),
                    tubeRadius * std::cos(tubeAngle) * std::sin(segmentAngle)
                };

                Vertex vert;
                vert.pos = segmentCenter + offset;
                vert.normal = glm::normalize(offset);
                // vert.color = glm::vec4(1.f);
                // vert.texCoord = { static_cast<float>(i) / segments, static_cast<float>(j) / tubeSegments };
                // vert.tangent = glm::normalize(glm::vec3(-std::sin(segmentAngle), 0.0f, std::cos(segmentAngle)));
                mesh.vertices.push_back(vert);
            }
        }

        // Generate indices
        for (int i = 0; i < segments; ++i) {
            for (int j = 0; j < tubeSegments; ++j) {
                int current = i * (tubeSegments + 1) + j;
                int next = (i + 1) * (tubeSegments + 1) + j;

                mesh.indices.push_back(current);
                mesh.indices.push_back(next);
                mesh.indices.push_back(current + 1);

                mesh.indices.push_back(current + 1);
                mesh.indices.push_back(next);
                mesh.indices.push_back(next + 1);
            }
        }

        return mesh;
    }

    HostMesh createConeMesh(float baseRadius, float height, int segments, bool capped) {
        HostMesh mesh;

        float angleStep = 2.0f * glm::pi<float>() / static_cast<float>(segments);

        // Apex vertex - normal points upward
        Vertex apex;
        apex.pos = { 0.0f, height, 0.0f };
        apex.normal = { 0.0f, 1.0f, 0.0f };
        mesh.vertices.push_back(apex);

        // Base circle vertices with smooth normals
        // The normal at a base point should point outward and upward at the cone surface angle
        for (int i = 0; i <= segments; ++i) {
            float angle = i * angleStep;
            float x = baseRadius * std::cos(angle);
            float z = baseRadius * std::sin(angle);

            Vertex baseVertex;
            baseVertex.pos = { x, 0.0f, z };
            // Smooth normal for cone: perpendicular to the cone surface
            // Points outward (x, z direction) and upward
            baseVertex.normal = glm::normalize(glm::vec3(x, baseRadius / height, z));
            mesh.vertices.push_back(baseVertex);
        }

        // Side triangles
        for (int i = 1; i <= segments; ++i) {
            mesh.indices.push_back(0); // Apex
            mesh.indices.push_back(i);
            mesh.indices.push_back(i + 1);
        }

        // Base cap (if needed)
        if (capped) {
            uint32_t centerIndex = mesh.vertices.size();
            mesh.vertices.push_back({{0.0f, 0.0f, 0.0f}, 0.0f, {0.0f, -1.0f, 0.0f}, 0.0f});

            for (int i = 0; i < segments; ++i) {
                float angle1 = i * angleStep;
                float angle2 = (i + 1) * angleStep;

                glm::vec3 base1 = { baseRadius * std::cos(angle1), 0.0f, baseRadius * std::sin(angle1) };
                glm::vec3 base2 = { baseRadius * std::cos(angle2), 0.0f, baseRadius * std::sin(angle2) };

                uint32_t baseIdx = mesh.vertices.size();
                mesh.vertices.push_back({base1, 0.0f, {0.0f, -1.0f, 0.0f}, 0.0f});
                mesh.vertices.push_back({base2, 0.0f, {0.0f, -1.0f, 0.0f}, 0.0f});

                mesh.indices.push_back(centerIndex);
                mesh.indices.push_back(baseIdx + 1);
                mesh.indices.push_back(baseIdx + 0);
            }
        }

        return mesh;
    }

    HostMesh createCylinderMesh(float radius, float height, int segments, bool capped) {
        HostMesh mesh;

        float halfHeight = height * 0.5f;
        float angleStep = 2.0f * glm::pi<float>() / float(segments);

        // Side vertices
        for (int i = 0; i <= segments; ++i)
        {
            float angle = float(i) * angleStep;
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);

            glm::vec3 n = glm::normalize(glm::vec3(cosA, 0, sinA));

            // top vertex
            mesh.vertices.push_back({
                glm::vec3(cosA * radius,  halfHeight, sinA * radius),
                0.0f,
                n,
                0.0f
            });

            // bottom vertex
            mesh.vertices.push_back({
                glm::vec3(cosA * radius, -halfHeight, sinA * radius),
                0.0f,
                n,
                0.0f
            });
        }

        // Side indices
        for (int i = 0; i < segments; ++i)
        {
            int top1    = 2*i;
            int bottom1 = top1 + 1;

            int top2    = 2*(i+1);
            int bottom2 = top2 + 1;

            // triangle 1
            mesh.indices.push_back(top1);
            mesh.indices.push_back(bottom1);
            mesh.indices.push_back(top2);

            // triangle 2
            mesh.indices.push_back(bottom1);
            mesh.indices.push_back(bottom2);
            mesh.indices.push_back(top2);
        }

        if (capped)
        {
            // Top cap
            uint32_t topCenter = mesh.vertices.size();
            mesh.vertices.push_back({ {0, halfHeight, 0}, 0.0f, {0,1,0}, 0.0f });

            uint32_t topStart = mesh.vertices.size();

            for (int i = 0; i <= segments; ++i)
            {
                float angle = float(i) * angleStep;
                float x = std::cos(angle) * radius;
                float z = std::sin(angle) * radius;

                mesh.vertices.push_back({ {x, halfHeight, z}, 0.0f, {0,1,0}, 0.0f });
            }

            for (int i = 0; i < segments; ++i)
            {
                mesh.indices.push_back(topCenter);
                mesh.indices.push_back(topStart + i);
                mesh.indices.push_back(topStart + i + 1);
            }

            // Bottom cap
            uint32_t bottomCenter = mesh.vertices.size();
            mesh.vertices.push_back({ {0,-halfHeight,0}, 0.0f, {0,-1,0}, 0.0f });

            uint32_t bottomStart = mesh.vertices.size();

            for (int i = 0; i <= segments; ++i)
            {
                float angle = float(i) * angleStep;
                float x = std::cos(angle) * radius;
                float z = std::sin(angle) * radius;

                mesh.vertices.push_back({ {x,-halfHeight,z}, 0.0f, {0,-1,0}, 0.0f });
            }

            for (int i = 0; i < segments; ++i)
            {
                mesh.indices.push_back(bottomCenter);
                mesh.indices.push_back(bottomStart + i + 1);
                mesh.indices.push_back(bottomStart + i);
            }
        }

        return mesh;
    }

    HostMesh createPrismMesh(float radius, float height, int sides, bool capped) {
        HostMesh mesh;

        float angleStep = 2.f * glm::pi<float>() / float(sides);
        float halfHeight = height * 0.5f;

        // -----------------------------
        // SIDE FACES (flat normals)
        // -----------------------------
        for (int i = 0; i < sides; ++i)
        {
            int next = (i + 1) % sides;

            float a1 = i * angleStep;
            float a2 = next * angleStep;

            glm::vec3 b1 = { radius * cos(a1), -halfHeight, radius * sin(a1) };
            glm::vec3 t1 = { radius * cos(a1),  halfHeight, radius * sin(a1) };

            glm::vec3 b2 = { radius * cos(a2), -halfHeight, radius * sin(a2) };
            glm::vec3 t2 = { radius * cos(a2),  halfHeight, radius * sin(a2) };

            glm::vec3 normal = glm::normalize(glm::cross(t1 - b1, b2 - b1));

            uint32_t i0 = mesh.vertices.size();
            mesh.vertices.push_back({ b1, 0, normal, 0 });
            mesh.vertices.push_back({ t1, 0, normal, 0 });
            mesh.vertices.push_back({ b2, 0, normal, 0 });
            mesh.vertices.push_back({ t2, 0, normal, 0 });

            mesh.indices.insert(mesh.indices.end(),
            {
                i0,     i0 + 1, i0 + 2,
                i0 + 1, i0 + 3, i0 + 2
            });
        }

        // -----------------------------
        // CAPS (optional)
        // -----------------------------
        if (capped)
        {
            // top center
            uint32_t topCenter = mesh.vertices.size();
            mesh.vertices.push_back({ {0, halfHeight, 0}, 0, {0,1,0}, 0 });

            // bottom center
            uint32_t bottomCenter = mesh.vertices.size();
            mesh.vertices.push_back({ {0,-halfHeight, 0}, 0, {0,-1,0}, 0 });

            // top cap ring
            uint32_t topStart = mesh.vertices.size();
            for (int i = 0; i <= sides; ++i)
            {
                float a = i * angleStep;
                mesh.vertices.push_back({
                    { radius*cos(a), halfHeight, radius*sin(a) },
                    0, {0,1,0}, 0
                });
            }

            // top cap triangles
            for (int i = 0; i < sides; ++i)
            {
                mesh.indices.push_back(topCenter);
                mesh.indices.push_back(topStart + i);
                mesh.indices.push_back(topStart + i + 1);
            }

            // bottom cap ring
            uint32_t bottomStart = mesh.vertices.size();
            for (int i = 0; i <= sides; ++i)
            {
                float a = i * angleStep;
                mesh.vertices.push_back({
                    { radius*cos(a), -halfHeight, radius*sin(a) },
                    0, {0,-1,0}, 0
                });
            }

            // bottom cap triangles
            for (int i = 0; i < sides; ++i)
            {
                mesh.indices.push_back(bottomCenter);
                mesh.indices.push_back(bottomStart + i + 1);
                mesh.indices.push_back(bottomStart + i);
            }
        }

        return mesh;
    }

    HostMesh createIcosahedronMesh(float radius) {
        HostMesh mesh;

        const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

        // Base vertex positions
        std::vector<glm::vec3> basePositions = {
            {-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
            { 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
            { t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1}
        };

        // Normalize and scale base positions
        for (auto& pos : basePositions) {
            pos = glm::normalize(pos) * radius;
        }

        // Triangle indices (20 faces)
        std::vector<std::array<uint32_t, 3>> faces = {
            // 5 faces around vertex 0
            {0,11,5}, {0,5,1}, {0,1,7}, {0,7,10}, {0,10,11},
            // 5 adjacent faces
            {1,5,9}, {5,11,4}, {11,10,2}, {10,7,6}, {7,1,8},
            // 5 faces around vertex 3
            {3,9,4}, {3,4,2}, {3,2,6}, {3,6,8}, {3,8,9},
            // 5 adjacent faces
            {4,9,5}, {2,4,11}, {6,2,10}, {8,6,7}, {9,8,1}
        };

        // Create vertices with flat face normals
        for (const auto& face : faces) {
            glm::vec3 v0 = basePositions[face[0]];
            glm::vec3 v1 = basePositions[face[1]];
            glm::vec3 v2 = basePositions[face[2]];

            // Calculate flat face normal
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

            // Add three vertices with the same face normal
            uint32_t baseIdx = mesh.vertices.size();
            mesh.vertices.push_back({v0, 0.0f, normal, 0.0f});
            mesh.vertices.push_back({v1, 0.0f, normal, 0.0f});
            mesh.vertices.push_back({v2, 0.0f, normal, 0.0f});

            // Add indices for this triangle
            mesh.indices.push_back(baseIdx + 0);
            mesh.indices.push_back(baseIdx + 1);
            mesh.indices.push_back(baseIdx + 2);
        }

        return mesh;
    }

    HostMesh createRhombusMesh(float edgeLength, float height)
    {
        HostMesh mesh;

        float halfEdge = edgeLength * 0.5f;
        float halfHeight = height * 0.5f;

        // Top apex
        glm::vec3 topApex = { 0.0f, halfHeight, 0.0f };

        // Bottom apex
        glm::vec3 bottomApex = { 0.0f, -halfHeight, 0.0f };

        // Middle square vertices (in XZ plane at y=0)
        glm::vec3 v0 = { -halfEdge, 0.0f, -halfEdge };
        glm::vec3 v1 = {  halfEdge, 0.0f, -halfEdge };
        glm::vec3 v2 = {  halfEdge, 0.0f,  halfEdge };
        glm::vec3 v3 = { -halfEdge, 0.0f,  halfEdge };

        // Helper to add a triangle face with calculated normal
        auto addTriangle = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            glm::vec3 edge1 = b - a;
            glm::vec3 edge2 = c - a;
            glm::vec3 normal = -glm::normalize(glm::cross(edge1, edge2));

            uint32_t baseIdx = mesh.vertices.size();
            mesh.vertices.push_back({a, 0.0f, normal, 0.0f});
            mesh.vertices.push_back({b, 0.0f, normal, 0.0f});
            mesh.vertices.push_back({c, 0.0f, normal, 0.0f});

            mesh.indices.push_back(baseIdx + 0);
            mesh.indices.push_back(baseIdx + 1);
            mesh.indices.push_back(baseIdx + 2);
        };

        // Top pyramid faces (4 triangles)
        addTriangle(topApex, v0, v1);
        addTriangle(topApex, v1, v2);
        addTriangle(topApex, v2, v3);
        addTriangle(topApex, v3, v0);

        // Bottom pyramid faces (4 triangles)
        addTriangle(bottomApex, v1, v0);
        addTriangle(bottomApex, v2, v1);
        addTriangle(bottomApex, v3, v2);
        addTriangle(bottomApex, v0, v3);

        return mesh;
    }

    // Ring / Annulus Mesh
    HostMesh createAnnulusMesh(float innerRadius, float outerRadius, int segments)
    {
        HostMesh mesh;

        float angleStep = 2.0f * glm::pi<float>() / static_cast<float>(segments);

        for (uint32_t i = 0; i <= segments; ++i) {

            float angle = i * angleStep;
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);

            // Outer vertex
            Vertex outerVertex;
            outerVertex.pos = { cosA * outerRadius, 0.0f, sinA * outerRadius };
            // outerVertex.color = glm::vec4(1.f);
            // outerVertex.texCoord = {  1.0f, static_cast<float>(i) / segments };
            // outerVertex.normal = glm::vec3(0, 1, 0);
            // outerVertex.tangent = glm::vec3(-sinA, 0.0f, cosA); // Tangent is perpendicular to the normal
            mesh.vertices.push_back(outerVertex);

            // Inner vertex
            Vertex innerVertex;
            innerVertex.pos = { cosA * innerRadius, 0.0f, sinA * innerRadius };
            // innerVertex.color = glm::vec4(1.f);
            // innerVertex.texCoord = { 0.0f, static_cast<float>(i) / segments };
            // innerVertex.normal = glm::vec3(0, 1, 0);
            // innerVertex.tangent = glm::vec3(-sinA, 0.0f, cosA); // Tangent is perpendicular to the normal
            mesh.vertices.push_back(innerVertex);

        }

        // Generate indices for both sides of the annulus
        for (uint32_t i = 0; i < segments; ++i) {
            uint32_t outerIndex = i * 2;
            uint32_t innerIndex = outerIndex + 1;
            uint32_t nextOuterIndex = ((i + 1) % segments) * 2;
            uint32_t nextInnerIndex = nextOuterIndex + 1;

            // Triangle 1
            mesh.indices.push_back(outerIndex);
            mesh.indices.push_back(innerIndex);
            mesh.indices.push_back(nextOuterIndex);

            // Triangle 2
            mesh.indices.push_back(innerIndex);
            mesh.indices.push_back(nextInnerIndex);
            mesh.indices.push_back(nextOuterIndex);

            mesh.indices.push_back(outerIndex);
            mesh.indices.push_back(nextOuterIndex);
            mesh.indices.push_back(innerIndex);

            mesh.indices.push_back(innerIndex);
            mesh.indices.push_back(nextOuterIndex);
            mesh.indices.push_back(nextInnerIndex);

        }

        return mesh;
    }

    HostMesh createQuadMesh(float width, float height, const glm::vec3& normal, const glm::vec3& origin, bool twoSided)
    {
        HostMesh mesh;

        // Normalize the input normal
        glm::vec3 n = glm::normalize(normal);

        // Create a coordinate system from the normal
        // Choose an arbitrary vector that's not parallel to the normal
        glm::vec3 up = glm::abs(n.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);

        // Create two perpendicular vectors in the plane of the quad
        glm::vec3 tangent = glm::normalize(glm::cross(up, n));
        glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));

        // Calculate vertex positions in the quad's local space
        glm::vec3 halfWidth = tangent * (width / 2.0f);
        glm::vec3 halfHeight = bitangent * (height / 2.0f);

        Vertex v0;
        v0.pos = origin - halfWidth - halfHeight;
        v0.normal = n;

        Vertex v1;
        v1.pos = origin + halfWidth - halfHeight;
        v1.normal = n;

        Vertex v2;
        v2.pos = origin + halfWidth + halfHeight;
        v2.normal = n;

        Vertex v3;
        v3.pos = origin - halfWidth + halfHeight;
        v3.normal = n;

        mesh.vertices.push_back(v0);
        mesh.vertices.push_back(v1);
        mesh.vertices.push_back(v2);
        mesh.vertices.push_back(v3);

        mesh.indices.push_back(0);
        mesh.indices.push_back(1);
        mesh.indices.push_back(2);

        mesh.indices.push_back(0);
        mesh.indices.push_back(2);
        mesh.indices.push_back(3);

        if(twoSided) {
            mesh.indices.push_back(0);
            mesh.indices.push_back(2);
            mesh.indices.push_back(1);

            mesh.indices.push_back(0);
            mesh.indices.push_back(3);
            mesh.indices.push_back(2);
        }

        return mesh;
    }

    HostMesh createCubeMesh(float width, float height, float depth)
    {
        HostMesh mesh;

        // Define the 8 vertices of the cube
        std::array<glm::vec3, 8> vertices = {
            glm::vec3(-width / 2, -height / 2, -depth / 2),
            glm::vec3(width / 2, -height / 2, -depth / 2),
            glm::vec3(width / 2, height / 2, -depth / 2),
            glm::vec3(-width / 2, height / 2, -depth / 2),
            glm::vec3(-width / 2, -height / 2, depth / 2),
            glm::vec3(width / 2, -height / 2, depth / 2),
            glm::vec3(width / 2, height / 2, depth / 2),
            glm::vec3(-width / 2, height / 2, depth / 2)
        };

        // Define the indices for the cube's faces
        std::array<uint32_t, 36> indices = {
            // Front face
            0, 2, 1,
            0, 3, 2,
            // Back face
            4, 5, 6,
            4, 6, 7,
            // Left face
            0, 4, 7,
            0, 7, 3,
            // Right face
            1, 6, 5,
            1, 2, 6,
            // Top face
            3, 6, 2,
            3, 7, 6,
            // Bottom face
            0, 1, 5,
            0, 5, 4
        };

        for (const auto& vertex : vertices) {
            Vertex v;
            v.pos = vertex;
            // v.color = {1.0f,1.0f,1.0f,1.0f}; // Default color (white)
            // v.texCoord = {0.0f, 0.0f};       // Default texture coordinates
            // v.normal = {0.0f, 0.0f, 0.0f};   // Default normal (to be calculated later)
            // v.tangent = {0.0f, 0.0f, 0.0f};  // Default tangent (to be calculated later)
            mesh.vertices.push_back(v);
        }

        for (const auto& index : indices) {
            mesh.indices.push_back(index);
        }

        return mesh;
    }

}