cmake_minimum_required(VERSION 3.24)
project(Mikrosim VERSION 0.1.0.0 LANGUAGES CXX)

option(VULKAN_VALIDATION "vulkan validation layers" ON)
option(VULKAN_VALIDATION_SUPRESS_LOADER_INFO "suppress vulkan validation loader info messages" ON)
option(VULKAN_VALIDATION_ABORT "abort on vulkan validation error" OFF)
option(COMPILE_SHADERS "compile shaders to spirv" ON)

set(FRAMES_IN_FLIGHT 2)
set(PARTICLE_COUNT 40960)
set(CELL_PARTICLE_COUNT 1024)
set(MEMBRANE_PARTICLE_COUNT 4096)
set(STRUCT_PARTICLE_COUNT 8192)
set(CELLS_X 120)
set(CELLS_Y 120)
set(WINDOW_CALLBACK_GENERATOR "on_##name")

if(VULKAN_VALIDATION)
	set(VULKAN_VALIDATION 1)
else()
	set(VULKAN_VALIDATION 0)
endif()
if(VULKAN_VALIDATION_SUPRESS_LOADER_INFO)
	set(VULKAN_VALIDATION_SUPRESS_LOADER_INFO 1)
else()
	set(VULKAN_VALIDATION_SUPRESS_LOADER_INFO 0)
endif()
if(VULKAN_VALIDATION_ABORT)
	set(VULKAN_VALIDATION_ABORT 1)
else()
	set(VULKAN_VALIDATION_ABORT 0)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/)

find_package(glm REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(VulkanMemoryAllocator REQUIRED)
if (COMPILE_SHADERS)
	find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)
endif()
if(POLICY CMP0167)
	cmake_policy(SET CMP0167 NEW)
endif()
find_package(Boost REQUIRED COMPONENTS math_tr1f)

add_compile_definitions(GLM_FORCE_RADIANS=1 GLM_FORCE_DEPTH_ZERO_TO_ONE=1 GLM_ENABLE_EXPERIMENTAL=1)

set(SHADER_BINARY_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
function(add_shaders SHADER_TARGET)
	add_custom_command(COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR} OUTPUT ${SHADER_BINARY_DIR} COMMENT "Creating ${SHADER_BINARY_DIR}")
	foreach(source IN LISTS ARGN)
		get_filename_component(FILENAME ${source} NAME)
		add_custom_command(
			COMMAND ${glslc_executable} -o ${SHADER_BINARY_DIR}/${FILENAME}.spv ${source}
			OUTPUT ${SHADER_BINARY_DIR}/${FILENAME}.spv
			DEPENDS ${source} ${SHADER_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src/shaders/settings.glsl
			COMMENT "Compiling shader ${source}")
		list(APPEND SPV_SHADERS ${SHADER_BINARY_DIR}/${FILENAME}.spv)
	endforeach()
	add_custom_target(${SHADER_TARGET} ALL DEPENDS ${SPV_SHADERS})
endfunction()

if (COMPILE_SHADERS)
	configure_file(${CMAKE_SOURCE_DIR}/src/shaders/settings.glsl.in ${CMAKE_SOURCE_DIR}/src/shaders/settings.glsl)
	add_shaders(sh ${CMAKE_SOURCE_DIR}/src/shaders/count.comp ${CMAKE_SOURCE_DIR}/src/shaders/upsweep.comp
		${CMAKE_SOURCE_DIR}/src/shaders/downsweep.comp ${CMAKE_SOURCE_DIR}/src/shaders/write.comp
		${CMAKE_SOURCE_DIR}/src/shaders/density.comp ${CMAKE_SOURCE_DIR}/src/shaders/update.comp
		${CMAKE_SOURCE_DIR}/src/shaders/report.comp
		${CMAKE_SOURCE_DIR}/src/shaders/particle.vert ${CMAKE_SOURCE_DIR}/src/shaders/particle.frag)
	configure_file(${CMAKE_SOURCE_DIR}/src/cmake_defs.hpp.in ${CMAKE_SOURCE_DIR}/src/cmake_defs.hpp)
endif()
add_executable(mikrosim
	${CMAKE_SOURCE_DIR}/src/main.cpp
		${CMAKE_SOURCE_DIR}/src/app.cpp ${CMAKE_SOURCE_DIR}/src/particles.cpp
		${CMAKE_SOURCE_DIR}/src/cell.cpp ${CMAKE_SOURCE_DIR}/src/compounds.cpp
		${CMAKE_SOURCE_DIR}/src/protein.cpp ${CMAKE_SOURCE_DIR}/src/views.cpp
		${CMAKE_SOURCE_DIR}/src/parse.cpp
	${CMAKE_SOURCE_DIR}/src/rendering/window.cpp ${CMAKE_SOURCE_DIR}/src/rendering/context.cpp
		${CMAKE_SOURCE_DIR}/src/rendering/device.cpp ${CMAKE_SOURCE_DIR}/src/rendering/pipeline.cpp
		${CMAKE_SOURCE_DIR}/src/rendering/vma_wrap.cpp ${CMAKE_SOURCE_DIR}/src/rendering/2d.cpp
		${CMAKE_SOURCE_DIR}/src/rendering/buffers.cpp ${CMAKE_SOURCE_DIR}/src/rendering/text.cpp
		${CMAKE_SOURCE_DIR}/src/rendering/timestamp.cpp ${CMAKE_SOURCE_DIR}/src/rendering/preset.cpp
	${CMAKE_SOURCE_DIR}/src/format/qoi.cpp
	${CMAKE_SOURCE_DIR}/src/input/input.cpp
	${CMAKE_SOURCE_DIR}/src/util.cpp
	${CMAKE_SOURCE_DIR}/src/imgui_impl.cpp)
target_include_directories(mikrosim PUBLIC ${CMAKE_SOURCE_DIR}/submodules/ ${CMAKE_SOURCE_DIR}/submodules/imgui/ ${Boost_INCLUDE_DIRS})
target_link_libraries(mikrosim PUBLIC glfw3 vulkan glm GPUOpen::VulkanMemoryAllocator ${Boost_LIBRARIES})
target_precompile_headers(mikrosim PUBLIC ${CMAKE_SOURCE_DIR}/src/vulkan_glfw_pch.hpp)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(mikrosim PUBLIC -Wall -Wextra -Wpedantic -Wno-class-memaccess)
	target_compile_options(mikrosim PRIVATE -fdiagnostics-color=always)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(mikrosim PUBLIC -Weverything -Wno-c++98-compat-pedantic -Wno-shadow-field-in-constructor -Wno-padded -Wno-shadow -Wno-unsafe-buffer-usage -Wno-covered-switch-default)
	target_compile_options(mikrosim PRIVATE -fcolor-diagnostics)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		message(STATUS "using undefined sanitizer")
		target_compile_options(mikrosim PRIVATE -fsanitize=undefined)
		target_link_options(mikrosim PRIVATE -fsanitize=undefined)
	endif()
	if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		message(STATUS "using address sanitizer")
		target_compile_options(mikrosim PRIVATE -fsanitize=address)
		target_link_options(mikrosim PRIVATE -fsanitize=address)
	endif()
endif()

message(STATUS "configured ${PROJECT_NAME} ${PROJECT_VERSION}")

