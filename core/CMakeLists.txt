cmake_minimum_required(VERSION 3.20)
project("NigelTrain")

set(CMAKE_CXX_STANDARD 20)

# ============================================================
# LibTorch (find before add_executable)
# ============================================================
if (EXISTS "${PROJECT_SOURCE_DIR}/../GigaLearnCPP/libtorch/")
	message("Using libtorch from GigaLearnCPP folder...")
	list(APPEND CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}/../GigaLearnCPP/libtorch/share/cmake/Torch")
endif()

find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

# ============================================================
# Qt6 (find BEFORE add_executable for AUTOMOC)
# ============================================================
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")
find_package(Qt6 REQUIRED COMPONENTS Widgets Qml Quick QuickControls2 Charts)

# Manual AUTOMOC setup instead of qt_standard_project_setup()
# (qt_standard_project_setup adds /permissive- which breaks LibTorch headers)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Strip /permissive- from CMAKE_CXX_FLAGS (torch may add it)
string(REPLACE "/permissive-" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "-permissive-" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Strip /permissive- from Qt6::Platform interface (use string replace to preserve genexprs)
if(TARGET Qt6::Platform)
	get_target_property(_qt_plat_opts Qt6::Platform INTERFACE_COMPILE_OPTIONS)
	if(_qt_plat_opts)
		string(REPLACE "/permissive-" "" _qt_plat_opts "${_qt_plat_opts}")
		string(REPLACE "-permissive-" "" _qt_plat_opts "${_qt_plat_opts}")
		set_target_properties(Qt6::Platform PROPERTIES INTERFACE_COMPILE_OPTIONS "${_qt_plat_opts}")
	endif()
endif()

# Ensure Qt-required flags are present
add_compile_options(/Zc:__cplusplus -utf-8)

# Qt defines 'slots' as a macro, which conflicts with LibTorch's slots() method
add_compile_definitions(QT_NO_KEYWORDS)

# ============================================================
# RLGymCPP (from GigaLearnCPP)
# ============================================================
add_subdirectory("${PROJECT_SOURCE_DIR}/../GigaLearnCPP/RLGymCPP" "${CMAKE_BINARY_DIR}/RLGymCPP")

# ============================================================
# Source files + executable
# ============================================================
file(GLOB_RECURSE FILES_SRC "src/*.cpp" "src/*.h")
set(APP_ICON_RC "${PROJECT_SOURCE_DIR}/src/nigel.rc")
add_executable(NigelTrain WIN32 ${FILES_SRC} ${APP_ICON_RC})
set_target_properties(NigelTrain PROPERTIES LINKER_LANGUAGE CXX)

# ============================================================
# Link LibTorch
# ============================================================
target_link_libraries(NigelTrain PRIVATE "${TORCH_LIBRARIES}")

# nvToolsExt fix (CUDA 13+ dropped this lib)
if (WIN32)
	target_link_directories(NigelTrain PRIVATE "${TORCH_INSTALL_PREFIX}/lib")
	if (DEFINED ENV{CUDA_PATH})
		target_link_directories(NigelTrain PRIVATE "$ENV{CUDA_PATH}/lib/x64")
	endif()
endif()

if (TORCH_CUDA_LIBRARIES)
	message("Enabling CUDA support...")
	target_compile_definitions(NigelTrain PRIVATE -DRG_CUDA_SUPPORT)
endif()

# ============================================================
# Link RLGymCPP
# ============================================================
target_link_libraries(NigelTrain PUBLIC RLGymCPP)

# ============================================================
# JSON (nlohmann, from GigaLearnCPP)
# ============================================================
target_include_directories(NigelTrain PUBLIC "${PROJECT_SOURCE_DIR}/../GigaLearnCPP/libsrc/json")

# ============================================================
# Link Qt6
# ============================================================
target_link_libraries(NigelTrain PRIVATE
	Qt6::Widgets
	Qt6::Qml
	Qt6::Quick
	Qt6::QuickControls2
	Qt6::Charts
)

# ============================================================
# NVML (GPU monitoring)
# ============================================================
if (WIN32 AND DEFINED ENV{CUDA_PATH})
	target_include_directories(NigelTrain PRIVATE "$ENV{CUDA_PATH}/include")
	target_link_libraries(NigelTrain PRIVATE "$ENV{CUDA_PATH}/lib/x64/nvml.lib")
	target_compile_definitions(NigelTrain PRIVATE -DHAS_NVML)
endif()

# ============================================================
# Post-build: Deploy Qt DLLs
# ============================================================
if (WIN32)
	get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
	get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
	find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
	if (WINDEPLOYQT_EXECUTABLE)
		add_custom_command(TARGET NigelTrain POST_BUILD
			COMMAND "${WINDEPLOYQT_EXECUTABLE}"
				--qmldir "${PROJECT_SOURCE_DIR}/src/qml"
				"$<TARGET_FILE:NigelTrain>"
			COMMENT "Running windeployqt..."
		)
	endif()
endif()

# ============================================================
# Post-build: Copy QML files + icon
# ============================================================
add_custom_command(TARGET NigelTrain POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${PROJECT_SOURCE_DIR}/src/qml"
	"$<TARGET_FILE_DIR:NigelTrain>/qml"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${PROJECT_SOURCE_DIR}/src/nigel.ico"
	"$<TARGET_FILE_DIR:NigelTrain>/nigel.ico"
	COMMENT "Copying QML files and icon..."
)

# ============================================================
# Post-build: Copy collision_meshes (RocketSim)
# ============================================================
if (EXISTS "${PROJECT_SOURCE_DIR}/../collision_meshes")
	add_custom_command(TARGET NigelTrain POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${PROJECT_SOURCE_DIR}/../collision_meshes"
		"$<TARGET_FILE_DIR:NigelTrain>/collision_meshes"
		COMMENT "Copying collision_meshes..."
	)
endif()

# ============================================================
# Post-build: Copy RocketSimVis (visualizer)
# ============================================================
if (EXISTS "${PROJECT_SOURCE_DIR}/../RocketSimVis")
	add_custom_command(TARGET NigelTrain POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${PROJECT_SOURCE_DIR}/../RocketSimVis"
		"$<TARGET_FILE_DIR:NigelTrain>/RocketSimVis"
		COMMENT "Copying RocketSimVis..."
	)
endif()

# ============================================================
# Post-build: Copy torch DLLs
# ============================================================
if (MSVC)
	file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
	add_custom_command(TARGET NigelTrain POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${TORCH_DLLS}
		$<TARGET_FILE_DIR:NigelTrain>)
endif()
