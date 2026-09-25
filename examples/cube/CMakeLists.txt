set(shader ${CMAKE_CURRENT_SOURCE_DIR}/cube.slang)
set(shader_dir ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(vertex_code ${shader_dir}/cube.vert.${NOGRAPHICSAPI_SHADER_EXTENSION})
set(fragment_code ${shader_dir}/cube.frag.${NOGRAPHICSAPI_SHADER_EXTENSION})
NoGraphicsAPI_compile_slang(${vertex_code} ${shader} vertexMain vertex
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cube_shared.h)
NoGraphicsAPI_compile_slang(${fragment_code} ${shader} fragmentMain fragment
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cube_shared.h)

set(texture_name lunarg_logo_256x256.rgba8)
set(texture_source ${CMAKE_CURRENT_SOURCE_DIR}/${texture_name})
set(texture ${CMAKE_CURRENT_BINARY_DIR}/${texture_name})
file(SIZE ${texture_source} texture_size)
if(NOT texture_size EQUAL 262144)
    message(FATAL_ERROR "Cube RGBA8 texture must be exactly 262144 bytes")
endif()
configure_file(${texture_source} ${texture} COPYONLY)

NoGraphicsAPI_add_example(example_cube
    cube.cpp
    cube_shared.h
    ${shader}
    ${texture_source}
    NOTICE.md
    LICENSE-Apache-2.0.txt
    ${vertex_code}
    ${fragment_code}
)
target_link_libraries(example_cube PRIVATE NoGraphicsAPIUtility::math NoGraphicsAPIUtility::textures NoGraphicsAPIUtility::uploads)
target_compile_definitions(example_cube PRIVATE
    NOGRAPHICSAPI_CUBE_VERTEX_SHADER_PATH="${vertex_code}"
    NOGRAPHICSAPI_CUBE_FRAGMENT_SHADER_PATH="${fragment_code}"
    NOGRAPHICSAPI_CUBE_TEXTURE_PATH="${texture}"
)
