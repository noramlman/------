"""
CubeMX 生成后自动恢复脚本（幂等，可以反复运行）

用法：
    python fix_after_generate.py            # 检查 + 修复
    python fix_after_generate.py --check    # 只检查不改

它会做 5 件事：
 1. main.c      : 确保 include app_config.h / vision_task.h，确保调用 VisionTask，栈 8KB
 2. FreeRTOSCfg : 确保 configTOTAL_HEAP_SIZE = 32768
 3. user_sources: 确保 vision_task.c 在 CMake 的编译列表里
 4. 检查应用层文件是否还在
 5. 检查 hal_msp.c / it.c 里摄像头/屏幕相关的引脚和中断是否还在
"""

import os
import re
import sys

PROJ = r'C:\Users\34566\Desktop\校赛\校赛整合工程'
CHECK_ONLY = '--check' in sys.argv

log = []


def note(msg):
    log.append(msg)
    print(msg)


def read_any(p):
    raw = open(p, 'rb').read()
    for enc in ('utf-8', 'gbk'):
        try:
            return raw.decode(enc), enc
        except UnicodeDecodeError:
            continue
    return raw.decode('utf-8', errors='replace'), 'utf-8'


def write_as(p, text, enc):
    open(p, 'wb').write(text.encode(enc))


# ============================================================
# 1. main.c
# ============================================================
main_c = os.path.join(PROJ, 'Core', 'Src', 'main.c')
s, enc = read_any(main_c)
orig = s
changed = []

if '#include "app_config.h"' not in s:
    if '#include "cam_lcd.h"' in s:
        s = s.replace('#include "cam_lcd.h"',
                      '#include "cam_lcd.h"\n#include "app_config.h"\n#include "vision_task.h"', 1)
        changed.append('补 include')
    elif '/* USER CODE BEGIN Includes */' in s:
        s = s.replace('/* USER CODE BEGIN Includes */',
                      '/* USER CODE BEGIN Includes */\n#include "app_config.h"\n#include "vision_task.h"', 1)
        changed.append('补 include')

# 任务入口
if 'VisionTask(argument)' not in s:
    def repl(m):
        return (m.group(1) +
                '#if APP_USE_VISION\n'
                '  VisionTask(argument);      /* 视觉算法 */\n'
                '#else\n'
                '  CamLcdTask(argument);      /* 原例程演示 */\n'
                '#endif' +
                m.group(2))
    s2, n = re.subn(
        r'(/\* USER CODE BEGIN 5 \*/\s*)CamLcdTask\(argument\);(\s*/\* USER CODE END 5 \*/)',
        repl, s, count=1)
    if n:
        s = s2
        changed.append('换任务入口')
    else:
        # CubeMX 可能改了大小写/位置，退而求其次：直接在 StartDefaultTask 里插
        s3, n3 = re.subn(r'(void StartDefaultTask\(void \*argument\)\s*\{[\s\S]*?)(\n\})',
                         lambda m: m.group(1) + '\n  VisionTask(argument);\n' + m.group(2),
                         s, count=1)
        if n3:
            s = s3
            changed.append('StartDefaultTask 插入 VisionTask')

# 栈大小：CubeMX 默认会是 128 * 4（=512 字节），对视觉任务远远不够
m_stack = re.search(r'\.stack_size\s*=\s*([^,\n]+),', s)
if m_stack:
    expr = m_stack.group(1).strip()
    if expr not in ('1024 * 8',):
        s = s.replace(m_stack.group(0), '.stack_size = 1024 * 8,', 1)
        changed.append('栈 %s -> 1024*8 (8KB)' % expr)

if s != orig:
    note('[main.c] 需要修改: ' + ', '.join(changed))
    if not CHECK_ONLY:
        write_as(main_c, s, enc)
        note('[main.c] 已写入')
else:
    note('[main.c] 无需修改 ✓')

# ============================================================
# 1b. main.c: 修复 CubeMX 丢失的 while (1) {
#     CubeMX 处理 USER CODE 3 块时会把 while(1){ 弄丢，留下孤立 }
# ============================================================
if '/* Infinite loop */' in s and 'while (1)\n' not in s and 'while (1)\r\n' not in s:
    pat = re.compile(r'(/\* Infinite loop \*/\s*)(/\* USER CODE BEGIN WHILE \*/)')
    s2, n = pat.subn(r'\1while (1)\n  {\n    \2', s, count=1)
    if n:
        s = s2
        changed.append('补回 while(1){')
        note('[main.c] 补回丢失的 while (1) {')

# ============================================================
# 2. FreeRTOSConfig.h
# ============================================================
fr = os.path.join(PROJ, 'Core', 'Inc', 'FreeRTOSConfig.h')
if os.path.exists(fr):
    s, enc = read_any(fr)
    m = re.search(r'#define\s+configTOTAL_HEAP_SIZE\s+\(\(size_t\)(\d+)\)', s)
    if m:
        cur = int(m.group(1))
        if cur < 32768:
            note('[FreeRTOSConfig.h] 堆 = %d，需要改到 32768' % cur)
            if not CHECK_ONLY:
                s = s.replace(m.group(0), '#define configTOTAL_HEAP_SIZE                    ((size_t)32768)')
                write_as(fr, s, enc)
                note('[FreeRTOSConfig.h] 已写入')
        else:
            note('[FreeRTOSConfig.h] 堆 = %d ✓' % cur)
    else:
        note('[FreeRTOSConfig.h] 没找到 configTOTAL_HEAP_SIZE，跳过')
else:
    note('[FreeRTOSConfig.h] 文件不存在!')

# ============================================================
# 3. 检查 cmake/user_sources.cmake 是否引用了 vision_task.c
#    （CMake 路线不依赖 uvprojx；MDK-ARM 已删除）
# ============================================================
us = os.path.join(PROJ, 'cmake', 'user_sources.cmake')
if os.path.exists(us):
    s_us, enc_us = read_any(us)
    if 'vision_task.c' in s_us:
        note('[user_sources.cmake] vision_task.c 已在编译列表 ✓')
    else:
        note('[user_sources.cmake] 缺少 vision_task.c！请手动补回')
        if not CHECK_ONLY:
            marker = 'target_sources(${PROJECT_NAME} PRIVATE ${USER_SOURCES})'
            line_to_add = 'list(APPEND USER_SOURCES ${CMAKE_SOURCE_DIR}/Core/Src/app/vision_task.c)'
            s_us = s_us.replace(marker, line_to_add + chr(10) + marker, 1)
            write_as(us, s_us, enc_us)
            note('[user_sources.cmake] 已补回')
else:
    note('[user_sources.cmake] 文件不存在!')

# ============================================================
# 4b. 链接脚本：.data/.bss 从 DTCMRAM 改到 RAM(AXI)
#     CubeMX 默认把 RW/ZI 放进 DTCM(128KB)，但：
#       ① 摄像头帧缓冲 rgb_frame 150KB 放不下
#       ② DTCM 不能被 DMA 访问，放了 DCMI 也会错
#     所以必须改成 AXI SRAM(0x24000000, 512KB)
# ============================================================
ld_path = os.path.join(PROJ, 'STM32H743xx_FLASH.ld')
if os.path.exists(ld_path):
    s, enc = read_any(ld_path)
    if '} >DTCMRAM' in s or '} >DTCMRAM AT> FLASH' in s:
        s = s.replace('} >DTCMRAM AT> FLASH', '} >RAM AT> FLASH')
        s = s.replace('} >DTCMRAM', '} >RAM')
        note('[链接脚本] .data/.bss 从 DTCMRAM 改到 RAM(AXI) —— 必须，否则帧缓冲放不下且 DMA 不可访问')
        if not CHECK_ONLY:
            write_as(ld_path, s, enc)
            note('[链接脚本] 已写入')
    else:
        note('[链接脚本] .data/.bss 已在 RAM ✓')
else:
    note('[链接脚本] 文件不存在!')

# ============================================================
# 5. 应用层文件
# ============================================================
must = [
    'Core/Inc/app/app_config.h',
    'Core/Inc/app/vision_task.h',
    'Core/Src/app/vision_task.c',
    'Core/Src/dcmi_app.c', 'Core/Inc/dcmi_app.h',
    'Core/Src/ov2640.c', 'Core/Inc/ov2640.h',
    'Core/Src/lcd_144.c', 'Core/Inc/lcd_144.h',
    'Core/Src/cam_lcd.c', 'Core/Inc/cam_lcd.h',
]
miss = [f for f in must if not os.path.exists(os.path.join(PROJ, *f.split('/')))]
note('[文件检查] 缺失: ' + (', '.join(miss) if miss else '无 ✓'))

# ============================================================
# 5. 系统层关键配置检查
# ============================================================
checks = {
    'Core/Src/stm32h7xx_hal_msp.c': [
        ('GPIO_AF13_DCMI', 'DCMI 引脚复用'),
        ('DMA_REQUEST_DCMI', 'DCMI 的 DMA 请求'),
        ('I2C2', '摄像头 SCCB (I2C2)'),
        ('SPI2', '屏幕 SPI2'),
        ('PC6', 'DCMI_D0'),
        ('PE6', 'DCMI_D7'),
        ('PB13', 'SPI2_SCK'),
    ],
    'Core/Src/stm32h7xx_it.c': [
        ('DCMI_IRQHandler', 'DCMI 帧中断'),
        ('DMA1_Stream0_IRQHandler', 'DMA1_Stream0 中断'),
    ],
}
for f, keys in checks.items():
    p = os.path.join(PROJ, *f.split('/'))
    if not os.path.exists(p):
        note('[%s] 文件不存在!' % f)
        continue
    t, _ = read_any(p)
    bad = [desc for k, desc in keys if k not in t]
    if bad:
        note('[%s] 缺关键配置: %s' % (f, ', '.join(bad)))
    else:
        note('[%s] 关键配置齐全 ✓' % f)

print()
print('=' * 50)
print('检查完毕' + ('（未修改任何文件）' if CHECK_ONLY else ''))
