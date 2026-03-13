#include "SceneGraph.h"
#include "VulkanHelper.h"
#include "AssetPath.h"
#include "geometry/HostMesh.h"
#include "geometry/MeshFactory.h"
#include "loader/ObjLoader.h"


SceneGraph::SceneGraph(std::shared_ptr<VulkanContext> ctx)
    : _ctx(std::move(ctx))
{
    createGeometryTemplates();
    createSceneObjects();
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

void SceneGraph::createSceneObjects() {
    // List of 10 colors for the spheres
    std::vector<glm::vec3> colors = {
        {1.0f, 0.0f, 0.0f}, // Red
        {0.0f, 1.0f, 0.0f}, // Green
        {0.0f, 0.0f, 1.0f}, // Blue
        {1.0f, 1.0f, 0.0f}, // Yellow
        {1.0f, 0.0f, 1.0f}, // Magenta
        {0.0f, 1.0f, 1.0f}, // Cyan
        {1.0f, 0.5f, 0.0f}, // Orange
        {0.5f, 0.0f, 1.0f}, // Purple
        {0.5f, 0.5f, 0.5f}, // Gray
        {1.0f, 1.0f, 1.0f}  // White
    };

    // Add a large plane to the scene
    {
        SceneObject obj;
        obj.geometryType = "plane";
        obj.transform = glm::mat4(1.0f); // Identity transform
        obj.materialType = 999;          // Checkerboard material
        obj.color = glm::vec3(0.8f, 0.8f, 0.8f); // Light gray
        obj.metallic = 0.0;
        obj.roughness = 0.8;
        obj.transparency = 0.0;
        _sceneObjects.push_back(obj);
    }

    // Add a light source visualization sphere
    // {
    //     _lightSphereIndex = _sceneObjects.size(); // Store the index
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 20.0f, 0.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(0.5f)); // Small sphere
    //     obj.materialType = 1; // Emissive material
    //     obj.color = glm::vec3(1.0f, 1.0f, 0.8f); // Warm white light color
    //     _sceneObjects.push_back(obj);
    // }

    //Objects placed in a grid fashion
    // Create a box
    // {
    //     SceneObject obj;
    //     obj.geometryType = "box";
    //     obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 0.5f, 3.0f));
    //     obj.color = glm::vec3(0.7f, 0.3f, 0.2f); // Brownish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     _sceneObjects.push_back(obj);
    // }

    // Create a reflective metallic sphere (center)
    // {
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 2.5f, 0.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(5.0f)); // Scale up
    //     obj.color = glm::vec3(1.0f, 0.766f, 0.336f); // Gold
    //     obj.metallic = 1.0;
    //     obj.roughness = 0.2;
    //     obj.transparency = 0.0;  // Opaque
    //     _sceneObjects.push_back(obj);
    // }


    //


    // Plastic Cylinder
    {
        SceneObject obj;
        obj.geometryType = "teapot";
        obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.05f, 0.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(1.0f)); // Scale up
        obj.color = glm::vec3(0.672f, 0.490f, 0.203f); // Yellowish
        obj.metallic = 1.0;
        obj.roughness = 0.3;
        obj.transparency = 0.0;
        //obj.ior = 1.05f;
        //obj.absorbance = glm::vec3(8.f, 8.0f, 2.f);
        _sceneObjects.push_back(obj);
    }

    // Create a glass sphere (transparent with refraction)
    // {
    //     SceneObject obj;
    //     obj.geometryType = "box";
    //     obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 2.05f, 0.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(.2f,4.0f,7.0f)); // Scale up
    //     obj.color = glm::vec3(0.95f, 0.98f, 1.0f); // Slight blue tint for glass
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.05;
    //     obj.transparency = 1.00;  // Nearly fully transparent
    //     obj.ior = 1.52f;  // Glass index of refraction
    //     obj.absorbance = glm::vec3(0.1f,0.1f,0.1f);
    //     _sceneObjects.push_back(obj);
    // }


    // {
    //     SceneObject obj;
    //     obj.geometryType = "doughnut";
    //     obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 1.01f, 3.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(2.f)); // Scale up
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    //     obj.color = glm::vec3(0.05f, 0.7f, 0.01f); // Slight blue tint for glass
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     obj.transparency = 0.8;  // Nearly fully transparent
    //     obj.absorbance = glm::vec3(4.f, 0.1f , 4.f);
    //     obj.ior = 1.02;
    //     _sceneObjects.push_back(obj);
    // }

    // // Create more spheres in random positions without overlapping
    // std::vector<glm::vec3> spherePositions; // Track placed sphere positions
    // const float sphereRadius = 0.2f; // Base sphere radius
    // const float minDistance = sphereRadius * 2.0f + 0.5f; // Minimum distance between centers (with spacing)
    // const int numSpheres = 30;
    // const int maxAttempts = 100; // Max attempts per sphere to find valid position

    // for(int i = 0; i < numSpheres; i++) {
    //     bool validPosition = false;
    //     glm::vec3 position;
    //     int attempts = 0;

    //     while(!validPosition && attempts < maxAttempts) {
    //         // Generate random position in a circular area
    //         float angle = static_cast<float>(rand()) / RAND_MAX * 360.0f;
    //         float radius = 3.0f + static_cast<float>(rand()) / RAND_MAX * 12.0f; // Random radius 3-15
    //         float x = radius * cos(glm::radians(angle));
    //         float z = radius * sin(glm::radians(angle));
    //         position = glm::vec3(x, 0.5f, z);

    //         // Check if this position overlaps with any existing sphere
    //         validPosition = true;
    //         for(const auto& existingPos : spherePositions) {
    //             float dist = glm::distance(glm::vec2(position.x, position.z),
    //                                       glm::vec2(existingPos.x, existingPos.z));
    //             if(dist < minDistance) {
    //                 validPosition = false;
    //                 break;
    //             }
    //         }
    //         attempts++;
    //     }

    //     // Only add sphere if we found a valid position
    //     if(validPosition) {
    //         spherePositions.push_back(position);

    //         SceneObject obj;
    //         obj.geometryType = i % 3 == 0 ? "sphere" : "icosahedron";
    //         obj.transform = glm::translate(glm::mat4(1.0f), position);
    //         obj.transform = glm::scale(obj.transform, glm::vec3(1.0f));
    //         obj.color = colors[i % colors.size()];
    //         obj.metallic = i % 2 == 0 ? 0.0 : 1.0;
    //         obj.roughness = 0.2;
    //         obj.transparency = i % 4 == 0 ? 1.0 : 0.0;
    //         _sceneObjects.push_back(obj);
    //     }
    // }

    // Create a Pyramid
    // {
    //     SceneObject obj;
    //     obj.geometryType = "pyramid";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(-3.0f, 0.0f, -3.0f));
    //     obj.color = glm::vec3(0.3f, 0.7f, 0.2f); // Greenish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     _sceneObjects.push_back(obj);
    // }

    // Semi-Transparent (jello like)
    // {
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::scale(glm::mat4(1.f), glm::vec3(1.5f)); // Scale up
    //     obj.transform = glm::translate(obj.transform, glm::vec3(0.0f, 0.502f, 0.0f));
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //     obj.color = glm::vec3(1.0f, 0.766f, 0.336f); // Yellowish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.02;
    //     obj.transparency = 0.99; // we still want shadows
    //     obj.ior = 1.00029;
    //     obj.absorbance = glm::vec3(8.f, 8.0f, 2.f);
    //     _sceneObjects.push_back(obj);
    // }

    // Silver
    // {
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::scale(glm::mat4(1.f), glm::vec3(1.5f)); // Scale up
    //     obj.transform = glm::translate(obj.transform, glm::vec3(0.0f, 0.502f, 0.0f));
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //     obj.color = glm::vec3(0.97, 0.96, 0.91);
    //     obj.metallic = 1.0;
    //     obj.roughness = 0.1;
    //     obj.transparency = 0.0;
    //     _sceneObjects.push_back(obj);
    // }

    // Glass
    // {
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::scale(glm::mat4(1.f), glm::vec3(1.5f)); // Scale up
    //     obj.transform = glm::translate(obj.transform, glm::vec3(0.0f, 0.502f, 0.0f));
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //     obj.color = glm::vec3(0.97, 0.96, 0.91);
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.1;
    //     obj.transparency = 1.0;
    //     obj.ior = 1.03;
    //     _sceneObjects.push_back(obj);
    // }

    // Plastic
    // {
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::scale(glm::mat4(1.f), glm::vec3(1.5f)); // Scale up
    //     obj.transform = glm::translate(obj.transform, glm::vec3(0.0f, 0.502f, 0.0f));
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //     obj.color = glm::vec3(1.0, 0.8, 0.0);
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.9;
    //     obj.transparency = 0.99;
    //     obj.absorbance = glm::vec3(0.1,0.5,8.0);
    //     obj.ior = 1.001;
    //     _sceneObjects.push_back(obj);
    // }

    // Create a Cone
    // {
    //     SceneObject obj;
    //     obj.geometryType = "cone";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(3.0f, 0.f, 0.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(1.2f)); // Scale up
    //     obj.color = glm::vec3(0.7f, 0.2f, 0.7f); // Purple-ish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     _sceneObjects.push_back(obj);
    // }

    // Create a Cylinder
    // {
    //     SceneObject obj;
    //     obj.geometryType = "cylinder";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(3.0f, 0.5f, -3.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(1.0f, 1.0f, 1.0f)); // Scale up
    //     obj.color = glm::vec3(0.2f, 0.7f, 0.7f); // Teal-ish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     _sceneObjects.push_back(obj);
    // }

    // Create a extruded hexagon
    // {
    //     SceneObject obj;
    //     obj.geometryType = "extruded_hexagon";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.6f, 3.0f));
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(1.f, 1.0f, 1.0f)); // Scale up
    //     obj.color = glm::vec3(0.9f, 0.6f, 0.2f); // Orange-ish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     obj.transparency = 1.0;
    //     _sceneObjects.push_back(obj);
    // }

    // Create an icosahedron
    // {
    //     SceneObject obj;
    //     obj.geometryType = "icosahedron";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.5f, -3.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(1.2f)); // Scale up
    //     obj.color = glm::vec3(0.4f, 0.4f, 0.9f); // Light blue-ish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     obj.transparency = 1.0;
    //     _sceneObjects.push_back(obj);
    // }

    // // Create a rhombus
    // {
    //     SceneObject obj;
    //     obj.geometryType = "rhombus";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(3.0f, 0.75f, 3.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(1.0f, 1.5f, 1.0f)); // Scale up
    //     obj.color = glm::vec3(0.6f, 0.2f, 0.2f); // Reddish
    //     obj.roughness = 0.8;
    //     obj.transparency = 1.0;
    //     _sceneObjects.push_back(obj);
    // }

    //

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
        instanceData.roughness = 0.5f;
        instanceData.transparency = obj.transparency;
        instanceData.ior = obj.ior;
        instanceData.absorbance = obj.absorbance;

        result.push_back(instanceData);
    }

    return result;
}
