find_package(directx-headers CONFIG REQUIRED)

function(add_note_code NOTE_NAME NOTE_SOURCES)
    add_executable(${NOTE_NAME} ${NOTE_SOURCES})
    target_link_libraries(${NOTE_NAME} PRIVATE Microsoft::DirectX-Headers "D3D12.lib" "DXGI.lib")
    target_precompile_headers(${NOTE_NAME} PRIVATE "common/stdafx.h")
    target_compile_options(${NOTE_NAME} PRIVATE /DUNICODE /D_UNICODE)
    target_compile_options(${NOTE_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
    target_compile_options(${NOTE_NAME} PRIVATE "$<$<C_COMPILER_ID:MSVC>:/source-charset:utf-8>")
endfunction()