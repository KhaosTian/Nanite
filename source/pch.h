#pragma once

// std
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <bit>
#include <type_traits>
#include <vector>
#include <map>
#include <unordered_map>
#include <source_location>

// glm
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>