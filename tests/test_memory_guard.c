/* madden19: no-access guards over reserved / unmapped space (sceKernelBatchMap op 2, prot 0).
 * Standalone: gcc -std=gnu11 -pthread -I. -Isrc tests/test_memory_guard.c src/runtime_memory.c
 *             src/platform/platform_posix.c -o out/memory-guard-test && out/memory-guard-test */
#include "runtime.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
/* Stand-ins for the three runtime functions the memory module uses. */
static int32_t err; int32_t *runtime_errno(void) { return &err; }
void runtime_guest_call_sites(uint64_t out[3]) { out[0]=out[1]=out[2]=0; }
uintptr_t runtime_lookup(const RuntimeExport *t, size_t n, const char *name) {
    for (size_t i=0;i<n;++i) if (!strcmp(t[i].name,name)) return (uintptr_t)t[i].function; return 0; }
uintptr_t runtime_memory_resolve(const char *name);
typedef struct { void *start; uint64_t offset, length; int8_t prot, type; int16_t reserved; int32_t operation; } BatchEntry;
typedef int32_t (ABI *Reserve)(void **, uint64_t, int, uint64_t);
typedef int32_t (ABI *Batch)(BatchEntry *, int, int *);
typedef int32_t (ABI *Alloc)(int64_t, int64_t, uint64_t, uint64_t, int, int64_t *);
typedef int32_t (ABI *Query)(void *, void **, void **, uint32_t *);
static sigjmp_buf jump; static void on_fault(int s) { (void)s; siglongjmp(jump, 1); }
static int readable(volatile char *p) { if (sigsetjmp(jump, 1)) return 0; (void)*p; return 1; }
static int failures;
#define CHECK(c, what) do { if (c) printf("ok   %s\n", what); else { printf("FAIL %s\n", what); ++failures; } } while (0)
int main(void) {
    signal(SIGSEGV, on_fault); signal(SIGBUS, on_fault);
    Reserve reserve = (Reserve)runtime_memory_resolve("sceKernelReserveVirtualRange");
    Batch batch = (Batch)runtime_memory_resolve("sceKernelBatchMap");
    Alloc alloc = (Alloc)runtime_memory_resolve("sceKernelAllocateDirectMemory");
    Query query = (Query)runtime_memory_resolve("sceKernelQueryMemoryProtection");
    const uint64_t M2 = 0x200000;
    int64_t phys = 0;
    CHECK(alloc(0, 0x100000000ll, 2*M2, M2, 0, &phys) == 0, "allocate 4 MiB of direct memory");
    void *base = (void *)(uintptr_t)0x1040000000ull;
    CHECK(reserve(&base, 4*M2, 0x10 /* fixed */, 0) == 0, "reserve 8 MiB at 0x1040000000");
    BatchEntry map = {base, (uint64_t)phys, M2, 3, 0, 0, 0};
    int done = 0;
    CHECK(batch(&map, 1, &done) == 0 && done == 1, "BatchMap op 0: map 2 MiB at the start of the reservation");
    volatile char *mapped = (volatile char *)base;
    mapped[0] = 42;
    CHECK(readable(mapped), "mapped memory is readable before the guard");
    /* Madden 19's call: no access over a range that is mapped, reserved and beyond the reservation. */
    BatchEntry guard = {base, 0x7ff7107ab7d6ull, 6*M2, 0, 20, 0, 2};
    CHECK(batch(&guard, 1, &done) == 0 && done == 1, "BatchMap op 2 prot 0 over mapped + reserved + unmapped -> success");
    CHECK(!readable(mapped), "the mapped piece is now no-access");
    void *s, *e; uint32_t prot = 99;
    CHECK(query(base, &s, &e, &prot) == 0 && prot == 0, "QueryMemoryProtection reports prot 0 on the mapped piece");
    BatchEntry unmapped = {(void *)(uintptr_t)0x1100000000ull, 0, M2, 0, 0, 0, 2};
    CHECK(batch(&unmapped, 1, &done) == 0, "prot 0 over a wholly unmapped range -> success");
    BatchEntry grant = {(char *)base + 2*M2, 0, M2, 3, 0, 0, 2};
    CHECK(batch(&grant, 1, &done) != 0, "prot 3 over reserved-only space -> still an error");
    BatchEntry back = {base, 0, M2, 3, 0, 0, 2};
    CHECK(batch(&back, 1, &done) == 0 && readable(mapped) && mapped[0] == 42, "prot 3 back on the mapped piece -> readable, data kept");
    printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
    return failures != 0;
}
