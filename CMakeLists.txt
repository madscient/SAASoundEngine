cmake_minimum_required(VERSION 3.15)
project(SAASoundEngine VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- SAASound (git submodule: extern/SAASound) -----------------------------
set(SAASOUND_ROOT "${CMAKE_SOURCE_DIR}/extern/SAASound")

set(SAASOUND_SRCS
    ${SAASOUND_ROOT}/src/SAAAmp.cpp
    ${SAASOUND_ROOT}/src/SAADevice.cpp
    ${SAASOUND_ROOT}/src/SAAEnv.cpp
    ${SAASOUND_ROOT}/src/SAAFreq.cpp
    ${SAASOUND_ROOT}/src/SAAImpl.cpp
    ${SAASOUND_ROOT}/src/SAANoise.cpp
    ${SAASOUND_ROOT}/src/SAASndC.cpp
    ${SAASOUND_ROOT}/src/SAASound.cpp
    ${SAASOUND_ROOT}/src/SAAConfig.cpp
    ${SAASOUND_ROOT}/src/minIni/minIni.c
)

# ---- ラッパー共有ライブラリ ------------------------------------------------
add_library(SAASoundEngine SHARED
    src/SAASoundEngine.cpp
    ${SAASOUND_SRCS}
)

target_compile_definitions(SAASoundEngine PRIVATE
    FMENGINE_EXPORTS
    HAVE_CONFIG_H=1        # defns.h に src/saasound_cmake_config.h を読ませる
)

if (WIN32)
    target_compile_definitions(SAASoundEngine PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
        UNICODE
        _UNICODE
    )
    target_compile_options(SAASoundEngine PRIVATE /utf-8)
endif()

target_include_directories(SAASoundEngine PRIVATE
    src                          # FmEngineApi.h, saasound_cmake_config.h
    ${SAASOUND_ROOT}/include     # SAASound.h
    ${SAASOUND_ROOT}/src         # types.h, defns.h 等の内部ヘッダ
    ${SAASOUND_ROOT}/src/minIni  # minIni.h
)

# ---- 出力先 ----------------------------------------------------------------
set_target_properties(SAASoundEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# ---- インストール ----------------------------------------------------------
install(TARGETS SAASoundEngine
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
