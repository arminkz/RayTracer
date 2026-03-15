#include "scene/SceneGraph.h"
#include "vulkan/VulkanHelper.h"
#include "core/AssetPath.h"
#include "geometry/HostMesh.h"
#include "geometry/MeshFactory.h"
#include "geometry/ObjLoader.h"


SceneGraph::SceneGraph(std::shared_ptr<VulkanContext> ctx)
    : _ctx(std::move(ctx))
{
    createGeometryTemplates();
}

void SceneGraph::createGeometryTemplates() {
    // Identity transform for geometry templates (actual transforms are in TLAS instances)
    VkTransformMatrixKHR identityTransform = VulkanHelper::convertToVkTransform(glm::mat4(1.0f));

    // Plane (or large quad)
    HostMesh planeMesh = MeshFactory::createQuadMesh(1000.0f, 1000.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), true);
    _geometryTemplates["plane"].dmesh = std::make_unique<DeviceMesh>(_ctx, planeMesh, identityTransform);
    _geometryTemplates["plane"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["plane"].dmesh);

    // Sphere
    HostMesh sphereMesh = MeshFactory::createSphereMesh(0.5f, 64, 32);
    _geometryTemplates["sphere"].dmesh = std::make_unique<DeviceMesh>(_ctx, sphereMesh, identityTransform);
    _geometryTemplates["sphere"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["sphere"].dmesh);

    // Box
    HostMesh boxMesh = MeshFactory::createBoxMesh(1.0f, 1.0f, 1.0f);
    _geometryTemplates["box"].dmesh = std::make_unique<DeviceMesh>(_ctx, boxMesh, identityTransform);
    _geometryTemplates["box"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["box"].dmesh);

    // Pyramid
    HostMesh pyramidMesh = MeshFactory::createPyramidMesh(1.0f, 1.0f, 1.0f);
    _geometryTemplates["pyramid"].dmesh = std::make_unique<DeviceMesh>(_ctx, pyramidMesh, identityTransform);
    _geometryTemplates["pyramid"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["pyramid"].dmesh);

    // Doughnut
    HostMesh doughnutMesh = MeshFactory::createDoughnutMesh(0.35f, 0.5f, 64, 32);
    _geometryTemplates["doughnut"].dmesh = std::make_unique<DeviceMesh>(_ctx, doughnutMesh, identityTransform);
    _geometryTemplates["doughnut"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["doughnut"].dmesh);

    // Cone
    HostMesh coneMesh = MeshFactory::createConeMesh(0.5f, 1.0f, 32, true);
    _geometryTemplates["cone"].dmesh = std::make_unique<DeviceMesh>(_ctx, coneMesh, identityTransform);
    _geometryTemplates["cone"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["cone"].dmesh);

    // Cylinder
    HostMesh cylinderMesh = MeshFactory::createCylinderMesh(0.5f, 1.f, 32, true);
    _geometryTemplates["cylinder"].dmesh = std::make_unique<DeviceMesh>(_ctx, cylinderMesh, identityTransform);
    _geometryTemplates["cylinder"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["cylinder"].dmesh);

    // Extruded Hexagon
    HostMesh extrudedHexagonMesh = MeshFactory::createPrismMesh(0.7f, 0.2f, 6, true);
    _geometryTemplates["extruded_hexagon"].dmesh = std::make_unique<DeviceMesh>(_ctx, extrudedHexagonMesh, identityTransform);
    _geometryTemplates["extruded_hexagon"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["extruded_hexagon"].dmesh);

    // Icosahedron
    HostMesh icosahedronMesh = MeshFactory::createIcosahedronMesh(0.5f);
    _geometryTemplates["icosahedron"].dmesh = std::make_unique<DeviceMesh>(_ctx, icosahedronMesh, identityTransform);
    _geometryTemplates["icosahedron"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["icosahedron"].dmesh);

    // Rhombus
    HostMesh rhombusMesh = MeshFactory::createRhombusMesh(0.7f, 1.0f);
    _geometryTemplates["rhombus"].dmesh = std::make_unique<DeviceMesh>(_ctx, rhombusMesh, identityTransform);
    _geometryTemplates["rhombus"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["rhombus"].dmesh);

    // Teapot
    HostMesh teapotMesh = ObjLoader::load(AssetPath::getInstance()->get("mesh/teapot.obj"));
    _geometryTemplates["teapot"].dmesh = std::make_unique<DeviceMesh>(_ctx, teapotMesh, identityTransform);
    _geometryTemplates["teapot"].blas = std::make_unique<BLAS>(_ctx, *_geometryTemplates["teapot"].dmesh);

    // Put more geometry templates here as needed
}

std::vector<VkAccelerationStructureInstanceKHR> SceneGraph::buildInstanceList() const {
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(_sceneObjects.size());

    for (size_t i = 0; i < _sceneObjects.size(); i++) {
        const auto& obj = _sceneObjects[i];
        const auto& geom = _geometryTemplates.at(obj.geometryType);

        VkAccelerationStructureInstanceKHR instance{};
        instance.transform = VulkanHelper::convertToVkTransform(obj.transform);
        instance.instanceCustomIndex = static_cast<uint32_t>(i);
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = geom.blas->getDeviceAddress();

        instances.push_back(instance);
    }

    return instances;
}

std::vector<InstanceData> SceneGraph::buildInstanceDataArray() const {
    std::vector<InstanceData> result;
    result.reserve(_sceneObjects.size());

    for (size_t i = 0; i < _sceneObjects.size(); i++) {
        const auto& obj = _sceneObjects[i];
        const auto& geom = _geometryTemplates.at(obj.geometryType);

        InstanceData instanceData{};
        instanceData.vertexBufferAddress = geom.dmesh->getVertexBufferDeviceAddress().deviceAddress;
        instanceData.indexBufferAddress = geom.dmesh->getIndexBufferDeviceAddress().deviceAddress;
        instanceData.materialType = obj.materialType;
        instanceData.color = obj.color;
        instanceData.metallic = obj.metallic;
        instanceData.roughness = obj.roughness;
        instanceData.transparency = obj.transparency;
        instanceData.ior = obj.ior;
        instanceData.absorbance = obj.absorbance;

        result.push_back(instanceData);
    }

    return result;
}
