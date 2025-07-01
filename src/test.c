#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"

__attribute__((weak)) extern __TestFunctionEntry __start_test_registry[];
__attribute__((weak)) extern __TestFunctionEntry __stop_test_registry[];

void print_line() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) < 0 || w.ws_col == 0) {
        w.ws_col = 80; // fallback
    }

    for (int i = w.ws_col; i-- > 0;) {
        putchar('-');
    }

    putchar('\n');
}

int main(void) {
    __TestFunctionEntry* start = __start_test_registry;
    __TestFunctionEntry* stop = __stop_test_registry;

    LOG_INFO_G("test", "Running tests...");
    print_line();

    int successful = 0;
    int failed = 0;

    for (__TestFunctionEntry* entry = start; entry < stop; entry++) {
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
            close(pipefd[0]); // we only write

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
            TestResult result = entry->function();
            fflush(stdout);

            // Restore stdout
            if (dup2(saved_stdout, STDOUT_FILENO) < 0) {
                perror("dup2 restore");
                _exit(EXIT_FAILURE);
            }
            close(saved_stdout);

            // Print success/failure
            if (result.error_file) {
                print_line();
                LOG_ERROR_G(
                    "test",
                    "Test `%s` failed at \x1b[4m%s:%d\x1b[0m",
                    entry->name,
                    result.error_file,
                    result.error_line
                );
                _exit(EXIT_FAILURE);
            } else {
                LOG_DEBUG_G(
                    "test",
                    "Test `%s` (\x1b[4m%s:%d\x1b[0m) succeeded",
                    entry->name,
                    entry->file,
                    entry->line
                );
                _exit(EXIT_SUCCESS);
            }
        } else {
            close(pipefd[1]); // we only read

            // Save stdout into buffer
            char* buffer = NULL;
            size_t buffer_size = 0;
            ssize_t bytes_read;
            char read_buf[4096];

            while ((bytes_read = read(pipefd[0], read_buf, sizeof(read_buf)))
                   > 0) {
                // Grow buffer
                char* old_buffer = buffer;
                buffer = realloc(buffer, buffer_size + (size_t)bytes_read + 1);
                if (!buffer) {
                    free(old_buffer);
                    perror("realloc");
                    exit(EXIT_FAILURE);
                }

                // Copy new bytes to buffer
                memcpy(buffer + buffer_size, read_buf, (size_t)bytes_read);
                buffer_size += (size_t)bytes_read;
                buffer[buffer_size] = '\0';
            }
            close(pipefd[0]);

            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
                successful++;
            } else {
                failed++;
                if (buffer_size > 0) {
                    if (fwrite(buffer, 1, buffer_size, stdout) != buffer_size) {
                        perror("fwrite");
                        exit(EXIT_FAILURE);
                    }
                    print_line();
                }
            }

            free(buffer);
        }
    }

    print_line();
    LOG_INFO_G(
        "test",
        "Test results: %d/%d succeeded, %d/%d failed",
        successful,
        successful + failed,
        failed,
        successful + failed
    );

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
