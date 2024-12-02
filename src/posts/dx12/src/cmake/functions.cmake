find_package(directx-headers CONFIG REQUIRED)

function(add_note_code NOTE_NAME NOTE_SOURCES)
    add_executable(${NOTE_NAME})
    foreach(CUR_SOURCE IN LISTS NOTE_SOURCES)
        target_sources(${NOTE_NAME} PRIVATE "${NOTE_NAME}/${CUR_SOURCE}")
    endforeach()
    target_link_libraries(${NOTE_NAME} PRIVATE Microsoft::DirectX-Headers D3D12.lib DXGI.lib D3DCompiler.lib dxguid.lib)
    target_precompile_headers(${NOTE_NAME} PRIVATE "common/stdafx.h")
    target_compile_options(${NOTE_NAME} PRIVATE /DUNICODE /D_UNICODE)
    target_compile_options(${NOTE_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
    target_compile_options(${NOTE_NAME} PRIVATE "$<$<C_COMPILER_ID:MSVC>:/source-charset:utf-8>")
    if (${ENABLE_VSYNC})
        target_compile_definitions(${NOTE_NAME} PRIVATE "ENABLE_VSYNC")
    endif()
    target_include_directories(${NOTE_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/")
    set_target_properties(${NOTE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/${NOTE_NAME}/bin>")
    set_target_properties(${NOTE_NAME} PROPERTIES
        OUTPUT_NAME_DEBUG ${NOTE_NAME}d
        OUTPUT_NAME_RELEASE ${NOTE_NAME}
        OUTPUT_NAME_RELWITHDEBINFO ${NOTE_NAME}rd
        OUTPUT_NAME_MINSIZEREL ${NOTE_NAME}mr
    )
endfunction()

function(add_note_asset NOTE_NAME NOTE_ASSETS)
    foreach(CUR_ASSET IN LISTS NOTE_ASSETS)
        add_custom_command(
            TARGET ${NOTE_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${CMAKE_SOURCE_DIR}/${NOTE_NAME}/${CUR_ASSET}"
                    "${CMAKE_BINARY_DIR}/${NOTE_NAME}/${CUR_ASSET}")
    endforeach()
endfunction()