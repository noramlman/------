# =============================================================================
#  校赛整合工程 · 根 CMakeLists
#
#  说明：
#    - 外设初始化、HAL、FreeRTOS、USB 全部由 CubeMX 生成在
#      cmake/stm32cubemx/CMakeLists.txt 里，本文件只负责把它们组合起来。
#    - 视觉那 6 个源文件 CubeMX 不认识，放在 cmake/user_sources.cmake 里。
#    - CubeMX 重新生成时只会覆盖 cmake/stm32cubemx/ 下的文件，
#      本文件和 cmake/user_sources.cmake 不会被改。
#
#  构建：
#    cmake --preset Debug
#    cmake --build --preset Debug
# =============================================================================

cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

project(cam_lcd_demo C CXX ASM)

# add_executable 至少要有一个源文件，这里用占位文件；
# 真正的源文件由 cmake/stm32cubemx/CMakeLists.txt 和 cmake/user_sources.cmake 追加。
add_executable(${PROJECT_NAME} cmake/placeholder.c)

# ---- CubeMX 生成部分（HAL / FreeRTOS / USB / 外设初始化）----
include(cmake/stm32cubemx/CMakeLists.txt)

# ---- 用户层（视觉驱动 + 算法）----
include(cmake/user_sources.cmake)

# ---- 产物：hex / bin + 体积 ----
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "生成 hex / bin 并打印体积"
)
