#ifndef __FINSH_CONFIG_H__
#define __FINSH_CONFIG_H__

//#ifdef RT_USING_FINSH
#if 1

#define RT_FALSE        0
#define RT_TRUE         1
#define RT_ERROR        1
#define RT_NULL         NULL
#define RT_CONSOLEBUF_SIZE   256

#define RT_USING_HEAP

//#define RT_USING_DEVICE

#define DFS_USING_POSIX
#define RT_USING_POSIX_STDIO

#define DFS_USING_WORKDIR
#define FINSH_USING_SYMTAB

#define FINSH_USING_HISTORY
#define FINSH_USING_DESCRIPTION

#define FINSH_THREAD_STACK_SIZE 8192
#define FINSH_CMD_SIZE          256

#include <stdint.h>

void *hal_thread_create(const char *name, void (*thread_func)(void *), void *param, int stack_size, int prority);

#endif
#endif
