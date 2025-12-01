#pragma once
#include "../stdafx.h"
#include "HostMesh.h"

namespace MeshFactory {

    HostMesh createSphereMesh(float radius, int segments, int rings, bool skySphere = false);
    HostMesh createAnnulusMesh(float innerRadius, float outerRadius, int segments);
    HostMesh createQuadMesh(float width, float height, bool twoSided = false);
    HostMesh createCubeMesh(float width, float height, float depth);
} 
