set(shader ${CMAKE_CURRENT_SOURCE_DIR}/triangle.slang)
set(shader_dir ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(vertex_code ${shader_dir}/triangle.vert.${NOGRAPHICSAPI_SHADER_EXTENSION})
set(fragment_code ${shader_dir}/triangle.frag.${NOGRAPHICSAPI_SHADER_EXTENSION})

NoGraphicsAPI_compile_slang(${vertex_code} ${shader} vertexMain vertex)
NoGraphicsAPI_compile_slang(${fragment_code} ${shader} fragmentMain fragment)

NoGraphicsAPI_add_example(example_triangle
    triangle.cpp
    ${shader}
    ${vertex_code}
    ${fragment_code}
)
target_compile_definitions(example_triangle PRIVATE
    NOGRAPHICSAPI_VERTEX_SHADER_PATH="${vertex_code}"
    NOGRAPHICSAPI_FRAGMENT_SHADER_PATH="${fragment_code}"
)
