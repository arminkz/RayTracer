#pragma once

// [SDL]
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// [Vulkan]
#include <vulkan/vulkan.h>

// [SPDLOG]
#include "spdlog/spdlog.h"

// [GLM]
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <glm/gtx/hash.hpp>

// [Standard libraries: basic]
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <execution>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <thread>
#include <filesystem>
#include <streambuf>
#include <optional>

// [Standard libraries: data structures]
#include <array>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// [Time]
using TimePoint = std::chrono::high_resolution_clock::time_point;

// [Global constants]
const int MAX_FRAMES_IN_FLIGHT = 2;