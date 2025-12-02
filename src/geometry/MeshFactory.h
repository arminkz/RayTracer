#pragma once
#include "../stdafx.h"
#include "HostMesh.h"

namespace MeshFactory {

    HostMesh createSphereMesh(float radius, int segments, int rings, bool skySphere = false);
    HostMesh createBoxMesh(float width, float height, float depth);
    HostMesh createPyramidMesh(float baseWidth, float baseDepth, float height);
    HostMesh createDoughnutMesh(float innerRadius, float outerRadius, int segments, int tubeSegments);
    HostMesh createConeMesh(float baseRadius, float height, int segments, bool capped);
    HostMesh createCylinderMesh(float radius, float height, int segments, bool capped);
    HostMesh createPrismMesh(float radius, float height, int sides, bool capped);
    HostMesh createIcosahedronMesh(float radius);
    HostMesh createRhombusMesh(float edgeLength, float height);

    HostMesh createAnnulusMesh(float innerRadius, float outerRadius, int segments);
    HostMesh createQuadMesh(float width, float height, const glm::vec3& normal = glm::vec3(0.0f, 1.0f, 0.0f), const glm::vec3& origin = glm::vec3(0.0f), bool twoSided = false);
    HostMesh createCubeMesh(float width, float height, float depth);
} 
