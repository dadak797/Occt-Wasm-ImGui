include(ExternalProject)  # To use ExternalProject command set

set(DEP_INSTALL_DIR ${PROJECT_BINARY_DIR}/install)
set(DEP_INCLUDE_DIR ${DEP_INSTALL_DIR}/include)
set(DEP_LIB_DIR ${DEP_INSTALL_DIR}/lib)

# SPDLOG: Fast Logger Library
# ExternalProject_Add(
#     dep_spdlog
#     GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
#     GIT_TAG "v1.x"
#     GIT_SHALLOW 1
#     UPDATE_COMMAND ""
#     PATCH_COMMAND ""
#     CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${DEP_INSTALL_DIR}
#     TEST_COMMAND ""
# )
# set(DEP_LIST ${DEP_LIST} dep_spdlog)
# set(DEP_LIBS ${DEP_LIBS} spdlog$<$<CONFIG:Debug>:d>)

# ImGUI
set(IMGUI_VERSION "1.88")
set(IMGUI_SOURCE_DIR ${CMAKE_SOURCE_DIR}/dependencies/ImGui/imgui-${IMGUI_VERSION})
add_library(imgui
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PRIVATE ${DEP_INCLUDE_DIR})
set(DEP_INCLUDE_DIR ${DEP_INCLUDE_DIR} ${IMGUI_SOURCE_DIR})
set(DEP_LIST ${DEP_LIST} imgui)
set(DEP_LIBS ${DEP_LIBS} imgui)

# OCCT
set(OCCT_VERSION "7.6.0")
set(OCCT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/dependencies/OCCT-wasm-${OCCT_VERSION}/inc)
set(OCCT_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/dependencies/OCCT-wasm-${OCCT_VERSION}/lib)
set(OCCT_LIBS 
    TKBin TKBinL TKBinTObj TKBinXCAF TKBO TKBool TKBRep TKCAF TKCDF TKernel
    TKFeat TKFillet TKG2d TKG3d TKGeomAlgo TKGeomBase TKHLR TKIGES TKLCAF TKMath
    TKMesh TKMeshVS TKOffset TKOpenGles TKPrim TKRWMesh TKService TKShHealing TKStd TKStdL
    TKSTEP TKSTEP209 TKSTEPAttr TKSTEPBase TKSTL TKTObj TKTopAlgo TKV3d TKVCAF TKVRML
    TKXCAF TKXDEIGES TKXDESTEP TKXMesh TKXml TKXmlL TKXmlTObj TKXmlXCAF TKXSBase
)
set(DEP_INCLUDE_DIR ${DEP_INCLUDE_DIR} ${OCCT_INCLUDE_DIR})
set(DEP_LIB_DIR ${DEP_LIB_DIR} ${OCCT_LIBRARY_DIR})
set(DEP_LIBS ${DEP_LIBS} ${OCCT_LIBS})