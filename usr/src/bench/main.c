// for microbenchmarking kernel. 2025.02
// original name "benchmark.c"

// syscall number: musl arch/aarch64/bits/syscall.h.in


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/uio.h> // For writev()

#define _GNU_SOURCE
#include <sched.h> // clone

// #define prxxx printf
#define prxxx(...)

// in ms, cf kern/syscall.c
int uptime(void) {    
    unsigned long ret = ioctl(0/*does not care*/, 0x1000); 
    // printf("uptime: %lu\n", ret);
    return (int)ret;  // we indeed lose precision here.
}

#define uptime_ms uptime

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

// #define TEST_FILE "/d/testfile"
#define TEST_FILE "/testfile"
#define FILE_SIZES_COUNT 5
// kernel malloc can do 1MB at most. cf kernel/alloc.c
// Iterations for 4K, 64KB, 512K, 1M
// 128KB file  seems supported (xv6
const int FILE_SIZES[FILE_SIZES_COUNT] = {4096, 16 *1024, 131072, 524288, 0x100000 /*1MB*/};
const int FILE_ITERATIONS[FILE_SIZES_COUNT] = {1000, 100, 50, 10, 1};

char buffer[2097152]; // 2MB buffer

// unimplemented: unlink, write(writev instead), lseek. have to find workaround
void benchmark_file() {
    int fd, t0, t1;
    // unlink(TEST_FILE); // Ensure the file does not exist before starting

    for (int i = 0; i < FILE_SIZES_COUNT; i++) {
        long size = FILE_SIZES[i];
        int iterations = FILE_ITERATIONS[i];
        memset(buffer, 'A', size);

        ////////////////////////////////////////////////////
        // Write benchmark        
#if 0
        // unlink(TEST_FILE); // Ensure the file does not exist before starting
        
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
        struct iovec iov[1];
        ssize_t bytes_written;

        t0 = uptime();
        // Perform writev() in a loop
        for (int j = 0; j < iterations; j++) {
            // Open the file (this resets the file pointer to the beginning)
            fd = open(TEST_FILE, O_CREAT | O_RDWR, 0644);
            if (fd < 0) {
                printf("Failed to open file for writing\n");
                return;
            }
            // Prepare the iovec structure
            iov[0].iov_base = (void *)buffer;
            iov[0].iov_len = size;
            // Write the buffer
            bytes_written = writev(fd, iov, 1);
            if (bytes_written < 0) {
                printf("writev failed at iteration %d\n", j);
                close(fd);
                return;
            }
            prxxx("writev %ld bytes...%d/%d\n", size, j, iterations);
            close(fd);
        }
        t1 = uptime();

        printf("Write %ld bytes (%d iterations): %d ms, Avg Latency: %d us, Throughput: %ld KB/s\n",
               (long)size, iterations, t1 - t0, 1000 * (t1 - t0) / iterations,
               (long)size * iterations * 1000 / (t1 - t0 + 1) / 1024);

        ////////////////////////////////////////////////////
        // Read benchmark        
        t0 = uptime();
        for (int j = 0; j < iterations; j++) {
            // Open the file (this resets the file pointer to the beginning)
            fd = open(TEST_FILE, O_RDONLY);
            if (fd < 0) {
                printf("Failed to open file for reading\n");
                return;
            }            
            ssize_t bytes_read = read(fd, buffer, size);
            if (bytes_read < 0) {
                printf("read failed at iteration %d\n", j);
                close(fd);
                return;
            }            
            // lseek(fd, 0, SEEK_SET);
            prxxx("read %ld bytes...%d/%d\n", size, j, iterations);
            close(fd);
        }
        t1 = uptime();
        // close(fd);
        printf("Read %ld bytes (%d iterations): %d ms, Avg Latency: %d us, Throughput: %ld KB/s\n",
            //    size, iterations, t1 - t0, 1000 * (t1 - t0) / iterations, (size * iterations / (t1 - t0 + 1)) / 1024);
            size, iterations, t1 - t0, 1000 * (t1 - t0) / iterations, size * iterations * 1000 / (t1 - t0 + 1) / 1024);
    }
    // unlink(TEST_FILE); //
}

// ------------- context switch benchmark ------------------- //
// in fact, this measures ipc more than context switch
// idea: ping poing 1 byte, via pipes, between two processes
#define DEFAULT_SWITCH_ITERATIONS 1000

// uniplemented: write
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

// from usertests.c
// on uva os, sometimes trigger a kernel panic -- TBD 
#define N_CHILD 50
unsigned char child_stk[N_CHILD][1024]; 
void forktest(char *s)
{

  int n, pid;
  int t0, t1;
  t0 = uptime_ms();

  for(n=0; n<N_CHILD; n++){
    pid = fork();
    // pid = clone(0 /* dont care*/, child_stk[n] + 1024, SIGCHLD, 0);
    if(pid < 0)
      break;  // this tests if kernel handles fork() failure right (e.g. free sources? locks?)
    if(pid == 0)
      exit(0);
    printf("forked once %d\n", n);
  }

  if (n == 0) {
    printf("%s: no fork at all!\n", s);
    exit(1);
  }

//   if(n == N_CHILD){
//     printf("%s: fork claimed to work 1000 times!\n", s);
//     exit(1);
//   }

  t1 = uptime_ms();
  printf("Total time for %d fork()s: %d ms\n", n, t1 - t0);
  printf("Avg fork() latency: %d us\n", (1000 * (t1 - t0)) / n);  
  
  for(; n > 0; n--){
    if(wait(0) < 0){
      printf("%s: wait stopped early\n", s);
      exit(1);
    }
  }

  if(wait(0) != -1){
    printf("%s: wait got too many\n", s);
    exit(1);
  }
  printf("%s: all done\n", s);  
}

// based on countfree() in usertests.c
// NOT working for xv6-rpi3, b/c musl does not have sbrk
// cf sbrk.c in musl
void test_sbrk() {
    enum{ N = 500 };
    int t0, t1;
    printf("wont work. cf sbrk.c in musl, which is only for returning the current brk\n");

    t0 = uptime_ms();
    for (int i = 0; i < N; i++) {
        unsigned long a = (unsigned long)sbrk(4096);
        // unsigned long a = (unsigned long)brk(4096); // not calling to kernel?
        if(a == 0xffffffffffffffff) { // failed to alloc, quit
            break; 
        }
        // modify the memory to make sure it's really allocated.
        *(char *)(a + 4096 - 1) = 1;
    }
    t1 = uptime_ms();
    printf("Total time for %d sbrk()s: %d ms\n", N, t1 - t0);
    printf("Avg sbrk() latency: %d us\n", (1000 * (t1 - t0)) / N);
}

// below is a updated version of sbrk.c in musl, which simply fails (maybe malloc() jst
// uses mmap instead). we fixed that by using the syscall brk 
// (so that we dont have to change musl and commit it)
#include <unistd.h>
#include <errno.h>
#include "syscall.h"
int mybrk(void *end) {
    if ((void *)syscall(SYS_brk, end) == end) return 0;
	return -1;
}

// fxl: 
// cf sbrk.c in musl, which is only for returning the current brk when inc==0
// so first get the current brk using sbrk(), then calls brk() to set the new brk
// sys_brk returns oldsz if failed, newsz if succeeded (diff from "man brk")
void test_brk() {
    enum { N = 500 };
    int t0, t1;
    void *initial_brk = sbrk(0); // Get the current program break
    void *current_brk = initial_brk;

    printf("Initial program break: %p\n", initial_brk); // ok

    t0 = uptime_ms();
    int i;
    for (i = 0; i < N; i++) {
        current_brk += 4096;
        // if (brk(current_brk) != (int)current_brk) { // Failed to set new program break
        //     break;
        // }
        if (mybrk(current_brk)!=0) {printf("brk failed\n"); break;} 
        // Modify the memory to make sure it's really allocated.
        *(char *)(current_brk - 1) = 1;
    }
    t1 = uptime_ms();

    printf("Total time for %d brk()s: %d ms\n", i, t1 - t0);
    printf("Avg brk() latency: %d us\n", (1000 * (t1 - t0)) / i);
}

#define N 10 
// static buf
void test_memset0() {
    enum{ MB = 4 };
    int t0, t1;
    static char buf[MB * 1024 * 1024]; // Static buffer
    t0 = uptime_ms();
    for (int i = 0; i < N; i++) // 10 times
        memset(buf, i, MB * 1024 * 1024);
    t1 = uptime_ms();
    printf("Total time for memset(1) %d MB: %d ms\n", N * MB, t1 - t0);
    printf("Avg memset() latency: %d us per MB\n", 1000 * (t1 - t0) / MB / N);
}


// malloc, will call mmap(), which is unimplemented
void test_memset1() {
    enum{ MB = 4 };
    int t0, t1;
    char *buf = malloc(MB * 1024  * 1024); assert(buf);
    t0 = uptime_ms();
    for (int i = 0; i < N; i++) // 10 times
        memset(buf, i, MB * 1024  * 1024);
    t1 = uptime_ms();
    printf("Total time for memset(1) %d MB: %d ms\n", N*MB, t1 - t0);
    printf("Avg memset() latency: %d us per MB\n", 1000*(t1 - t0) /MB/N);
    free(buf);     
}

// use sbrk ... musl directly returns -1?? (not getting into kernel) why
// in kernel, it's brk, not sbrk. but even brk is unimplemented.
void test_memset() {
    enum{ MB = 4 };
    int t0, t1;    
    char *buf = sbrk(MB * 1024 * 1024); 
    printf("wont work b/c musl's sbrk does not work. cf its source"); 
    if(buf == (void *)-1) {
        printf("sbrk failed\n");
        return;
    }
    t0 = uptime_ms();
    for (int i = 0; i < N; i++) // 10 times
        memset(buf, i, MB * 1024 * 1024);
    t1 = uptime_ms();
    printf("Total time for memset(1) %d MB: %d ms\n", N*MB, t1 - t0);
    printf("Avg memset() latency: %d us per MB\n", 1000*(t1 - t0) /MB/N);
}

int compare_uint32(const void *a, const void *b) {
    uint32_t val1 = *(const uint32_t *)a;
    uint32_t val2 = *(const uint32_t *)b;
    return (val1 > val2) - (val1 < val2);
}
void test_qsort() {
    // enum { NUM_ELEMENTS = 1000*1000 }; // 10k 
    enum { NUM_ELEMENTS = 1000 * 100};
    int t0, t1;
    // uint32_t *array = malloc(NUM_ELEMENTS * sizeof(uint32_t));
    // if (!array) {
    //     printf("Memory allocation failed");
    //     return;
    // }
    // static buffer. 
    static uint32_t array[NUM_ELEMENTS];

    // Fill array with random values
    for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
        array[i] = rand(); //newlib has this?
    }

    t0 = uptime_ms();
    qsort(array, NUM_ELEMENTS, sizeof(uint32_t), compare_uint32);
    t1 = uptime_ms();
    printf("Total time for qsort() %d elements: %d ms\n", NUM_ELEMENTS, t1 - t0);
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
        } else if (strcmp(argv[1], "fork") == 0) {
            forktest("forktest");
            return 1; 
        } else if (strcmp(argv[1], "sbrk") == 0) {
            test_sbrk();
            return 1;         
        } else if (strcmp(argv[1], "brk") == 0) {
            test_brk();
            return 1;        
        } else if (strcmp(argv[1], "memset") == 0) {
            test_memset0();  // static buf
            return 1;        
        } else if (strcmp(argv[1], "qsort") == 0) {
            test_qsort();
            return 1;
        } else {
            printf("Unknown benchmark: %s\n", argv[1]);
            printf("Available benchmarks: getpid, file, ctx, fork, sbrk, brk, memset, qsort\n");
            return 1;
        }
    } else {
        printf("Usage: %s <benchmark> [iterations]\n", argv[0]);
        return 1;
    }

    return 0;
}
