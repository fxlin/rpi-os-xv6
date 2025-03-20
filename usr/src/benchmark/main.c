// for microbenchmarking kernel. 2025.02
// original name "benchmark.c"

// syscall number: musl arch/aarch64/bits/syscall.h.in


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

// #define prxxx printf
#define prxxx(...)

// in ms, cf kern/syscall.c
int uptime(void) {
    unsigned long ret = ioctl(0/*does not care*/, 0x1000); 
    // printf("uptime: %lu\n", ret);
    return (int)ret;  // we indeed lose precision here.
}

#define DEFAULT_ITERATIONS 100000
void benchmark_getpid(int iterations) {
    int t0, t1; // ms

    t0 = uptime();
    for (int i = 0; i < iterations; i++) {
        getpid();
    }
    t1 = uptime();

    printf("Total time for %d getpid() calls: %d ms avg %d us \n",
           iterations, t1 - t0, 1000 * (t1 - t0) / iterations);
}

// ----------------- file benchmarks ----------------------- //
// likely b/c of sd.c flaw, for continous reads/writes, there's a sd card error (cf note-benchmarks.txt)
// for this reason, can only test a reduced number of iterations (for smaller read/writes) and cannot test 1MB of read/write

#define TEST_FILE "/d/testfile"
#define FILE_SIZES_COUNT 4
// kernel malloc can do 1MB at most. cf kernel/alloc.c
// Iterations for 4K, 128K, 512K, 1M
const int FILE_SIZES[FILE_SIZES_COUNT] = {4096, 131072, 524288, 0x100000 /*1MB*/};
// const int FILE_ITERATIONS[FILE_SIZES_COUNT] = {10000, 1000, 100, 10};        // sd writes fail at ~4000 4KB writes
const int FILE_ITERATIONS[FILE_SIZES_COUNT] = {1000, 50, 10, 1};

char buffer[2097152]; // 2MB buffer
void benchmark_file() {
    int fd, t0, t1;
    // unlink(TEST_FILE); // Ensure the file does not exist before starting

    for (int i = 0; i < FILE_SIZES_COUNT; i++) {
        long size = FILE_SIZES[i];
        int iterations = FILE_ITERATIONS[i];
        memset(buffer, 'A', size);

        // Write benchmark
#if 1
        unlink(TEST_FILE); // Ensure the file does not exist before starting
        fd = open(TEST_FILE, O_CREAT | O_RDWR);
        if (fd < 0) {
            printf("Failed to open file for writing\n");
            return;
        }
        t0 = uptime();
        for (int j = 0; j < iterations; j++) {
            write(fd, buffer, size);
            lseek(fd, 0, SEEK_SET);
            prxxx("write %d bytes...%d/%d\n", size, j, iterations);
        }
        t1 = uptime();
        close(fd);
        printf("Write %ld bytes (%d iterations): %d ms, Avg Latency: %d us, Throughput: %ld KB/s\n",
            //    size, iterations, t1 - t0, 1000 * (t1 - t0) / iterations, (size * iterations / (t1 - t0 + 1)) / 1024);
                size, iterations, t1 - t0, 1000 * (t1 - t0) / iterations, size * iterations * 1000 / (t1 - t0 + 1) / 1024);

#endif
        // Read benchmark
        fd = open(TEST_FILE, O_RDONLY);
        if (fd < 0) {
            printf("Failed to open file for reading\n");
            return;
        }
        t0 = uptime();
        for (int j = 0; j < iterations; j++) {
            read(fd, buffer, size);
            lseek(fd, 0, SEEK_SET);
            prxxx("read %d bytes...%d/%d\n", size, j, iterations);
        }
        t1 = uptime();
        close(fd);
        printf("Read %ld bytes (%d iterations): %d ms, Avg Latency: %d us, Throughput: %ld KB/s\n",
            //    size, iterations, t1 - t0, 1000 * (t1 - t0) / iterations, (size * iterations / (t1 - t0 + 1)) / 1024);
            size, iterations, t1 - t0, 1000 * (t1 - t0) / iterations, size * iterations * 1000 / (t1 - t0 + 1) / 1024);
    }
    // unlink(TEST_FILE);
}

// ------------- context switch benchmark ------------------- //
// in fact, this measures ipc more than context switch
// idea: ping poing 1 byte, via pipes, between two processes
#define DEFAULT_SWITCH_ITERATIONS 1000

void benchmark_ctx_switch(int iterations) {
    // in our kernel pipe implementation, a pair of pipe fds only has one buffer.
    // therefore, if task1 writes to fd[1] (fd for read) and reads from fd[0] (fd for write),
    // it will read back its own written data (instead of blocking for t2's write). 
    // (this is not the semantics of Linux pipes?) 
    // therefore, we need two pairs of pipe fds
    int p1[2], p2[2]; // Two pipes for bidirectional communication

    int t0, t1;
    char buf = 'x';

    if (pipe(p1) < 0 || pipe(p2) < 0) {
        printf("Failed to create pipes\n");
        return;
    }

    t0 = uptime();
    
    if (fork() == 0) { // Child process
        close(p1[1]); // Close unused write end of p1
        close(p2[0]); // Close unused read end of p2

        prxxx("%s: child %d starts\n", __func__, getpid());
        for (int i = 0; i < iterations; i++) {
            read(p1[0], &buf, 1); // Wait for parent
            prxxx("child read %d/%d\n", i, iterations);
            write(p2[1], &buf, 1); // Respond to parent
            prxxx("child wrote %d/%d\n", i, iterations);
        }
        close(p1[0]); // Close read end of p1
        close(p2[1]); // Close write end of p2        
        prxxx("child done. bye\n");
        exit(0);
    } else { // Parent process
        close(p1[0]); // Close unused read end of p1
        close(p2[1]); // Close unused write end of p2

        prxxx("%s: parent %d starts\n", __func__, getpid());
        for (int i = 0; i < iterations; i++) {
            write(p1[1], &buf, 1); // Signal child
            prxxx("parent wrote %d/%d\n", i, iterations);
            read(p2[0], &buf, 1);  // Wait for child
            prxxx("parent read %d/%d\n", i, iterations);
        }
        close(p1[1]); // Close write end of p1
        close(p2[0]); // Close read end of p2        
        prxxx("parent done.wait...");
        wait(0);
        t1 = uptime();
    }

    printf("Total time for %d context switches: %d ms\n", iterations, t1 - t0);
    printf("Avg context switch latency: %d us\n", (1000 * (t1 - t0)) / (iterations * 2));
}


int main(int argc, char *argv[]) {
    int iterations = DEFAULT_ITERATIONS;

    if (argc > 1) {
        if (strcmp(argv[1], "getpid") == 0) {
            if (argc > 2) {
                iterations = atoi(argv[2]);
                if (iterations <= 0) {
                    printf("Invalid iteration count. Using default: %d\n", DEFAULT_ITERATIONS);
                    iterations = DEFAULT_ITERATIONS;
                }
            }
            benchmark_getpid(iterations);
        } else if (strcmp(argv[1], "file") == 0) {
            benchmark_file();
        } else if (strcmp(argv[1], "ctx") == 0) {
            iterations = DEFAULT_SWITCH_ITERATIONS;
            if (argc > 2) {
                iterations = atoi(argv[2]);
                if (iterations <= 0)                    
                    iterations = DEFAULT_SWITCH_ITERATIONS;
            }
            printf("iterations: %d\n", iterations);
            benchmark_ctx_switch(iterations);
        } else {
            printf("Unknown benchmark: %s\n", argv[1]);
            printf("Available benchmarks: getpid, file, ctx\n");
            return 1;
        }
    } else {
        printf("Usage: %s <benchmark> [iterations]\n", argv[0]);
        return 1;
    }

    return 0;
}
