/* madden19: PS4 functions Madden NFL 19 (CUSA10038, Frostbite) imports that the UFC ports never
 * needed (logs\imports.txt from --list-imports, 2026-10-19). Two kinds:
 *   - real behaviour where the game depends on it: POSIX semaphores, sleeping, memory queries,
 *     PlayGo languages, the display safe area;
 *   - "not available" for features with no offline meaning (PSN profiles and dialogs, the
 *     companion app server, camera face analysis, sockets, the hardware video decoder), so the
 *     game takes its fallback path. Each of those is logged the first time it is called. */
#include "runtime.h"
#include "platform/platform.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR(n) ((int32_t)(UINT32_C(0x80020000)|(n)))
/* FreeBSD errno values the guest reads. */
#define GUEST_EINVAL 22
#define GUEST_ENETDOWN 50

/* One line the first time each stub runs: what the game asked for and what it got. */
#define SAY_ONCE(...) do { static atomic_flag said_ = ATOMIC_FLAG_INIT; \
    if (!atomic_flag_test_and_set(&said_)) { printf(__VA_ARGS__); fflush(stdout); } } while (0)

static void sleep_ns(uint64_t ns) { platform_sleep_ns(ns); }

/* ---- libkernel ---- */
/* Frostbite names its memory regions for the console's debugger; nothing to do here. */
static ABI int32_t set_virtual_range_name(void *address, uint64_t size, const char *name) {
    (void)address; (void)size; (void)name; return 0;
}
static ABI int32_t kernel_sleep(uint32_t seconds) { sleep_ns((uint64_t)seconds * 1000000000u); return 0; }
static ABI int32_t available_flexible_memory(uint64_t *size) {
    if (!size) return ERR(GUEST_EINVAL);
    *size = runtime_memory_flexible_available();
    return 0;
}
/* Every module is linked offline; there is no dynamic symbol table to search. */
static ABI int32_t kernel_dlsym(int32_t handle, const char *symbol, void **address) {
    SAY_ONCE("Runtime: sceKernelDlsym(%d, %s) -> not found (modules are linked offline)\n", handle, symbol ? symbol : "(null)");
    if (address) *address = NULL;
    return ERR(3); /* ESRCH */
}

/* ---- libScePosix ---- */
/* PS4 sem_t is a pointer to the semaphore object (libkernel's posix semaphores). */
typedef struct { pthread_mutex_t lock; pthread_cond_t ready; uint32_t count; } GuestSem;
static ABI int32_t sem_init_guest(GuestSem **sem, int32_t pshared, uint32_t value) {
    (void)pshared;
    if (!sem) { *runtime_errno() = GUEST_EINVAL; return -1; }
    GuestSem *s = calloc(1, sizeof(*s));
    if (!s) { *runtime_errno() = 12; return -1; } /* ENOMEM */
    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->ready, NULL);
    s->count = value;
    *sem = s;
    return 0;
}
static ABI int32_t sem_destroy_guest(GuestSem **sem) {
    if (!sem || !*sem) { *runtime_errno() = GUEST_EINVAL; return -1; }
    GuestSem *s = *sem;
    pthread_cond_destroy(&s->ready);
    pthread_mutex_destroy(&s->lock);
    free(s);
    *sem = NULL;
    return 0;
}
static ABI int32_t sem_post_guest(GuestSem **sem) {
    if (!sem || !*sem) { *runtime_errno() = GUEST_EINVAL; return -1; }
    GuestSem *s = *sem;
    pthread_mutex_lock(&s->lock);
    ++s->count;
    pthread_cond_signal(&s->ready);
    pthread_mutex_unlock(&s->lock);
    return 0;
}
static ABI int32_t sem_wait_guest(GuestSem **sem) {
    if (!sem || !*sem) { *runtime_errno() = GUEST_EINVAL; return -1; }
    GuestSem *s = *sem;
    pthread_mutex_lock(&s->lock);
    while (!s->count) pthread_cond_wait(&s->ready, &s->lock);
    --s->count;
    pthread_mutex_unlock(&s->lock);
    return 0;
}
static ABI int32_t pthread_equal_guest(void *a, void *b) { return a == b; }
/* No network: sockets fail with ENETDOWN. select() with no descriptors is a sleep. */
static ABI int32_t posix_no_network(void) {
    SAY_ONCE("Runtime: POSIX socket call -> ENETDOWN (offline)\n");
    *runtime_errno() = GUEST_ENETDOWN;
    return -1;
}
typedef struct { int64_t sec, usec; } GuestTimeval;
static ABI int32_t select_guest(int32_t n, void *r, void *w, void *e, const GuestTimeval *timeout) {
    (void)r; (void)w; (void)e;
    if (n == 0) {
        if (timeout) sleep_ns((uint64_t)timeout->sec * 1000000000u + (uint64_t)timeout->usec * 1000u);
        return 0;
    }
    return posix_no_network();
}

/* ---- libSceJson2 (C++ classes) ----
 * Madden reads EA's online/config JSON with these. Offline there is nothing to parse: the parser
 * reports an error, values are null, strings keep their text (a String holds one pointer). The
 * objects themselves live in the game's memory with its own sizes; only the first 8 bytes of a
 * String are used here, and Values and Arrays are not written at all. */
static const unsigned char null_value[64];
static const char *empty_text = "";
static const char *string_text(const void *self) { const char *t = self ? *(char *const *)self : NULL; return t ? t : empty_text; }
static ABI void json_string_ctor(void *self) { if (self) *(char **)self = NULL; }
static ABI void json_string_ctor_text(void *self, const char *text) {
    if (!self) return;
    size_t n = text ? strlen(text) : 0;
    char *copy = malloc(n + 1);
    if (copy) { if (n) memcpy(copy, text, n); copy[n] = 0; }
    *(char **)self = copy;
}
static ABI void json_string_dtor(void *self) { if (self) { free(*(char **)self); *(char **)self = NULL; } }
static ABI uint64_t json_string_size(const void *self) { return strlen(string_text(self)); }
static ABI const char *json_string_c_str(const void *self) { return string_text(self); }
static ABI void json_object_noop(void *self) { (void)self; }
static ABI void json_object_noop2(void *self, const void *other) { (void)self; (void)other; }
static ABI int32_t json_parse(void *value, const char *text, uint64_t size) {
    (void)value; (void)text;
    SAY_ONCE("Runtime: sce::Json::Parser::parse(%llu bytes) -> error (JSON not supported offline)\n", (unsigned long long)size);
    return (int32_t)0x80920001; /* invalid argument class */
}
static ABI int32_t json_fail1(void *self, const void *other) { (void)self; (void)other; return (int32_t)0x80920001; }
static ABI int32_t json_ok3(void *self, const void *a, const void *b) { (void)self; (void)a; (void)b; return 0; }
static ABI const void *json_null_ref1(const void *self, uintptr_t key) { (void)self; (void)key; return null_value; }
static ABI uint64_t json_zero(const void *self) { (void)self; return 0; }
static ABI const void *json_empty_string(const void *self) { (void)self; static const char *const holder = NULL; return &holder; }

/* ---- PSN (NpToolkit2 V2 user profiles, friend list dialog, account language, party) ----
 * The console is signed out: requests are empty objects, responses carry an error, dialogs do
 * not open. */
#define NP_SIGNED_OUT ((int32_t)0x80550006)
static ABI int32_t np_response_error(const void *self) { (void)self; return NP_SIGNED_OUT; }
static ABI const void *np_response_none(const void *self) { (void)self; return NULL; }
static ABI int32_t np_signed_out(void) { SAY_ONCE("Runtime: PSN request -> signed out (offline)\n"); return NP_SIGNED_OUT; }
static ABI int32_t np_ok(void) { return 0; }
static ABI int32_t dialog_status_none(void) { return 0; } /* SCE_COMMON_DIALOG_STATUS_NONE */
static ABI int32_t dialog_unavailable(void) { SAY_ONCE("Runtime: PSN friend list dialog -> unavailable (offline)\n"); return (int32_t)0x80B80003; }

/* ---- companion app server (second screen) ---- */
static ABI int32_t companion_unavailable(void) {
    SAY_ONCE("Runtime: PS4 Second Screen companion server -> unavailable\n");
    return (int32_t)0x80E40001;
}
static ABI int32_t companion_no_event(void) { return (int32_t)0x80AD0008; } /* no event */

/* ---- camera face analysis: memory sizes only (no PlayStation Camera, nothing is analysed) ---- */
static ABI int32_t face_working_memory(void) { return 0x100000; }

/* ---- hardware video decoders (libSceVideodec, libSceVideodec2) ----
 * Not provided: a decoder cannot be created, so the game's own player sees an error and skips
 * the video. */
static ABI int32_t videodec_unavailable(void) {
    SAY_ONCE("Runtime: hardware video decoder (sceVideodec) -> unavailable, videos skipped\n");
    return (int32_t)0x80C10001;
}

/* ---- PlayGo languages, safe area, download data space ---- */
static ABI int32_t playgo_get_language_mask(int32_t handle, uint64_t *mask) {
    (void)handle; if (!mask) return (int32_t)0x80B20002; *mask = ~UINT64_C(0); return 0;
}
static ABI int32_t playgo_set_language_mask(int32_t handle, uint64_t mask) { (void)handle; (void)mask; return 0; }
typedef struct { float ratio; uint8_t reserved[128]; } DisplaySafeArea;
static ABI int32_t display_safe_area(DisplaySafeArea *info) {
    if (!info) return (int32_t)0x80A10003;
    memset(info, 0, sizeof(*info));
    info->ratio = 1.0f;
    return 0;
}
static ABI int32_t download_space_kb(const char *mount, uint64_t *kb) {
    (void)mount; if (!kb) return (int32_t)0x80D90002; *kb = UINT64_C(4) * 1024 * 1024; return 0;
}
/* libSceDiscMap NID fJgP+wqifno (unnamed): shadPS4 answers "no bitmap info" (the game is installed). */
static ABI int32_t discmap_no_bitmap(void) { return (int32_t)0x81100004; }
/* Unnamed imports from libSceNpToolkit2 (YlpnKm99h3Q) and EAWebKit (qiOZEn26Kds). */
static ABI int32_t unnamed_toolkit(void) { SAY_ONCE("Runtime: NpToolkit2 YlpnKm99h3Q -> signed out\n"); return NP_SIGNED_OUT; }
static ABI uintptr_t unnamed_eawebkit(void) { SAY_ONCE("Runtime: EAWebKit qiOZEn26Kds -> 0 (web views unavailable)\n"); return 0; }

static const RuntimeExport exports[] = {
    {"sceKernelSetVirtualRangeName", set_virtual_range_name}, {"sceKernelSleep", kernel_sleep},
    {"sceKernelAvailableFlexibleMemorySize", available_flexible_memory}, {"sceKernelDlsym", kernel_dlsym},
    {"sem_init", sem_init_guest}, {"sem_destroy", sem_destroy_guest}, {"sem_post", sem_post_guest},
    {"sem_wait", sem_wait_guest}, {"pthread_equal", pthread_equal_guest},
    {"accept", posix_no_network}, {"bind", posix_no_network}, {"connect", posix_no_network},
    {"getpeername", posix_no_network}, {"recv", posix_no_network}, {"send", posix_no_network},
    {"select", select_guest},
    /* sce::Json */
    {"_ZN3sce4Json6StringC1Ev", json_string_ctor}, {"_ZN3sce4Json6StringC1EPKc", json_string_ctor_text},
    {"_ZN3sce4Json6StringD1Ev", json_string_dtor},
    {"_ZNK3sce4Json6String4sizeEv", json_string_size}, {"_ZNK3sce4Json6String5c_strEv", json_string_c_str},
    {"_ZN3sce4Json5ArrayC1Ev", json_object_noop}, {"_ZN3sce4Json5ArrayD1Ev", json_object_noop},
    {"_ZN3sce4Json5Array9push_backERKNS0_5ValueE", json_object_noop2},
    {"_ZN3sce4Json5ValueC1Ev", json_object_noop}, {"_ZN3sce4Json5ValueD1Ev", json_object_noop},
    {"_ZN3sce4Json5ValueC1ERKNS0_5ArrayE", json_object_noop2}, {"_ZN3sce4Json5ValueC1ERKNS0_6StringE", json_object_noop2},
    {"_ZN3sce4Json5Value5clearEv", json_object_noop},
    {"_ZN3sce4Json5Value3setERKNS0_5ArrayE", json_ok3},
    {"_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_", json_ok3},
    {"_ZN3sce4Json5Value9serializeERNS0_6StringE", json_fail1},
    {"_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm", json_parse},
    {"_ZNK3sce4Json5Value10getBooleanEv", json_zero}, {"_ZNK3sce4Json5Value10getIntegerEv", json_zero},
    {"_ZNK3sce4Json5Value5countEv", json_zero}, {"_ZNK3sce4Json5Value7getTypeEv", json_zero},
    {"_ZNK3sce4Json5Value8getValueEm", json_null_ref1}, {"_ZNK3sce4Json5Value9getStringEv", json_empty_string},
    {"_ZNK3sce4Json5ValueixEPKc", json_null_ref1}, {"_ZNK3sce4Json5ValueixEm", json_null_ref1},
    /* NpToolkit2 V2 user profiles */
    {"_ZN3sce7Toolkit2NP2V211UserProfile7Request13GetNpProfilesC1Ev", json_object_noop},
    {"_ZN3sce7Toolkit2NP2V211UserProfile7Request13GetNpProfilesD1Ev", json_object_noop},
    {"_ZN3sce7Toolkit2NP2V24Core8ResponseINS2_11UserProfile10NpProfilesEEC1Ev", json_object_noop},
    {"_ZN3sce7Toolkit2NP2V24Core8ResponseINS2_11UserProfile10NpProfilesEED1Ev", json_object_noop},
    {"_ZN3sce7Toolkit2NP2V24Core12ResponseBase13getReturnCodeEv", np_response_error},
    {"_ZN3sce7Toolkit2NP2V24Core12ResponseBase14getServerErrorEv", np_response_none},
    {"_ZN3sce7Toolkit2NP2V24Core8ResponseINS2_11UserProfile10NpProfilesEE3getEv", np_response_none},
    {"sceNpGetAccountLanguageA", np_signed_out}, {"sceNpPartyCheckCallback", np_ok},
    {"sceNpFriendListDialogInitialize", np_ok}, {"sceNpFriendListDialogTerminate", np_ok},
    {"sceNpFriendListDialogUpdateStatus", dialog_status_none},
    {"sceNpFriendListDialogOpenA", dialog_unavailable}, {"sceNpFriendListDialogGetResultA", dialog_unavailable},
    /* companion app */
    {"sceCompanionHttpdInitialize2", companion_unavailable}, {"sceCompanionHttpdTerminate", np_ok},
    {"sceCompanionHttpdStart", companion_unavailable}, {"sceCompanionHttpdStop", np_ok},
    {"sceCompanionHttpdAddHeader", companion_unavailable}, {"sceCompanionHttpdSetBody", companion_unavailable},
    {"sceCompanionHttpdGetEvent", companion_no_event},
    {"sceCompanionHttpdRegisterRequestCallback2", companion_unavailable},
    {"sceCompanionHttpdUnregisterRequestCallback", np_ok},
    {"sceCompanionUtilGetEvent", companion_no_event},
    /* face analysis */
    {"sceFaceAgeGetWorkingMemorySize", face_working_memory}, {"sceFaceAllPartsGetWorkingMemorySize", face_working_memory},
    {"sceFaceAttributeGetWorkingMemorySize", face_working_memory}, {"sceFaceDetectionGetWorkingMemorySize", face_working_memory},
    {"sceFaceIdentifyGetWorkingMemorySize", face_working_memory}, {"sceFaceIdentifyLiteGetWorkingMemorySize", face_working_memory},
    {"sceFacePartsGetWorkingMemorySize", face_working_memory}, {"sceFaceShapeGetWorkingMemorySize", face_working_memory},
    /* video decoders */
    {"sceVideodecQueryResourceInfo", videodec_unavailable}, {"sceVideodecCreateDecoder", videodec_unavailable},
    {"sceVideodecDecode", videodec_unavailable}, {"sceVideodecDeleteDecoder", np_ok},
    {"sceVideodecFlush", videodec_unavailable}, {"sceVideodecReset", videodec_unavailable},
    {"sceVideodec2QueryComputeMemoryInfo", videodec_unavailable}, {"sceVideodec2QueryDecoderMemoryInfo", videodec_unavailable},
    {"sceVideodec2AllocateComputeQueue", videodec_unavailable}, {"sceVideodec2ReleaseComputeQueue", np_ok},
    {"sceVideodec2CreateDecoder", videodec_unavailable}, {"sceVideodec2DeleteDecoder", np_ok},
    {"sceVideodec2Decode", videodec_unavailable}, {"sceVideodec2Flush", videodec_unavailable},
    {"sceVideodec2Reset", videodec_unavailable}, {"sceVideodec2GetPictureInfo", videodec_unavailable},
    /* PlayGo, system service, AppContent */
    {"scePlayGoGetLanguageMask", playgo_get_language_mask}, {"scePlayGoSetLanguageMask", playgo_set_language_mask},
    {"sceSystemServiceGetDisplaySafeAreaInfo", display_safe_area},
    {"sceAppContentDownloadDataGetAvailableSpaceKb", download_space_kb},
};
uintptr_t runtime_bringup_resolve(const char *name) {
    /* Imports without a known symbol name, matched on NID and library. */
    if (runtime_nid_equals(name, "fJgP+wqifno#libSceDiscMap")) return (uintptr_t)discmap_no_bitmap;
    if (runtime_nid_equals(name, "YlpnKm99h3Q#libSceNpToolkit2")) return (uintptr_t)unnamed_toolkit;
    if (runtime_nid_equals(name, "qiOZEn26Kds#EAWebKit")) return (uintptr_t)unnamed_eawebkit;
    return RUNTIME_LOOKUP(exports, name);
}
