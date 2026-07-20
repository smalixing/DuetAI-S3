# LVGL 集成说明

本项目在 ESP32-S3 上用 LVGL v9 驱动一块 240×240 的 ST7789 屏(SPI 接口)。
本文档说明 LVGL 的初始化流程、显示接管方式、关键配置与内存布局。

代码入口:[`main/lvgl_port.c`](../main/lvgl_port.c) / [`main/lvgl_port.h`](../main/lvgl_port.h)。
LVGL 内存分配器覆盖:[`main/lvgl_mem_psram.c`](../main/lvgl_mem_psram.c)(详见
[内存与 PSRAM 使用规范](memory-psram.md))。

## 显示参数

| 项目 | 值 | 位置 |
|---|---|---|
| 分辨率 | 240 × 240 | `LCD_H_RES` / `LCD_V_RES` |
| 颜色深度 | RGB565（16 位） | `CONFIG_LV_COLOR_DEPTH=16` |
| 渲染模式 | Partial（部分刷新） | `LV_DISPLAY_RENDER_MODE_PARTIAL` |
| 部分缓冲 | 1/6 屏 = 240 × 40 像素 | `LVGL_BUF_LINES=40` |
| 刷新周期 | 33 ms（约 30 FPS） | `CONFIG_LV_DEF_REFR_PERIOD=33` |
| 任务轮询周期 | 10 ms | `LVGL_TASK_PERIOD_MS` |

## 初始化流程

`lvgl_port_init()` 由 `app_main` 在 `bsp_board_init()`(内部完成 `bsp_lcd_init()`)
之后调用,见 [`main/main.c:311`](../main/main.c#L311)。流程:

1. `lv_init()` + `lv_tick_set_cb()`:用 `esp_timer_get_time()` 提供毫秒级 tick。
2. `lv_display_create(240, 240)`:创建显示对象。
3. 分配两个部分绘制缓冲(double buffer),各 240×40×2 = 18.75 KB,用
   `MALLOC_CAP_DMA` 从**内部 RAM** 分配 —— SPI DMA 硬约束,不能放 PSRAM。
4. `lv_display_set_buffers(..., LV_DISPLAY_RENDER_MODE_PARTIAL)` + 绑定 flush 回调。
5. `bsp_lcd_set_cb()`:注册 LCD 传输完成回调,用于异步通知 LVGL 刷新结束。
6. 构建 UI(当前为 `lv_demo_widgets()` 占位),然后创建 `lvgl_task` 跑
   `lv_timer_handler()`。

> UI 在启动任务前构建,因此无需加锁。一旦引入运行时并发改 UI 的路径,需自行补锁
> (当前 `CONFIG_LV_OS_NONE=y`,LVGL 自身不提供线程安全)。

## 刷新路径(flush)

`lvgl_flush_cb()`([`lvgl_port.c:34`](../main/lvgl_port.c#L34)):

1. LVGL 渲染完一块区域后回调,交出 `px_map`(RGB565 小端)。
2. `lv_draw_sw_rgb565_swap()`:ST7789 需要大端 RGB565,做字节交换。
3. `bsp_lcd_flush(x1, y1, x2+1, y2+1, ...)`:注意 esp_lcd 用**开区间**结束坐标,故 +1。
4. 传输是异步的:传完由 LCD 的 trans-done 回调触发 `lvgl_flush_ready_cb()` →
   `lv_display_flush_ready()`,LVGL 才复用缓冲画下一块。double buffer 让渲染与传输
   可以重叠。

## 内存布局

| 项目 | 大小 | 位置 | 说明 |
|---|---|---|---|
| 两个部分绘制缓冲 | 2 × 18.75 KB ≈ 37.5 KB | 内部 DMA RAM | SPI DMA 需要,固定 |
| LVGL 堆(对象/样式/图层) | 按需增长 | **PSRAM** | 见下 |
| Layer simple 缓冲 | 最大 24 KB | LVGL 堆(PSRAM） | `CONFIG_LV_DRAW_LAYER_SIMPLE_BUF_SIZE=24576` |

### LVGL 堆迁到 PSRAM

历史上 LVGL 内建 TLSF 分配器有一个 64 KB 的编译期静态数组常驻内部 `.bss`,在
341 KB 的 DIRAM 上占用可观。现改为 custom malloc:

- `sdkconfig`:`CONFIG_LV_USE_CUSTOM_MALLOC=y`。
- [`main/lvgl_mem_psram.c`](../main/lvgl_mem_psram.c) 实现 LVGL 要求的核心分配函数
  (`lv_malloc_core` / `lv_realloc_core` / `lv_free_core` 及 `lv_mem_init` 等),
  转发到 `heap_caps_*(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`。

**链接要点(WHOLE_ARCHIVE)**:开启 custom malloc 后,LVGL 组件内所有内建 core 函数
定义会被 `#if` 掉,必须由外部提供。由于 `main` 组件内没有代码**直接调用**这些函数,
链接器默认不会从 `libmain.a` 抽取 `lvgl_mem_psram.c.obj`,会报
`undefined reference to lv_malloc_core`。因此 [`main/CMakeLists.txt`](../main/CMakeLists.txt)
的 `idf_component_register(...)` 加了 `WHOLE_ARCHIVE`,强制该目标文件参与链接。

> 权衡:LVGL 对象/样式访问频繁,放 PSRAM 有额外访问延迟,理论上会略降刷新性能。
> 240×240 屏实测可接受;若后续 UI 复杂化出现掉帧,可考虑按分配大小分流
> (大缓冲进 PSRAM、小对象留内部 RAM)。

## 关键 sdkconfig

```
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_USE_CUSTOM_MALLOC=y          # LVGL 堆走 lvgl_mem_psram.c → PSRAM
CONFIG_LV_OS_NONE=y                    # LVGL 不自带线程,UI 改动需自行加锁
CONFIG_LV_DEF_REFR_PERIOD=33           # ~30 FPS
CONFIG_LV_DRAW_LAYER_SIMPLE_BUF_SIZE=24576
CONFIG_LV_USE_DEMO_WIDGETS=y           # 占位 UI,生产环境应关闭
```

## 待办 / 注意

- **移除 demo**:当前 `lvgl_create_demo_ui()` 调 `lv_demo_widgets()` 作占位,
  会链入大量 widget 代码与字体常量(主要占 flash)。接入真实 UI 时应关闭
  `CONFIG_LV_USE_DEMO_WIDGETS` 并替换该函数。
- **线程安全**:`CONFIG_LV_OS_NONE=y`,若从 `lvgl_task` 之外的任务调 LVGL API,
  必须自行用互斥保护。
- **部分缓冲大小**:`LVGL_BUF_LINES` 决定内部 DMA RAM 占用与刷新粒度;内部 RAM
  紧张时可下调(20~30 行),但会增加 flush 次数。
