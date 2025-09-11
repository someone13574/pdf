#include "test/test.h"

#ifdef TEST

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "logger/log.h"

#define TEST_TIMEOUT_MS 5000

#ifdef DEBUG_TEST_FUNCTION
#include <string.h>
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#else
#include <stddef.h>
#endif

__attribute__((weak)) extern _TestFuncEntry __start_test_registry[];
__attribute__((weak)) extern _TestFuncEntry __stop_test_registry[];

static void print_line(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) < 0 || w.ws_col == 0) {
        w.ws_col = 80; // fallback
    }

    for (int i = w.ws_col; i-- > 0;) {
        putchar('-');
    }

    putchar('\n');
}

#ifndef DEBUG_TEST_FUNCTION
#include <poll.h>
#include <signal.h>
#include <time.h>

static long elapsed_ms(const struct timespec* start) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    long sec = now.tv_sec - start->tv_sec;
    long nsec = now.tv_nsec - start->tv_nsec;
    return sec * 1000 + nsec / 1000000;
}

#ifdef TEST_NO_CAPTURE
static void
run_forked_test(_TestFuncEntry* test_entry, int* passed, int* failed) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Run test and capture output
        TestResult result = test_entry->func();
        _exit((result == TEST_RESULT_PASS) ? EXIT_SUCCESS : EXIT_FAILURE);
    } else {
        int status;
        bool timed_out = false;
        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);

        while (true) {
            pid_t wait_result = waitpid(pid, &status, WNOHANG);
            if (wait_result == pid) {
                // Child exited
                break;
            } else if (wait_result == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }

            long elapsed_time = elapsed_ms(&start);
            if (elapsed_time >= TEST_TIMEOUT_MS) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                timed_out = true;
                break;
            }

            long remaining_time = TEST_TIMEOUT_MS - elapsed_time;
            long poll_time = remaining_time < elapsed_time
                               ? remaining_time
                               : elapsed_time; // have poll speed start fast
            struct timespec wait_time = {0, poll_time * 1000L * 1000L};
            nanosleep(&wait_time, NULL);
        }

        if (!timed_out && WIFEXITED(status)
            && WEXITSTATUS(status) == EXIT_SUCCESS) {
            LOG_DIAG(
                DEBUG,
                TEST,
                "Test `%s` (\x1b[4m%s:%lu\x1b[0m) passed",
                test_entry->name,
                test_entry->file,
                test_entry->line
            );

            print_line();

            *passed += 1;
        } else {
            print_line();

            if (timed_out) {
                LOG_WARN(
                    TEST,
                    "Test `%s` (\x1b[4m%s:%lu\x1b[0m) exceeded test timeout",
                    test_entry->name,
                    test_entry->file,
                    test_entry->line
                );
            }

            LOG_ERROR(
                TEST,
                "Test `%s` (\x1b[4m%s:%lu\x1b[0m) failed",
                test_entry->name,
                test_entry->file,
                test_entry->line
            );

            print_line();

            *failed += 1;
        }
    }
}
#endif // TEST_NO_CAPTURE

#ifndef TEST_NO_CAPTURE
#include <errno.h>
#include <string.h>

static void
run_forked_test(_TestFuncEntry* test_entry, int* passed, int* failed) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        close(pipefd[0]); // child only write

        // Save stdout for restore
        int saved_stdout = dup(STDOUT_FILENO);
        if (saved_stdout < 0) {
            perror("dup");
            _exit(EXIT_FAILURE);
        }

        // Redirect stdout to pipe
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            perror("dup2");
            _exit(EXIT_FAILURE);
        }
        close(pipefd[1]);

        // Run test and capture output
        TestResult result = test_entry->func();

        fflush(stdout);

        // Restore stdout
        if (dup2(saved_stdout, STDOUT_FILENO) < 0) {
            perror("dup2 restore");
            _exit(EXIT_FAILURE);
        }
        close(saved_stdout);
        _exit((result == TEST_RESULT_PASS) ? EXIT_SUCCESS : EXIT_FAILURE);
    } else {
        close(pipefd[1]); // parent only reads

        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);

        // Buffer for accumulated output
        char* buffer = NULL;
        size_t buffer_size = 0;
        char read_buf[4096];

        struct pollfd poll_fd = {
            .fd = pipefd[0],
            .events = POLLIN | POLLHUP | POLLERR
        };

        bool timed_out = false;
        int status = 0;
        bool child_exited = 0;

        while (1) {
            long elapsed_time = elapsed_ms(&start);
            if (!child_exited && elapsed_time >= TEST_TIMEOUT_MS) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                timed_out = true;
                child_exited = true;
            }

            if (!child_exited) {
                pid_t r = waitpid(pid, &status, WNOHANG);
                if (r == pid) {
                    child_exited = 1;
                } else if (r == -1) {
                    perror("waitpid");
                }
            }

            long remaining_time = TEST_TIMEOUT_MS - elapsed_time;
            long poll_time = remaining_time < elapsed_time
                               ? remaining_time
                               : elapsed_time; // have poll speed start fast

            int poll_return = poll(&poll_fd, 1, (int)poll_time);
            if (poll_return > 0) {
                if (poll_fd.revents & POLLIN) {
                    ssize_t n = read(pipefd[0], read_buf, sizeof(read_buf));
                    if (n > 0) {
                        char* old = buffer;
                        buffer = realloc(buffer, buffer_size + (size_t)n + 1);
                        if (!buffer) {
                            free(old);
                            perror("realloc");
                            exit(EXIT_FAILURE);
                        }
                        memcpy(buffer + buffer_size, read_buf, (size_t)n);
                        buffer_size += (size_t)n;
                        buffer[buffer_size] = '\0';
                    } else if (n == 0) {
                        // EOF from child
                        break;
                    } else if (n < 0 && errno != EINTR && errno != EAGAIN) {
                        perror("read");
                        break;
                    }
                }

                if (poll_fd.revents & (POLLHUP | POLLERR)) {
                    // drain any remaining bytes
                    ssize_t n;
                    while ((n = read(pipefd[0], read_buf, sizeof(read_buf))) > 0
                    ) {
                        char* old = buffer;
                        buffer = realloc(buffer, buffer_size + (size_t)n + 1);
                        if (!buffer) {
                            free(old);
                            perror("realloc");
                            exit(EXIT_FAILURE);
                        }
                        memcpy(buffer + buffer_size, read_buf, (size_t)n);
                        buffer_size += (size_t)n;
                        buffer[buffer_size] = '\0';
                    }
                    break;
                }
            } else if (poll_return < 0) {
                if (errno == EINTR) {
                    continue;
                }

                perror("poll");
                break;
            }

            if (child_exited) {
                ssize_t n = read(pipefd[0], read_buf, sizeof(read_buf));
                if (n > 0) {
                    char* old = buffer;
                    buffer = realloc(buffer, buffer_size + (size_t)n + 1);
                    if (!buffer) {
                        free(old);
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }

                    memcpy(buffer + buffer_size, read_buf, (size_t)n);
                    buffer_size += (size_t)n;
                    buffer[buffer_size] = '\0';
                    continue; // Continue until all data is read
                } else if (n == 0 || (n < 0 && errno == EAGAIN)) {
                    break;
                } else if (n < 0 && errno == EINTR) {
                    continue;
                } else if (n < 0) {
                    perror("read");
                    break;
                }
            }
        }

        close(pipefd[0]);

        if (!child_exited) {
            waitpid(pid, &status, 0);
        }

        if (!timed_out && WIFEXITED(status)
            && WEXITSTATUS(status) == EXIT_SUCCESS) {
            LOG_DIAG(
                DEBUG,
                TEST,
                "Test `%s` (\x1b[4m%s:%lu\x1b[0m) passed",
                test_entry->name,
                test_entry->file,
                test_entry->line
            );

            *passed += 1;
        } else {
            print_line();

            if (buffer_size > 0) {
                if (fwrite(buffer, 1, buffer_size, stdout) != buffer_size) {
                    perror("fwrite");
                    exit(EXIT_FAILURE);
                }
            }

            if (timed_out) {
                LOG_WARN(
                    TEST,
                    "Test `%s` (\x1b[4m%s:%lu\x1b[0m) exceeded test timeout",
                    test_entry->name,
                    test_entry->file,
                    test_entry->line
                );
            }

            LOG_ERROR(
                TEST,
                "Test `%s` (\x1b[4m%s:%lu\x1b[0m) failed",
                test_entry->name,
                test_entry->file,
                test_entry->line
            );

            print_line();

            *failed += 1;
        }

        free(buffer);
    }
}
#endif // not TEST_NO_CAPTURE
#endif // not DEBUG_TEST_FUNCTION

int test_entry(void) {
    _TestFuncEntry* start = __start_test_registry;
    _TestFuncEntry* stop = __stop_test_registry;

#ifdef DEBUG_TEST_FUNCTION
    LOG_DIAG(
        INFO,
        TEST,
        "Running test: `" STRINGIFY(DEBUG_TEST_FUNCTION) "`..."
    );
#else
    ptrdiff_t num_tests = stop - start;
    LOG_DIAG(INFO, TEST, "Running %ld tests...", num_tests);
#endif

    print_line();

    int passed = 0;
    int failed = 0;

    for (_TestFuncEntry* test_entry = start; test_entry < stop; test_entry++) {
#ifdef DEBUG_TEST_FUNCTION
        if (strcmp(test_entry->name, STRINGIFY(DEBUG_TEST_FUNCTION)) != 0) {
            continue;
        }

        TestResult result = test_entry->func();
        if (result == TEST_RESULT_PASS) {
            LOG_DIAG(
                DEBUG,
                TEST,
                "Test `%s` (\x1b[4m%s:%lu\x1b[0m) passed",
                test_entry->name,
                test_entry->file,
                test_entry->line
            );

            passed += 1;
        } else {
            LOG_ERROR(
                TEST,
                "Test `%s` (\x1b[4m%s:%lu\x1b[0m) failed",
                test_entry->name,
                test_entry->file,
                test_entry->line
            );

            failed += 1;
        }
#else  // ends DEBUG_TEST_FUNCTION
        run_forked_test(test_entry, &passed, &failed);
#endif // not DEBUG_TEST_FUNCTION
    }

    print_line();

#ifdef DEBUG_TEST_FUNCTION
    if (passed + failed == 0) {
        LOG_WARN(
            TEST,
            "Test function `" STRINGIFY(DEBUG_TEST_FUNCTION) "` not found"
        );
    } else if (passed + failed > 1) {
        LOG_WARN(
            TEST,
            "Multiple test functions named `" STRINGIFY(DEBUG_TEST_FUNCTION
            ) "` found. Persistent state may persist"
        );
    }
#else  // ends DEBUG_TEST_FUNCTION
    if (failed == 0) {
        LOG_DIAG(
            INFO,
            TEST,
            "Test results: %d/%d passed, %d/%d failed",
            passed,
            passed + failed,
            failed,
            passed + failed
        );
    } else {
        LOG_ERROR(
            TEST,
            "Test results: %d/%d passed, %d/%d failed",
            passed,
            passed + failed,
            failed,
            passed + failed
        );
    }
#endif // not DEBUG_TEST_FUNCTION

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#else // TEST

void translation_unit_not_empty(void) {}

#endif
