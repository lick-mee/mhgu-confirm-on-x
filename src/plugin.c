// Confirm on X: menus confirm with the bottom face button (PlayStation Cross, Xbox A) and cancel with the right
// one (Circle / Xbox B), while the hunter's controls stay as they are.
//
// sFestaPad (the game's sPad) keeps, besides the raw per-pad state the hunter reads, a menu input object
// (cMenuButtonData at sFestaPad+0x6BF0). Every frame it takes the active pad's held buttons through a getter that
// folds the left stick into the D-pad bits (main+0x3DE4F8), then derives pressed/released from the result; menu
// code tests its "decide" bit 0x2000 (right button) and "cancel" bit 0x4000 (bottom button) there. Swapping the
// two bits in that getter's result moves confirm/cancel in menus only, which is all the plugin does by default.
//
// Two optional modes (off by default, switched in the overlay) change what the hunter gets too. The hunter reads
// the raw pad state every pad's held/pressed buttons are built from (main+0xBAC204); the plugin changes it there.
// In towns (village, Hub, Pub) the two buttons swap, so talking to people and
// opening boxes use the bottom button (and the right one rolls). On quests, while the hunter has an interaction
// prompt on screen (gather, carve, supply box...), the bottom button becomes the interact button and the right one
// keeps doing that too, so nothing rolls while a prompt is up: a box opened from the prompt closes with the right
// button without a roll. The prompt shown is in the hunter's +0x32B4 (1..35, 0 = none; the HUD picks the prompt
// from it), the hunter coming from sPlayer's getMyPlayer (main+0x276278). The mode only changes while neither face
// button is held, so a press that starts a gather can't turn into a roll when the prompt goes away under it.
//
// The menu input is built from that changed state, so the menu getter puts back the face buttons actually pressed
// (swapped): menus always confirm with the bottom button and cancel with the right one, whatever the hunter's mode.
//
// Town or quest: sQuest's mQuestTimer (+0x1C) runs only while a quest is under way and is 0 in towns, also after
// coming back from one. Town/quest changes are logged, with whether quest data is loaded (mpQuestDataRes).
#include "forge.h"

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "dev"
#endif

#define MENU_BUTTONS_GET 0x3DE4F8
#define PAD_RAW_READ 0xBAC204
#define GET_MY_PLAYER 0x276278
#define QUEST_TIMER 0x1C
#define QUEST_DATA_RES 0xA0
#define PLAYER_PROMPT 0x32B4

#define BTN_DECIDE 0x2000u // right face button (Switch A)
#define BTN_CANCEL 0x4000u // bottom face button (Switch B)

void *memset(void *d, int c, u32 n) { u8 *p = d; while (n--) *p++ = (u8)c; return d; }

static ForgeHook g_menu_hook, g_raw_hook;
static bool g_hooked;
static bool g_enabled = true;
static bool g_town_interact = false;
static bool g_quest_prompt = false;
enum { RAW_AS_IS, RAW_SWAP, RAW_INTERACT };
static int g_raw_mode; // changed only between presses
static u32 g_main, g_quest, g_player_mgr;
static int g_on_quest = -1;

static u32 swap_buttons(u32 b) {
    u32 d = b & BTN_DECIDE, c = b & BTN_CANCEL;
    return (b & ~(BTN_DECIDE | BTN_CANCEL)) | (d ? BTN_CANCEL : 0) | (c ? BTN_DECIDE : 0);
}

static bool in_town(void) {
    if (!g_quest) g_quest = forge_singleton_getInstanceByName("sQuest");
    if (!g_quest) return true;
    int on = *(volatile u32 *)(g_quest + QUEST_TIMER) != 0;
    if (on != g_on_quest) {
        forge_log(LOG_INFO, "confirm-on-x: %s (quest data %s)", on ? "quest" : "town",
                  *(volatile u32 *)(g_quest + QUEST_DATA_RES) ? "loaded" : "none");
        g_on_quest = on;
    }
    return !on;
}

static bool prompt_shown(void) {
    if (!g_player_mgr) g_player_mgr = forge_singleton_getInstanceByName("sPlayer");
    if (!g_player_mgr) return false;
    u32 player = ((u32 (*)(u32))(g_main + GET_MY_PLAYER))(g_player_mgr);
    return player && *(volatile u32 *)(player + PLAYER_PROMPT) != 0;
}

static int wanted_raw_mode(void) {
    if (!g_enabled) return RAW_AS_IS;
    if (in_town()) return g_town_interact ? RAW_SWAP : RAW_AS_IS;
    return g_quest_prompt && prompt_shown() ? RAW_INTERACT : RAW_AS_IS;
}

#define FACE (BTN_DECIDE | BTN_CANCEL)
#define MAX_PADS 8

// What each pad's raw read had before the change and what it was changed to, to give menus the real buttons back.
static struct { u32 key, pressed, sent; } g_pads[MAX_PADS];

typedef u32 (*PadRawRead)(u32, u32, u32, u32 *);

static u32 pad_raw_read(u32 a0, u32 a1, u32 a2, u32 *state) {
    u32 r = ((PadRawRead)g_raw_hook.original)(a0, a1, a2, state);
    u32 b = state[0], pressed = b; // the low word of the u64 button state
    if (!(b & FACE)) g_raw_mode = wanted_raw_mode();
    if (g_raw_mode == RAW_SWAP) b = swap_buttons(b);
    else if (g_raw_mode == RAW_INTERACT && (b & BTN_CANCEL)) b = (b & ~BTN_CANCEL) | BTN_DECIDE;
    state[0] = b;
    for (u32 i = 0; i < MAX_PADS; i++)
        if (g_pads[i].key == a0 || !g_pads[i].key) { g_pads[i].key = a0; g_pads[i].pressed = pressed; g_pads[i].sent = b; break; }
    return r;
}

typedef u32 (*MenuButtonsGet)(u32 self, u32 pad_on);

static u32 menu_buttons_get(u32 self, u32 pad_on) {
    u32 b = ((MenuButtonsGet)g_menu_hook.original)(self, pad_on);
    if (!g_enabled) return b;
    u32 pressed = b & FACE;
    for (u32 i = 0; i < MAX_PADS && g_pads[i].key; i++)
        if (!((g_pads[i].sent ^ pad_on) & 0xFFFF)) { pressed = g_pads[i].pressed & FACE; break; }
    return (b & ~FACE) | swap_buttons(pressed);
}

#ifdef CONFIRM_DEV
// Dev build only: find game state by diffing every singleton between two kinds of snapshot ("A" and "B", keys 1
// and 2; 3 logs bytes that hold one value in every A snapshot and another in every B one, 0 clears).
void *memcpy(void *d, const void *s, u32 n) { u8 *p = d; const u8 *q = s; while (n--) *p++ = *q++; return d; }

#define MAX_SINGLETONS 512
#define MAX_TRACK_SIZE 0x100000
typedef struct {
    u32 obj, size;
    const char *name;
    u8 *ref_a, *ref_b, *alive;
} Tracked;
static Tracked g_tr[MAX_SINGLETONS];
static u32 g_ntr, g_na, g_nb;

static u32 dti_of(u32 obj) {
    u32 vt = *(u32 *)obj;
    return ((u32 (*)(u32)) * (u32 *)(vt + 5 * 4))(obj);
}

static void track_reset(void) {
    for (u32 i = 0; i < g_ntr; i++) { free(g_tr[i].ref_a); free(g_tr[i].ref_b); free(g_tr[i].alive); }
    memset(g_tr, 0, sizeof g_tr);
    g_ntr = g_na = g_nb = 0;
}

// A game object: a heap pointer whose vtable lies in main's data and whose getDTI entry in main's code.
static bool is_object(u32 p) {
    u32 main = forge_mem_getMainTextAddr();
    if (p < 0x08000000 || p >= 0x40000000 || (p & 3)) return false;
    u32 vt = *(u32 *)p;
    if (vt < main + 0x13F2000 || vt >= main + 0x1C50000 || (vt & 3)) return false;
    u32 fn = *(u32 *)(vt + 5 * 4);
    return fn >= main && fn < main + 0x13F1DEC;
}

static bool track_add(u32 obj, const char *name) {
    if (g_ntr >= MAX_SINGLETONS) return false;
    u32 dti = dti_of(obj);
    u32 size = (*(u32 *)(dti + 0x18) & 0x7FFFFF) << 2;
    if (size == 0 || size > MAX_TRACK_SIZE) size = size ? MAX_TRACK_SIZE : 0x1000;
    Tracked *t = &g_tr[g_ntr++];
    t->obj = obj;
    t->size = size;
    t->name = name ? name : *(const char **)(dti + 4);
    t->ref_a = malloc(size); t->ref_b = malloc(size); t->alive = malloc(size);
    if (!t->ref_a || !t->ref_b || !t->alive) { forge_log(LOG_WARN, "confirm-on-x: out of memory at %s", t->name); g_ntr--; return false; }
    return true;
}

static bool name_is(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return *a == *b; }

// Objects that hold the hunter's and the HUD's state hang off these managers.
static void track_children(u32 parent, u32 parent_size, const char *parent_name) {
    static char names[MAX_SINGLETONS][48];
    static u32 nnames;
    u32 limit = parent_size < 0x800 ? parent_size : 0x800;
    for (u32 o = 4; o < limit && nnames < MAX_SINGLETONS; o += 4) {
        u32 p = *(u32 *)(parent + o);
        bool seen = false;
        if (!is_object(p)) continue;
        for (u32 i = 0; i < g_ntr; i++) if (g_tr[i].obj == p) seen = true;
        if (seen) continue;
        char *n = names[nnames++], *q = n;
        const char *cls = *(const char **)(dti_of(p) + 4);
        for (const char *s = parent_name; *s && q < n + 16; s++) *q++ = *s;
        *q++ = '+';
        for (int i = 3; i >= 0; i--) *q++ = "0123456789ABCDEF"[(o >> (i * 4)) & 15];
        *q++ = ':';
        for (const char *s = cls; *s && q < n + 46; s++) *q++ = *s;
        *q = 0;
        if (!track_add(p, n)) return;
    }
}

static void track_init(void) {
    static u32 objs[MAX_SINGLETONS];
    u32 n = forge_singleton_getAllInstances(objs, MAX_SINGLETONS);
    for (u32 i = 0; i < n; i++) if (!track_add(objs[i], NULL)) break;
    u32 singletons = g_ntr;
    for (u32 i = 0; i < singletons; i++) {
        const char *nm = g_tr[i].name;
        if (name_is(nm, "sPlayer") || name_is(nm, "sUI") || name_is(nm, "sUIObject") || name_is(nm, "sFestaGUI") || name_is(nm, "sMenu"))
            track_children(g_tr[i].obj, g_tr[i].size, nm);
    }
    // One level deeper, where the hunter object itself may hang.
    for (u32 i = singletons, end = g_ntr; i < end; i++)
        if (name_is(g_tr[i].name, "sPlayer") || (g_tr[i].name[0] == 's' && g_tr[i].name[1] == 'P' && g_tr[i].name[7] == '+'))
            track_children(g_tr[i].obj, g_tr[i].size, "sPl>");
    for (u32 i = singletons; i < g_ntr; i++) forge_log(LOG_INFO, "confirm-on-x:   tracked %s (%X bytes)", g_tr[i].name, g_tr[i].size);
    forge_log(LOG_INFO, "confirm-on-x: tracking %u objects", g_ntr);
}

// alive bits per byte: STRICT = one value in every A snapshot and one other value in every B one; LOOSE = one value
// in every A snapshot and anything else in every B one (e.g. a different value per kind of prompt).
#define ALIVE_STRICT 1
#define ALIVE_LOOSE 2

static void snapshot(bool b) {
    if (!g_ntr) track_init();
    if (b && !g_na) { forge_log(LOG_WARN, "confirm-on-x: take an A snapshot (1) first"); return; }
    for (u32 i = 0; i < g_ntr; i++) {
        Tracked *t = &g_tr[i];
        const u8 *mem = (const u8 *)t->obj;
        if (!b) {
            if (!g_na) { memcpy(t->ref_a, mem, t->size); memset(t->alive, ALIVE_STRICT | ALIVE_LOOSE, t->size); }
            else for (u32 o = 0; o < t->size; o++) if (mem[o] != t->ref_a[o]) t->alive[o] = 0;
            continue;
        }
        if (!g_nb) memcpy(t->ref_b, mem, t->size);
        for (u32 o = 0; o < t->size; o++) {
            if (mem[o] == t->ref_a[o]) t->alive[o] = 0;
            else if (mem[o] != t->ref_b[o]) t->alive[o] &= ~ALIVE_STRICT;
        }
    }
    if (b) g_nb++; else g_na++;
    forge_log(LOG_INFO, "confirm-on-x: snapshot %s (%u A, %u B)", b ? "B" : "A", g_na, g_nb);
}

static u32 report(u8 kind, const char *label) {
    u32 shown = 0;
    for (u32 i = 0; i < g_ntr; i++) {
        Tracked *t = &g_tr[i];
        u32 here = 0;
        for (u32 o = 0; o < t->size; o++) {
            if ((t->alive[o] & (ALIVE_STRICT | ALIVE_LOOSE)) != kind) continue;
            if (here++ < 12) forge_log(LOG_INFO, "  %s %s+%X A=%02X B=%02X", label, t->name, o, t->ref_a[o], t->ref_b[o]);
        }
        if (here > 12) forge_log(LOG_INFO, "  %s %s: %u more", label, t->name, here - 12);
        shown += here;
    }
    return shown;
}

static void analyze(void) {
    forge_log(LOG_INFO, "confirm-on-x: analysis after %u A / %u B snapshots", g_na, g_nb);
    if (!g_nb) return;
    u32 strict = report(ALIVE_STRICT | ALIVE_LOOSE, "strict");
    u32 loose = report(ALIVE_LOOSE, "loose");
    forge_log(LOG_INFO, "confirm-on-x: %u strict, %u loose candidates", strict, loose);
}

EXPORT void forge_onUpdate(void) {
    if (forge_input_isKeyPressed(30)) snapshot(false);
    if (forge_input_isKeyPressed(31)) snapshot(true);
    if (forge_input_isKeyPressed(32)) analyze();
    if (forge_input_isKeyPressed(39)) { track_reset(); forge_log(LOG_INFO, "confirm-on-x: snapshots cleared"); }
}
#endif

EXPORT void forge_onLoad(ForgePluginInfo *info) {
    static const char name[] = "Confirm on X " PLUGIN_VERSION;
    if (!info) return;
    info->major = 1; info->minor = 5; info->patch = 1;
    memset(info->name, 0, sizeof info->name);
    for (u32 i = 0; i < sizeof name; i++) info->name[i] = name[i];
}

EXPORT void forge_onInit(void) {
    u32 main = g_main = forge_mem_getMainTextAddr();
    u32 original = 0; // non-null asks Forge for a trampoline to the original
    forge_hook_create(&g_menu_hook, main + MENU_BUTTONS_GET, (u32)menu_buttons_get, &original);
    forge_hook_create(&g_raw_hook, main + PAD_RAW_READ, (u32)pad_raw_read, &original);
    g_hooked = g_menu_hook.original && g_raw_hook.original;
    forge_log(g_hooked ? LOG_INFO : LOG_WARN, "confirm-on-x: %s", g_hooked ? "confirm on the bottom button" : "hook FAILED, mod inactive");
}

EXPORT void forge_onImGuiInit(void *ctx, void *alloc, void *freefn) {
    if (!ctx || !alloc || !freefn) return;
    igSetCurrentContext(ctx);
    igSetAllocatorFunctions(alloc, freefn, NULL);
}

EXPORT void forge_onImGuiRender(void) {
    if (!igTreeNode_Str("Confirm on X###confirm-on-x")) return;
    if (!g_hooked) igTextUnformatted("Hook failed: the mod is inactive (see log.txt).", NULL);
    igCheckbox("Confirm with bottom button, cancel with right", &g_enabled);
    igCheckbox("In towns, also talk/open boxes with the bottom button (right one rolls)", &g_town_interact);
    igCheckbox("On quests, the same while a gather/carve/box prompt is shown", &g_quest_prompt);
    igTextUnformatted(g_on_quest < 0 ? "Location: not known yet" : g_on_quest ? "Location: quest" : "Location: town", NULL);
    igTreePop();
}
