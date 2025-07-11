cmake_minimum_required(VERSION 3.0...3.25)

project(
	LearnWebGPU
	VERSION 0.1.0
	LANGUAGES CXX C
)

if (NOT EMSCRIPTEN)
	add_subdirectory(glfw)
else()
	add_library(glfw INTERFACE)
	target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)
add_subdirectory(imgui)


add_executable(App
	main.cpp
	# We add the ResourceManager files
	ResourceManager.h
    ResourceManager.cpp
	
	Splat.h

	Node.h
	Node.cpp

	Camera.h

	OrbitCamera.h

	SplatMesh.h
	SplatMeshOctree.hpp
	SplatMeshHC.hpp
	SplatMeshGridHC.hpp
	SplatMeshGridHC.cpp

	Renderer.hpp
	Renderer.cpp

	GUI.hpp
	GUI.cpp

	BB.hpp

	Octree.hpp
	Octree.cpp

	HC.hpp
	HC.cpp

	GridHC.hpp
	GridHC.cpp
)

target_include_directories(App PRIVATE .)

target_link_libraries(App PRIVATE glfw webgpu glfw3webgpu imgui)

# We add an option to enable different settings when developing the app than
# when distributing it.
option(DEV_MODE "Set up development helper settings" ON)

if(DEV_MODE)
    # In dev mode, we load resources from the source tree, so that when we
    # dynamically edit resources (like shaders), these are correctly
    # versionned.
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
    )
else()
    # In release mode, we just load resources relatively to wherever the
    # executable is launched from, so that the binary is portable
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="./resources"
    )
endif()

target_copy_webgpu_binaries(App)

set_target_properties(App PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	COMPILE_WARNING_AS_ERROR OFF
)

if (MSVC)
	target_compile_options(App PRIVATE /W4)
else()
	target_compile_options(App PRIVATE -Wall -Wextra -pedantic -O3)
endif()

if (XCODE)
	set_target_properties(App PROPERTIES
		XCODE_GENERATE_SCHEME ON
		XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
	)
endif()

if (EMSCRIPTEN)
	set_target_properties(App PROPERTIES SUFFIX ".html")
	target_link_options(App PRIVATE
		-sASYNCIFY
		--preload-file "${CMAKE_CURRENT_SOURCE_DIR}/resources"
	)
endif()



option(PARALLEL "Enable parallel execution with TBB" OFF)
option(TBB_PATH "Path to TBB installation" "")
# If TBB_PATH is not set, provide a default value
if(NOT TBB_PATH)
    message(FATAL_ERROR "Please specify the path to TBB using -DTBB_PATH=/path/to/tbb")
endif()

if(PARALLEL)
	message(STATUS "Parallel execution enabled. Finding TBB...")
	set(TBB_DIR TBB_PATH)
    find_package(TBB REQUIRED)
    target_link_libraries(App PRIVATE TBB::tbb)
    target_compile_definitions(App PRIVATE -DPARALLEL)
else()
    message(STATUS "Parallel execution disabled.")
endif()
