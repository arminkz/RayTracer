#include "scene/content/GlassAndSphereScene.h"
#include "scene/SceneGraph.h"

void GlassAndSphereScene::onLoad()
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

    // Metallic sphere at center (unit sphere mesh = radius 0.5)
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
        obj.color        = glm::vec3(0.9f, 0.9f, 0.9f);
        obj.metallic     = 1.0f;
        obj.roughness    = 0.15f;
        obj.transparency = 0.0f;
        graph.addObject(obj);
    }

    // Narrow glass sheet in front of the sphere — 2.0 wide, 1.5 tall, 0.05 thick
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "box";
        glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.75f, 1.2f));
        t = glm::scale(t, glm::vec3(2.0f, 1.5f, 0.05f));
        obj.transform    = t;
        obj.color        = glm::vec3(1.0f, 1.0f, 1.0f);
        obj.metallic     = 0.0f;
        obj.roughness    = 0.05f;
        obj.transparency = 1.0f;
        obj.ior          = 1.5f;
        obj.absorbance   = glm::vec3(0.0f);
        graph.addObject(obj);
    }
}

void GlassAndSphereScene::buildUI()
{
    // No scene-specific controls
}
