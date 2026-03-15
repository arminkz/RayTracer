#include "scene/content/TeapotScene.h"
#include "scene/SceneGraph.h"
#include "gui/FontAwesome.h"
#include <imgui.h>

void TeapotScene::onLoad()
{
    populate(*_graph);
}

void TeapotScene::populate(SceneGraph& graph)
{
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

    // Teapot with selected material
    {
        SceneGraph::SceneObject obj;
        obj.geometryType = "teapot";
        obj.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.01f, 0.0f));
        applyMaterial(obj);
        graph.addObject(obj);
    }
}

void TeapotScene::applyMaterial(SceneGraph::SceneObject& obj) const
{
    switch (_materialCombo) {
    case 0: // Metallic (gold)
        obj.color        = glm::vec3(0.672f, 0.490f, 0.203f);
        obj.metallic     = 1.0f;
        obj.roughness    = 0.3f;
        obj.transparency = 0.0f;
        obj.ior          = 1.5f;
        obj.absorbance   = glm::vec3(0.0f);
        break;
    case 1: // Diffuse
        obj.color        = glm::vec3(0.0f, 0.6f, 0.8f);
        obj.metallic     = 0.0f;
        obj.roughness    = 1.0f;
        obj.transparency = 0.0f;
        obj.ior          = 0.0f;
        obj.absorbance   = glm::vec3(0.0f);
        break;
    case 2: // Glass
        obj.color        = glm::vec3(1.0f, 1.0f, 1.0f);
        obj.metallic     = 0.0f;
        obj.roughness    = 0.05f;
        obj.transparency = 1.0f;
        obj.ior          = 1.1f;
        obj.absorbance   = glm::vec3(0.0f);
        break;
    case 3: // Marble (Beer-Lambert)
        obj.color        = glm::vec3(0.05f, 0.01f, 0.7f);
        obj.metallic     = 0.0f;
        obj.roughness    = 0.8f;
        obj.transparency = 0.8f;
        obj.ior          = 1.02f;
        obj.absorbance   = glm::vec3(8.f, 8.f, 0.1f); // warm amber absorption
        break;
    }
}

void TeapotScene::buildUI()
{
    ImGui::Separator();
    ImGui::Text(ICON_FA_PAINT_BRUSH " Material");
    ImGui::Indent(16.0f);
        const char* materials[] = { "Metallic", "Diffuse", "Glass", "Marble" };
        if (ImGui::Combo("##teapot_material", &_materialCombo, materials, 4)) {
            _graph->clearObjects();
            populate(*_graph);
        }
    ImGui::Unindent(16.0f);
}
