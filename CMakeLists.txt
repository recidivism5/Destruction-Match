cmake_minimum_required(VERSION 3.5)
project (destructionmatch C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party)

add_subdirectory(third_party/Boxer)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS})

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW lib only")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW lib only")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW lib only")
set(GLFW_INSTALL OFF CACHE BOOL "GLFW lib only")
add_subdirectory(third_party/glfw)

add_subdirectory(third_party/glad)

add_subdirectory(third_party/openal-soft)

add_subdirectory(third_party/cglm)

add_subdirectory(third_party/stb_image)

add_subdirectory(third_party/stb_truetype)

add_subdirectory(third_party/stb_vorbis)

include_directories(third_party/tinydir)

add_subdirectory(third_party/whereami)

include_directories(third_party/debugbreak)

add_subdirectory(third_party/fast_obj)

include_directories(include)
file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
	 
add_executable(${PROJECT_NAME} ${SRC})
if(MSVC)
	set_target_properties(
		${PROJECT_NAME}
		PROPERTIES
			LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
			LINK_FLAGS_RELEASE "/SUBSYSTEM:windows /ENTRY:mainCRTStartup"
			LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:windows /ENTRY:mainCRTStartup"
			LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:windows /ENTRY:mainCRTStartup"
	)
endif()

target_link_libraries(${PROJECT_NAME} Boxer ${OPENGL_LIBRARIES} glfw glad OpenAL::OpenAL cglm stb_image stb_truetype stb_vorbis whereami fast_obj)
if( MSVC )
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	if(${CMAKE_VERSION} VERSION_LESS "3.6.0") 
		message( "\n\t[ WARNING ]\n\n\tCMake version lower than 3.6.\n\n\t - Please update CMake and rerun; OR\n\t - Manually set StartUp Project in Visual Studio.\n" )
	else()
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
	endif()
endif()

#https://stackoverflow.com/questions/7747857/in-cmake-how-do-i-work-around-the-debug-and-release-directories-visual-studio-2
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/$<0:>)