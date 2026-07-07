# PageManager C语言版本设计文档

## 1. 概述

PageManager C版本是基于C++版本的完整移植，保留了原有的设计思想和功能特性，同时针对C语言的特点进行了适配和优化。该版本采用纯C语言实现，不依赖C++的任何特性，可以在纯C环境中使用。

## 2. 设计差异

### 2.1 类到结构体的转换

**C++版本：**
```cpp
class PageBase {
    // 虚函数
    virtual void on_view_load() {}
    virtual void on_view_will_appear() {}
    // ...
};

class PageManager {
    std::vector<PageBase*> _PagePool;
    std::stack<PageBase*> _PageStack;
    // ...
};
```

**C版本：**
```c
typedef struct pm_page_base {
    const pm_page_vtable_t* vtable;  // 虚函数表指针
    // 其他成员...
} pm_page_base_t;

typedef struct pm_manager {
    pm_page_array_t pool;  // 动态数组替代vector
    pm_page_array_t stack; // 动态数组替代stack
    // 其他成员...
} pm_manager_t;
```

### 2.2 虚函数到函数指针的转换

**C++版本：**
```cpp
virtual void on_view_load() {}
virtual void on_view_will_appear() {}
```

**C版本：**
```c
typedef struct {
    void (*on_view_load)(pm_page_base_t* self);
    void (*on_view_will_appear)(pm_page_base_t* self);
    // ...
} pm_page_vtable_t;
```

### 2.3 容器到动态数组的转换

**C++版本：**
```cpp
std::vector<PageBase*> _PagePool;
std::stack<PageBase*> _PageStack;
```

**C版本：**
```c
typedef struct {
    pm_page_base_t** data;
    uint32_t     size;
    uint32_t     capacity;
} pm_page_array_t;
```

## 3. 核心功能实现

### 3.1 路由功能 (pm_router.c)

实现了与C++版本相同的路由功能：

- **Push**：进入新页面，旧页面压入栈
- **Pop**：返回上一页
- **Replace**：替换当前页面
- **SwitchTo**：核心切换逻辑
- **BackHome**：返回首页

```c
bool page_manager_push(pm_manager_t* self, const char* name, const pm_page_stash_t* stash);
bool page_manager_pop(pm_manager_t* self);
bool page_manager_replace(pm_manager_t* self, const char* name, const pm_page_stash_t* stash);
bool page_manager_back_home(pm_manager_t* self);
```

### 3.2 拖拽功能 (pm_drag.c)

实现了完整的拖拽手势处理：

- **on_root_drag_event**：拖拽事件回调
- **inertia prediction**：惯性预测
- **gesture detection**：手势检测
- **async navigation**：异步导航

```c
void page_manager_root_enable_drag(pm_manager_t* self, lv_obj_t* root);
/* 惯性预测为 pm_drag.c 内部静态函数 pm_root_get_drag_predict() */
```

## 4. 内存管理

### 4.1 动态数组管理

使用自定义的`pm_page_array_t`结构管理动态数组：

```c
bool page_array_push(pm_page_array_t* arr, pm_page_base_t* p); /* 导出: pm_router.c 使用 */
static void page_array_pop(pm_page_array_t* arr);
static pm_page_base_t* page_array_top(const pm_page_array_t* arr);
static bool page_array_reserve(pm_page_array_t* arr, uint32_t need);
```

### 4.2 缓存机制

保留了C++版本的缓存机制：

- **自动缓存**：页面卸载时保留缓存，下次加载时直接复用
- **手动缓存**：通过`SetCustomCacheEnable`和`SetCustomAutoCacheEnable`控制
- **内存释放**：使用`lv_mem_free`释放内存

## 5. 参数传递机制

### 5.1 Stash参数传递

保留了C++版本的Stash参数传递机制：

```c
// 发送参数
int value = 42;
page_manager_push(manager, "TargetPage", &(pm_page_stash_t){&value, sizeof(value)});

// 接收参数
void on_view_load(pm_page_base_t* self) {
    int received_value;
    PM_PAGE_STASH_POP(self, received_value);
    // 使用received_value
}
```

### 5.2 内存拷贝

使用`lv_mem_alloc`和`memcpy`实现参数的安全拷贝：

```c
void* buffer = lv_mem_alloc(stash->size);
memcpy(buffer, stash->ptr, stash->size);
```

## 6. 动画系统

### 6.1 动画类型

支持全部C++版本的动画类型：

- **覆盖式动画**：OVER_LEFT, OVER_RIGHT, OVER_TOP, OVER_BOTTOM
- **推动式动画**：MOVE_LEFT, MOVE_RIGHT, MOVE_TOP, MOVE_BOTTOM
- **淡入淡出**：FADE_ON
- **无动画**：NONE

### 6.2 动画属性

使用`pm_load_anim_attr_t`结构定义动画属性：

```c
typedef struct {
    pm_anim_setter_t setter;     // 动画setter函数
    pm_anim_getter_t getter;     // 动画getter函数
    pm_root_drag_dir_t    drag_dir;    // 拖拽方向
    pm_anim_value_t      push;       // 推入动画值
    pm_anim_value_t      pop;        // 弹出动画值
} pm_load_anim_attr_t;
```

## 7. 状态机

### 7.1 页面状态

保留了完整的页面生命周期状态：

```c
typedef enum {
    PM_PAGE_STATE_IDLE,
    PM_PAGE_STATE_LOAD,
    PM_PAGE_STATE_WILL_APPEAR,
    PM_PAGE_STATE_DID_APPEAR,
    PM_PAGE_STATE_ACTIVITY,
    PM_PAGE_STATE_WILL_DISAPPEAR,
    PM_PAGE_STATE_DID_DISAPPEAR,
    PM_PAGE_STATE_UNLOAD,
    _PM_PAGE_STATE_LAST
} pm_page_state_t;
```

### 7.2 状态转换

通过`page_manager_state_update`函数管理状态转换：

```c
void page_manager_state_update(pm_manager_t* self, pm_page_base_t* base);
```

## 8. 错误处理

### 8.1 日志系统

使用统一的日志宏进行错误处理：

```c
PM_LOG_ERROR("Page(%s) was not install", name);
PM_LOG_WARN("Page stack is empty, cat't pop");
PM_LOG_INFO("Page(%s) push >> [Screen]", name);
```

### 8.2 空指针检查

对所有输入参数进行空指针检查：

```c
if (self == NULL || name == NULL) return false;
if (base == NULL) {
    PM_LOG_ERROR("Page base is NULL");
    return;
}
```

## 9. 使用示例

### 9.1 完整可运行示例（demo 目录）

`demo/` 目录提供了一个完整的、基于 LVGL UI 的两页面示例，演示页面实现、工厂注册、Push/Pop/Replace/BackHome 导航以及 stash 参数传递：

- `demo/page_manager_c_demo.h` —— 对外入口声明
- `demo/page_manager_c_demo.c` —— HomePage + DetailPage 两个具体页面、工厂与导航逻辑

集成方式：在应用初始化处调用 `page_manager_c_demo_start();`，退出时调用 `page_manager_c_demo_stop();`。

```c
#include "PageManager_C/demo/page_manager_c_demo.h"

page_manager_c_demo_start();   /* 显示 Home 页，按钮驱动后续导航 */
/* ... */
page_manager_c_demo_stop();    /* 清栈、卸载页面、释放管理器 */
```

> 注意：示例的 `.c` 文件用相对路径 `#include "../page_manager.h"` 引用 C 版头文件，
> 以避免与工程中同名的 C++ 版 `page_manager.h`（其 `-I` 路径已在 Makefile 中）发生冲突。
> 页面对象由工厂用 `lv_mem_alloc` 分配，并在 `on_destroy` 回调里用 `lv_mem_free` 释放
> （`page_manager_uninstall` 只调用 `on_destroy`，不会自动释放页面本身）。

### 9.2 初始化页面管理器

```c
// 创建页面工厂
pm_factory_t my_factory = {
    .create = my_create_page_function
};

// 初始化页面管理器
pm_manager_t manager;
page_manager_init(&manager, &my_factory);

// 安装页面
page_manager_install(&manager, "MainPage", "MainPage");
page_manager_install(&manager, "SettingsPage", "SettingsPage");

// 页面导航
page_manager_push(&manager, "MainPage", NULL);
page_manager_push(&manager, "SettingsPage", NULL);
page_manager_pop(&manager);
page_manager_replace(&manager, "SettingsPage", NULL);
```

### 9.3 创建自定义页面

```c
// 定义页面虚函数表
static void my_page_on_view_load(pm_page_base_t* self) {
    // 页面加载逻辑
}

static const pm_page_vtable_t my_page_vtable = {
    .on_view_load = my_page_on_view_load,
    // 其他回调函数...
};

// 创建页面实例
static pm_page_base_t my_page_instance = {
    .vtable = &my_page_vtable,
    .name = "MyPage",
    // 其他初始化...
};
```

## 10. 性能优化

### 10.1 内存优化

- 动态数组按需扩容，避免频繁内存分配
- 缓存机制减少页面重建开销
- 及时释放不再使用的内存

### 10.2 CPU优化

- 状态机避免不必要的计算
- 动画系统使用LVGL原生API
- 拖拽手势优化减少重复计算

## 11. 兼容性

### 11.1 LVGL版本

兼容LVGL v8.x版本，使用标准API：

- `lv_obj_move_foreground`
- `lv_anim_start`
- `lv_event_send`
- `lv_mem_alloc`

### 11.2 平台支持

支持各种嵌入式平台：

- ARM Cortex-M系列
- ESP32
- STM32
- 其他支持LVGL的平台

## 12. 总结

PageManager C版本成功地将C++版本的核心思想和功能移植到了纯C环境中，保持了原有的设计理念和用户体验，同时针对C语言的特点进行了优化。该版本具有良好的性能、完整的特性和广泛的兼容性，是嵌入式GUI应用开发的理想选择。