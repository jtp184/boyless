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

static void test_settle_stabilizes(void)
{
    settle_tracker_t t;
    settle_tracker_init(&t);
    /* target 3 = three consecutive identical frames, counted inclusively. */
    assert(settle_tracker_update(&t, 1, 3, 100) == SETTLE_CONTINUE); /* run = 1 (hash 1) */
    assert(settle_tracker_update(&t, 2, 3, 100) == SETTLE_CONTINUE); /* changed, run = 1 */
    assert(settle_tracker_update(&t, 9, 3, 100) == SETTLE_CONTINUE); /* changed, run = 1 */
    assert(settle_tracker_update(&t, 9, 3, 100) == SETTLE_CONTINUE); /* run = 2 */
    assert(settle_tracker_update(&t, 9, 3, 100) == SETTLE_STABLE);   /* run = 3 */
}

static void test_settle_one_frame(void)
{
    settle_tracker_t t;
    settle_tracker_init(&t);
    /* settle 1 stabilizes on the very first frame: a single frame is,
       inclusively, "unchanged for 1 frame". */
    assert(settle_tracker_update(&t, 42, 1, 100) == SETTLE_STABLE);
}

static void test_settle_times_out(void)
{
    settle_tracker_t t;
    settle_tracker_init(&t);
    settle_status_t s = SETTLE_CONTINUE;
    for (unsigned i = 0; i < 5; i++) {
        s = settle_tracker_update(&t, i, 3, 5); /* hashes always change */
    }
    assert(s == SETTLE_TIMEOUT); /* waited reaches ceiling 5 before stabilizing */
}

static void test_settle_no_ceiling(void)
{
    settle_tracker_t t;
    settle_tracker_init(&t);
    for (unsigned i = 0; i < 50; i++) {
        assert(settle_tracker_update(&t, i, 3, 0) == SETTLE_CONTINUE); /* ceiling 0 never times out */
    }
}

static void test_framebuffer_is_blank(void)
{
    uint32_t uniform[4] = {7, 7, 7, 7};
    uint32_t varied[4]  = {7, 7, 7, 8};
    assert(framebuffer_is_blank(uniform, 4));   /* all identical => blank */
    assert(!framebuffer_is_blank(varied, 4));   /* one differs => not blank */
    assert(framebuffer_is_blank(uniform, 0));   /* empty => blank */
}

int main(void)
{
    test_settle_stabilizes();
    test_settle_one_frame();
    test_settle_times_out();
    test_settle_no_ceiling();
    test_hash_changes_with_content();
    test_hang_tracker();
    test_hang_tracker_disabled();
    test_framebuffer_is_blank();
    printf("test_runner: OK\n");
    return 0;
}
