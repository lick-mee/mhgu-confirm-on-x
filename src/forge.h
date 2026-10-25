// Forge API 1.5.1 as the MHGU Recomp exposes it (signatures worked out from the port and an existing plugin).
#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef int s32;
typedef _Bool bool;
#define true 1
#define false 0
#define NULL ((void *)0)

#define EXPORT __attribute__((visibility("default")))

typedef struct {
    u16 major, minor, patch;
    char name[64];
} ForgePluginInfo;

// Filled by forge_hook_create: the hooked address, our replacement, and a callable trampoline to the original.
typedef struct {
    u32 target;
    u32 replacement;
    u32 original;
    u32 reserved[3];
} ForgeHook;

enum { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };

extern void forge_hook_create(ForgeHook *hook, u32 target, u32 replacement, u32 *original);
extern u32 forge_mem_getMainTextAddr(void);
extern void forge_log(int level, const char *fmt, ...);
extern u32 forge_singleton_getInstanceByName(const char *name);
extern u32 forge_singleton_getAllInstances(u32 *out, u32 max);
extern bool forge_input_isKeyPressed(u32 scancode);
extern void *malloc(u32 size);
extern void free(void *p);

// cimgui, as far as this plugin needs it.
extern void igSetCurrentContext(void *ctx);
extern void igSetAllocatorFunctions(void *alloc, void *free, void *user);
extern bool igTreeNode_Str(const char *label);
extern void igTreePop(void);
extern void igTextUnformatted(const char *text, const char *text_end);
extern bool igCheckbox(const char *label, bool *v);
