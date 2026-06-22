#include <assert.h>
#include <stdio.h>
#include "runner.h"

static void test_hash_changes_with_content(void)
{
    uint32_t a[4] = {1,2,3,4};
    uint32_t b[4] = {1,2,3,5};
    assert(framebuffer_hash(a, 4) == framebuffer_hash(a, 4)); /* stable */
    assert(framebuffer_hash(a, 4) != framebuffer_hash(b, 4)); /* sensitive */
}

static void test_hang_tracker(void)
{
    hang_tracker_t t;
    hang_tracker_init(&t);
    /* First frame primes; never an immediate hang. */
    assert(!hang_tracker_update(&t, 100, 3));
    /* Identical frames accumulate staleness. */
    assert(!hang_tracker_update(&t, 100, 3)); /* stale=1 */
    assert(!hang_tracker_update(&t, 100, 3)); /* stale=2 */
    assert( hang_tracker_update(&t, 100, 3)); /* stale=3 => hang */
    /* A changed frame resets staleness. */
    assert(!hang_tracker_update(&t, 200, 3));
    assert(!hang_tracker_update(&t, 200, 3));
}

static void test_hang_tracker_disabled(void)
{
    hang_tracker_t t;
    hang_tracker_init(&t);
    for (int i = 0; i < 100; i++) {
        assert(!hang_tracker_update(&t, 42, 0)); /* limit 0 => never hangs */
    }
}

int main(void)
{
    test_hash_changes_with_content();
    test_hang_tracker();
    test_hang_tracker_disabled();
    printf("test_runner: OK\n");
    return 0;
}
