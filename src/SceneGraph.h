#pragma once
#include "stdafx.h"
#include "VulkanContext.h"
#include "structure/BLAS.h"
#include "geometry/DeviceMesh.h"
#include "InstanceData.h"


class SceneGraph {
public:
    struct GeometryTemplate {
        std::unique_ptr<DeviceMesh> dmesh;
        std::unique_ptr<BLAS> blas;
    };

    struct SceneObject {
        std::string geometryType;
        glm::mat4 transform;
        int materialType = 0;        // 0 = normal, 1 = emissive, 999 = checkerboard
        glm::vec3 color;
        float metallic = 0.0f;
        float roughness = 0.0f;
        float transparency = 0.0f;   // 0 = opaque, 1 = fully transparent
        float ior = 1.5f;            // Index of refraction (1.5 for glass)
        glm::vec3 absorbance;
    };

    SceneGraph(std::shared_ptr<VulkanContext> ctx);

    // Scene object management — called by SceneContent subclasses
    const std::vector<SceneObject>& getObjects() const { return _sceneObjects; }
    void addObject(const SceneObject& obj) { _sceneObjects.push_back(obj); _instanceDataDirty = true; }
    void clearObjects() { _sceneObjects.clear(); _instanceDataDirty = true; }

    // True when objects changed and the GPU instance data buffer must be re-uploaded
    bool needsInstanceRebuild() const { return _instanceDataDirty; }
    void clearInstanceRebuildFlag() { _instanceDataDirty = false; }
    std::vector<VkAccelerationStructureInstanceKHR> buildInstanceList() const;
    std::vector<InstanceData> buildInstanceDataArray() const;

private:
    bool _instanceDataDirty = false;
    std::shared_ptr<VulkanContext> _ctx;

    std::unordered_map<std::string, GeometryTemplate> _geometryTemplates;
    std::vector<SceneObject> _sceneObjects;

    void createGeometryTemplates();
};
