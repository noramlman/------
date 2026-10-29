# =============================================================================
#  用户层源文件（视觉相关）
#
#  这些文件 CubeMX 不认识（不在 .ioc 里），所以由本文件手动加进目标。
#  CubeMX 重新生成代码不会改动本文件，可以放心维护。
# =============================================================================

set(USER_SOURCES
    ${CMAKE_SOURCE_DIR}/Core/Src/cam_lcd.c
    ${CMAKE_SOURCE_DIR}/Core/Src/dcmi_app.c
    ${CMAKE_SOURCE_DIR}/Core/Src/ov2640.c
    ${CMAKE_SOURCE_DIR}/Core/Src/lcd_144.c
    ${CMAKE_SOURCE_DIR}/Core/Src/usb_vcp.c
    ${CMAKE_SOURCE_DIR}/Core/Src/app/vision_task.c
    ${CMAKE_SOURCE_DIR}/Core/Src/app/debug_draw.c
    ${CMAKE_SOURCE_DIR}/Core/Src/app/user_proc.c
)

target_sources(${PROJECT_NAME} PRIVATE ${USER_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc/app
)
