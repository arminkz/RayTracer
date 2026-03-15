#include "scene/content/SpheresScene.h"
#include "scene/SceneGraph.h"

void SpheresScene::onLoad()
{
    SceneGraph& graph = *_graph;
    // Ground plane with checkerboard material
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "plane";
        obj.transform    = glm::mat4(1.0f);
        obj.materialType = 999; // checkerboard
        obj.color        = glm::vec3(0.8f, 0.8f, 0.8f);
        obj.metallic     = 0.0f;
        obj.roughness    = 0.8f;
        obj.transparency = 0.0f;
        graph.addObject(obj);
    }

    // Matte red
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(-4.0f, 0.5f, -2.0f));
        obj.color        = glm::vec3(0.8f, 0.1f, 0.1f);
        obj.metallic     = 0.0f;
        obj.roughness    = 1.0f;
        obj.transparency = 0.0f;
        graph.addObject(obj);
    }

    // Matte white
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.5f, -2.0f));
        obj.color        = glm::vec3(0.9f, 0.9f, 0.9f);
        obj.metallic     = 0.0f;
        obj.roughness    = 0.9f;
        obj.transparency = 0.0f;
        graph.addObject(obj);
    }

    // Gold metal
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, -2.0f));
        obj.color        = glm::vec3(1.0f, 0.85f, 0.1f);
        obj.metallic     = 1.0f;
        obj.roughness    = 0.1f;
        obj.transparency = 0.0f;
        graph.addObject(obj);
    }

    // Rough metal
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.5f, -2.0f));
        obj.color        = glm::vec3(0.7f, 0.7f, 0.7f);
        obj.metallic     = 1.0f;
        obj.roughness    = 0.5f;
        obj.transparency = 0.0f;
        graph.addObject(obj);
    }

    // Glass sphere
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, 0.5f, -2.0f));
        obj.color        = glm::vec3(1.0f, 1.0f, 1.0f);
        obj.metallic     = 0.0f;
        obj.roughness    = 0.05f;
        obj.transparency = 1.0f;
        obj.ior          = 1.5f;
        graph.addObject(obj);
    }

    // Emissive light sphere
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.5f, 2.0f));
        obj.materialType = 1; // emissive
        obj.color        = glm::vec3(1.0f, 0.8f, 0.4f);
        obj.metallic     = 0.0f;
        obj.roughness    = 1.0f;
        obj.transparency = 0.0f;
        graph.addObject(obj);
    }

    // Tinted glass
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.5f, 2.0f));
        obj.color        = glm::vec3(0.2f, 0.8f, 0.4f);
        obj.metallic     = 0.0f;
        obj.roughness    = 0.05f;
        obj.transparency = 0.9f;
        obj.ior          = 1.45f;
        obj.absorbance   = glm::vec3(0.5f, 0.1f, 0.8f);
        graph.addObject(obj);
    }
}

void SpheresScene::buildUI()
{
    // No scene-specific controls yet
}
