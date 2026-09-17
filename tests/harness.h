/* The bootstrap test harness.
 *
 * burrow ports Go's testing package and that is what the translated tests will
 * run under, but testing depends on fmt, which depends on reflect, which
 * depends on the type registry, which depends on the allocators. Something has
 * to test the bottom of that stack before any of it exists, so this is a
 * hundred lines of asserts with no dependencies beyond the freestanding headers.
 *
 * It goes away once testing lands, at which point the tests written against it
 * get rewritten once. That is a known cost and it is smaller than the cost of
 * having no tests until the tenth pull request.
 *
 * Copyright 2026 The burrow Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style licence that can be found
 * in the LICENSE file. */

#ifndef BURROW_TESTS_HARNESS_H
#define BURROW_TESTS_HARNESS_H

#include <stdio.h>
#include <string.h>

static int harness_failures;
static int harness_checks;
static const char *harness_current;

#define TEST(name)                                                                     \
    static void test_##name(void);                                                     \
    static void run_##name(void) {                                                     \
        harness_current = #name;                                                       \
        test_##name();                                                                 \
    }                                                                                  \
    static void test_##name(void)

#define RUN(name) run_##name()

/* Reports the file and line of the check rather than of the macro, and keeps
 * going after a failure, because a test run that stops at the first problem
 * tells you about one problem per CI round trip. */
#define CHECK(cond)                                                                    \
    do {                                                                               \
        harness_checks++;                                                              \
        if (!(cond)) {                                                                 \
            harness_failures++;                                                        \
            fprintf(stderr, "%s:%d: %s: check failed: %s\n", __FILE__, __LINE__,       \
                    harness_current, #cond);                                           \
        }                                                                              \
    } while (0)

#define CHECK_STR_EQ(got, want)                                                        \
    do {                                                                               \
        harness_checks++;                                                              \
        const char *g_ = (got), *w_ = (want);                                          \
        if (g_ == NULL || w_ == NULL || strcmp(g_, w_) != 0) {                         \
            harness_failures++;                                                        \
            fprintf(stderr, "%s:%d: %s: got %s, want %s\n", __FILE__, __LINE__,        \
                    harness_current, g_ ? g_ : "(null)", w_ ? w_ : "(null)");          \
        }                                                                              \
    } while (0)

#define CHECK_INT_EQ(got, want)                                                        \
    do {                                                                               \
        harness_checks++;                                                              \
        long long g_ = (long long)(got), w_ = (long long)(want);                       \
        if (g_ != w_) {                                                                \
            harness_failures++;                                                        \
            fprintf(stderr, "%s:%d: %s: got %lld, want %lld\n", __FILE__, __LINE__,    \
                    harness_current, g_, w_);                                          \
        }                                                                              \
    } while (0)

static int harness_report(const char *suite) {
    if (harness_failures == 0) {
        printf("ok\t%s\t%d checks\n", suite, harness_checks);
        return 0;
    }
    printf("FAIL\t%s\t%d of %d checks failed\n", suite, harness_failures,
           harness_checks);
    return 1;
}

#endif /* BURROW_TESTS_HARNESS_H */
