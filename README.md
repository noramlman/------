# 校赛整合工程

> STM32H743VITx + OV2640 + 1.44 寸屏 + FreeRTOS(CMSIS-OS2)
> 构建方式：**CMake + Ninja + arm-none-eabi-gcc**（和实验室统一）
> 已验证：`arm-none-eabi-gcc 14.3.1` 编译 + 链接通过
> 　　FLASH 114.6 KB (5.5%)　RAM 205.4 KB (39.2%)

---

## 一、你的代码写在哪

### 唯一需要改的文件

```
Core/Src/app/vision_task.c      ← 算法写这里
```

配套头文件（一般不动）：

| 文件 | 作用 |
|---|---|
| `Core/Inc/app/app_config.h` | **总开关**：跑哪个任务、ROI 区域、扫描行、Otsu/固定阈值、要不要在屏上显示图像 |
| `Core/Inc/app/vision_task.h` | 视觉模块对外接口（结果结构体 + 函数声明） |

### 插入算法的位置

`vision_task.c` 的任务主循环里，有明确标记：

```c
xSemaphoreTake(xRgbFrameSem, ...);                  // 1. 等一帧
SCB_InvalidateDCache_by_Addr(...);                  // 2. 丢 Cache（不能省）

Vision_ProcessFrame(rgb_frame, rgb_frame_cnt);      // ★ 3. 你的算法在这里

/* 4. 显示 */  /* 5. 帧率统计 */
DCMI_RGB_Next();                                    // 6. 重启下一帧（snapshot 模式必须调）
```

`Vision_ProcessFrame()` 里现在实现的是：

| 步骤 | 函数 | 对应《视觉第一讲》 |
|---|---|---|
| RGB565 → 灰度 | `Rgb565_ToGray()` | 第 2 页 |
| 直方图 + Otsu | `Vision_Otsu()` | 第 5 页 |
| 逐行扫描找最长白区 | `Vision_FindEdges()` | 第 14 页 |
| 中线偏差 | `Vision_ProcessFrame()` | 第 14 页 |

**还没实现**：迷宫追边、去畸变/IPM、平滑+等距采样、预瞄点、转角+NMS、元素状态机、数字识别。

### 不要改的文件

| 文件 | 为什么不能改 |
|---|---|
| `Core/Src/dcmi_app.c` `ov2640.c` `lcd_144.c` `cam_lcd.c` `usb_vcp.c` | 驱动层，只调用。改了以后 CubeMX 或队友更新会冲突 |
| `cmake/stm32cubemx/CMakeLists.txt` | CubeMX 生成，重新生成就覆盖 |
| `startup_stm32h743xx.s` `STM32H743xx_FLASH.ld` | CubeMX 生成（链接脚本有例外，见第五节） |

---

## 二、驱动能不能用：能用，已验证

编译链接通过，说明这 5 个驱动的函数签名、依赖关系全部正确。可以直接调：

```c
/* dcmi_app.h —— 摄像头采集 */
DCMI_App_Init();                     // 建信号量、只留帧中断
DCMI_RGB_Start();                    // 配 OV2640 + 启动采集
xSemaphoreTake(xRgbFrameSem, ...);   // 等一帧好了
extern uint16_t rgb_frame[76800];    // 320x240 RGB565，150KB，直接用
DCMI_RGB_Next();                     // 处理完才能开下一帧

/* ov2640.h —— 摄像头配置（上面已内部调用，一般不用管） */
OV2640_Init();

/* lcd_144.h —— 屏幕 */
Lcd_Init(); Lcd_Clear(LCD_BLUE);
Lcd_ShowString(x, y, "文字", 前景色, 背景色);
Lcd_ShowCameraFrameScaled(rgb_frame, 320, 240);   // 缩放到 128x128
Lcd_FillRect(x, y, w, h, color);

/* cam_lcd.h —— 虚拟串口打印 */
Usb_PrintStr("hello\r\n");  Usb_Printf("[fps] %u\r\n", fps);
```

⚠️ **snapshot 模式的含义**：DMA 采完一帧就停，你处理完调 `DCMI_RGB_Next()` 才采下一帧。所以**你的算法越慢，帧率越低**，没有并行。这是例程的设计，别改成循环模式。

---

## 三、怎么构建

### 前置：工具链在 PATH 里

| 工具 | 版本 | 位置 |
|---|---|---|
| `arm-none-eabi-gcc` | 14.3.1 | `C:\Users\34566\AppData\Local\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin` |
| `cmake` | 4.4.0 | `...\stm32cube\bundles\cmake\4.4.0+st.1\bin` |
| `ninja` | 1.13.2 | `...\stm32cube\bundles\ninja\1.13.2+st.1\bin` |

### 编译

```bash
cmake --preset Debug
cmake --build --preset Debug
```

产物：`build/Debug/cam_lcd_demo.elf` / `.hex` / `.bin`

### VSCode

装 `CMake Tools` 扩展 → 选 `Debug` preset → 按 F7 构建。
`.vscode/c_cpp_properties.json` 已配好头文件路径（补全和跳转可用）。

---

## 四、构建系统怎么分工的

```
CMakeLists.txt                      ← 根：组合 + 用户层 + 生成 hex/bin
├── cmake/stm32cubemx/CMakeLists.txt  ← CubeMX 生成（HAL/FreeRTOS/USB/外设）
├── cmake/user_sources.cmake          ← 我们自己的 6 个源文件（CubeMX 不认识）
├── cmake/gcc-arm-none-eabi.cmake     ← CubeMX 生成（编译选项）
├── STM32H743xx_FLASH.ld              ← CubeMX 生成（链接脚本，需改，见第五节）
└── CMakePresets.json                 ← CubeMX 生成（Debug/Release preset）
```

**为什么单独抽 `user_sources.cmake`**：CubeMX 每次生成只覆盖 `cmake/stm32cubemx/`，不会动 `user_sources.cmake`。所以你的源文件列表不会丢。

---

## 五、CubeMX 重新生成后：必须跑修复脚本

⚠️ CubeMX 会覆盖 `main.c`、`FreeRTOSConfig.h`、链接脚本，并可能破坏几处关键内容。

**每次点完 GENERATE CODE，执行：**

```bash
python fix_after_generate.py
```

它会检查并修复：

| # | 问题 | CubeMX 生成后的默认值 | 修复成 |
|---|---|---|---|
| 1 | 任务栈太小 | `.stack_size = 128 * 4`（512 字节） | `1024 * 8`（8KB） |
| 2 | FreeRTOS 堆太小 | `configTOTAL_HEAP_SIZE = 15360` | `32768` |
| 3 | **`while (1) {` 被弄丢** | 只剩孤立的 `}`，编译报错 | 补回 |
| 4 | **`.data/.bss 落在 DTCM** | 128KB 装不下 150KB 帧缓冲，且 **DMA 访问不到 DTCM** | 改到 AXI SRAM (512KB) |
| 5 | `vision_task.c` 不在编译列表 | 丢失 | 补回 uvprojx / 由 user_sources.cmake 保证 |

第 3、4 条是**必踩的坑**，第 4 条尤其隐蔽——不改的话不是编译失败就是摄像头画面全黑。

---

## 六、第一次上板怎么验

1. `app_config.h` 里 `APP_USE_VISION` 改成 **0** → 编译烧录 → **屏幕出现摄像头实时画面**（验证硬件和驱动）
2. 改回 **1** → 屏幕显示 `fps / off / th`，USB 串口每秒打印：
   ```
   [fps] 21  frame 133  valid 1  off -12  th 154  white 3821
   ```
   - `off` = 近处中线相对画面中心的偏差（右为正）← **要交给控制的量**
   - `th` = Otsu 阈值，`white` = ROI 里白色像素数

看到这几个数在变，整条链路就通了。

---

## 七、关键参数在哪调

| 想改什么 | 文件 | 宏 |
|---|---|---|
| 处理区域 | `Core/Inc/app/app_config.h` | `ROI_X0/ROI_Y0/ROI_W/ROI_H` |
| 扫描行 | 同上 | `SCAN_ROW_NEAR/MID/FAR` |
| Otsu 还是固定阈值 | 同上 | `APP_USE_OTSU` / `APP_FIXED_THRESHOLD` |
| 要不要屏上显示摄像头 | 同上 | `APP_SHOW_CAMERA`（显示会占 SPI 时间，掉帧） |
| 分辨率 320x240 | `Core/Inc/dcmi_app.h` | `RGB_W/RGB_H`（要同步改 `app_config.h` 的 `IMG_W/IMG_H`） |
| FreeRTOS 堆 | `Core/Inc/FreeRTOSConfig.h` | `configTOTAL_HEAP_SIZE` |
| 任务栈 | `Core/Src/main.c` | `defaultTask_attributes.stack_size` |

---

## 八、引脚接线

全部是**母板固定接线**，11 个例程的 `.ioc` 完全一致、零冲突。详见 [Docs/01_板载接线表.md](Docs/01_板载接线表.md)。

一句话版：
- 摄像头 DCMI：PE0/1/4/5/6 + PD3 + PC6/7 + PA4 + PA6 + PB7
- 摄像头配置 I2C2：PB10/PB11
- 屏幕 SPI2：PB13/PB15 + PB0(BLK)/PB1(DC)/PB12(CS)/PB14(RST)
- IMU I2C1：PB8/PB9（**和摄像头不同总线，不冲突**）
- SD 卡 SDMMC1：PC8~PC12 + PD2
- 编码器 TIM1(PE9/PE11) + TIM4(PD12/PD13)
- PWM TIM2：PA0~PA3

---

## 九、其他文档

| 文件 | 内容 |
|---|---|
| `Docs/01_板载接线表.md` | 55 引脚完整接线表 |
| `Docs/02_工程结构.md` | 三层分层约定 |
| `Docs/03_整合步骤.md` | 加 SD/IMU/PWM/编码器的步骤 |
| `fix_after_generate.py` | CubeMX 重新生成后的自动修复 |
