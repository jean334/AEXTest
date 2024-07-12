/*
 * Copyright (C) 2011-2021 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX

#include <sgx_urts.h>
#include <sgx_uswitchless.h>
#include "App.h"
#include "Enclave_u.h"

#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include <unistd.h>  // For sleep
#include <stdio.h>   // For printf
#include <stdlib.h>
#include <threads.h>
#include <thread>

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

#define REPEATS 10//0000

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {
        SGX_ERROR_UNEXPECTED,
        "Unexpected error occurred.",
        NULL
    },
    {
        SGX_ERROR_INVALID_PARAMETER,
        "Invalid parameter.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_MEMORY,
        "Out of memory.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_LOST,
        "Power transition occurred.",
        "Please refer to the sample \"PowerTransition\" for details."
    },
    {
        SGX_ERROR_INVALID_ENCLAVE,
        "Invalid enclave image.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ENCLAVE_ID,
        "Invalid enclave identification.",
        NULL
    },
    {
        SGX_ERROR_INVALID_SIGNATURE,
        "Invalid enclave signature.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_EPC,
        "Out of EPC memory.",
        NULL
    },
    {
        SGX_ERROR_NO_DEVICE,
        "Invalid SGX device.",
        "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
    },
    {
        SGX_ERROR_MEMORY_MAP_CONFLICT,
        "Memory map conflicted.",
        NULL
    },
    {
        SGX_ERROR_INVALID_METADATA,
        "Invalid enclave metadata.",
        NULL
    },
    {
        SGX_ERROR_DEVICE_BUSY,
        "SGX device was busy.",
        NULL
    },
    {
        SGX_ERROR_INVALID_VERSION,
        "Enclave version was invalid.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ATTRIBUTE,
        "Enclave was not authorized.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_FILE_ACCESS,
        "Can't open enclave file.",
        NULL
    },
    {
        SGX_ERROR_MEMORY_MAP_FAILURE,
        "Failed to reserve memory for the enclave.",
        NULL
    },
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret)
{
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }

    if (idx == ttl)
        printf("Error: Unexpected error occurred.\n");
}

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(const sgx_uswitchless_config_t* us_config)
{
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;

    /* Call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */

    const void* enclave_ex_p[32] = { 0 };

    enclave_ex_p[SGX_CREATE_ENCLAVE_EX_SWITCHLESS_BIT_IDX] = (const void*)us_config;

    ret = sgx_create_enclave_ex(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL, SGX_CREATE_ENCLAVE_EX_SWITCHLESS, enclave_ex_p);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }

    return 0;
}

int ocall_close(int fd)
{
    return close(fd);
}

void ocall_print_string(const char *str)
{
    printf("%s", str);
}


void readTSC(long long* ts) {
    /*
    Read the TSC register
    */
    #if defined(_MSC_VER) // MSVC specific
        *ts = __rdtsc();
    #elif defined(__GNUC__) || defined(__clang__) // GCC or Clang specific
        unsigned int lo, hi;
        __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
        *ts = ((uint64_t)hi << 32) | lo;
    #else
    #error "Compiler not supported"
    #endif
}

void set_affinity(int core_id) {
    /*
    set the affinity of the current thread to the core_id
    */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    // Get the current thread (which is the main thread in this case)
    pid_t pid = getpid();
    //printf("pid: %d\n", pid);
    
    int result = sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        std::cerr << "Error setting thread affinity: " << strerror(result) << std::endl;
    } else {
        std::cout << "Thread affinity set to CPU " << core_id << std::endl;
    }
    
}

void see_pid(const char* str){
    /*
    Print the pid of the current thread
    */
    pid_t pid = getpid();
    printf("pid test %s: %d\n",str, pid);

}

void ocall_sleep(int* sec) {
    /*
    Sleep for sec seconds outside the enclave
    */
    printf("Sleeping for %d seconds outside the enclave...\n", *sec);
    sleep(*sec);
    printf("Done sleeping outside the enclave\n");
}

void add_thread(void)
{
    /*
    the function that enters the enclave to perform ADD operations.
    */
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    ret = countADD(global_eid);
    if (ret != SGX_SUCCESS)
        abort();
}

void ecall_main_thread(int sleep_time, int core_id, int set_aff, int sleep_inside_enclave)
{
    /*
    the function that enters the enclave to lanuch the main thread.
    */
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    ret = main_thread(global_eid, sleep_time, core_id, set_aff, sleep_inside_enclave);
    if (ret != SGX_SUCCESS)
        abort();
}

void start_threads(int sleep_time, int core_id, int set_aff, int sleep_inside_enclave)
{
    /*
    intialize the threads and start them.
    */
    printf("Info: Starting both threads...  \n");
    std::thread calib(ecall_main_thread, sleep_time, core_id, set_aff, sleep_inside_enclave);
    std::thread add(add_thread);

    calib.join();
    add.join();

}


/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    if(argc < 5)
    {
        printf("Usage: %s <sleep_time> <core_id> <set_affinity><sleep_inside_enclave>\n", argv[0]);
        return -1;
    }
    int sleep_time = atoi(argv[1]);
    int core_id = atoi(argv[2]);
    int set_aff = atoi(argv[3]);
    int sleep_inside_enclave = atoi(argv[4]);
    printf(" ==== Chosen configuration ====\n sleep_time: %d, core_id: %d, set_affinity: %d, sleep_inside_enclave: %d\n", sleep_time, core_id, set_aff, sleep_inside_enclave);
    /* Configuration for Switchless SGX */
    sgx_uswitchless_config_t us_config = SGX_USWITCHLESS_CONFIG_INITIALIZER;
    us_config.num_uworkers = 2;
    us_config.num_tworkers = 2;

    /* Initialize the enclave */
    if(initialize_enclave(&us_config) < 0)
    {
        printf("Error: enclave initialization failed\n");
        return -1;
    }

    start_threads(sleep_time, core_id, set_aff, sleep_inside_enclave);
    //main_thread(global_eid);

    sgx_destroy_enclave(global_eid);
    return 0;
}
