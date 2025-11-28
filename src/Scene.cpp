#include "Scene.h"

Scene::Scene(std::shared_ptr<VulkanContext> ctx,  std::shared_ptr<SwapChain> swapChain)
    : _ctx(std::move(ctx)), _swapChain(std::move(swapChain))
{
}

Scene::~Scene()
{
}

void Scene::update(uint32_t currentImage)
{
    _currentFrame = currentImage;
}