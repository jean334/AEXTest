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
#include "Enclave_t.h"
#include <stdio.h>
#include <sgx_trts_aex.h>
#include <sgx_thread.h>
#define SIZE 4096

long long timestamps;
long long aex_count = 0;
long long count = 0;
long long int Countadd[SIZE];
long long int Countadd_intervals[SIZE];
int index = 0;

# define BUFSIZ  8192

typedef struct {
    int isCounting;
    int isWaiting;
    sgx_thread_cond_t startCounting;
    sgx_thread_mutex_t mutex;

}cond_struct_t;

cond_struct_t cond = {0, 0, SGX_THREAD_COND_INITIALIZER, SGX_THREAD_MUTEX_INITIALIZER};

void t_print(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
}

static void my_aex_notify_handler(const sgx_exception_info_t *info, const void * args)
{
    /*
    a custom handler that will be called when an AEX occurs, storing the number of ADD operations (performed in another thread) in a global array. This allows you to 
    know when AEX occurs (the number of ADD operations increases linearly) and how often it occurs.
    */
   (void)info;
   (void)args;
   Countadd[aex_count] = count;
   Countadd_intervals[aex_count] = count - Countadd[aex_count-1];
   aex_count++;
}

void printArray(long long int *arr){
    /*
    Print a array of size SIZE, which contains the number of ADD operations performed before each AEX occurs.
    */
    for(int i = 0; i < aex_count ; i++){
        t_print("Countadd[%d] : %lld\n", i, arr[i]);
    }
    t_print("\n\n");
}

void countADD(void){
    /*
    The function that will be called in another thread to perform ADD operations.
    */
    see_pid("countADD");
    const char* args = NULL; 
    sgx_aex_mitigation_node_t node;
    sgx_register_aex_handler(&node, my_aex_notify_handler, (const void*)args);
    cond_struct_t *c = &cond;
    sgx_thread_mutex_lock(&c->mutex);
    while (!c->isCounting) {
        sgx_thread_cond_wait(&c->startCounting, &c->mutex);
    }
    sgx_thread_mutex_unlock(&c->mutex);
    while(c->isCounting == 1){
        count++; 
    }
    sgx_unregister_aex_handler(my_aex_notify_handler);
}


void main_thread(int sleep_time, int core_id, int set_aff, int sleep_inside_enclave){
    /*
    the main thread that will be called by the application.
    */
    see_pid("main_thread");
    cond_struct_t *c = &cond;
    if (set_aff)
        set_affinity(core_id);

    /*    
    const char* args = NULL; 
    sgx_aex_mitigation_node_t node;
    sgx_register_aex_handler(&node, my_aex_notify_handler, (const void*)args);
    */
    sgx_thread_mutex_lock(&c->mutex);
    c->isCounting = 1;
    sgx_thread_cond_signal(&c->startCounting);
    sgx_thread_mutex_unlock(&c->mutex);

    if(sleep_inside_enclave){
        for (int j = 0; j < 75*sleep_time; j++){
            for(int i = 0; i < 10000000; i++){
                
            }
        }
    }
    else
        ocall_sleep(&sleep_time);
    
    //t_print("c->isCounting : %d\n", c->isCounting);
    sgx_thread_mutex_lock(&c->mutex);
    c->isCounting = 0;
    sgx_thread_mutex_unlock(&c->mutex);

    //sgx_unregister_aex_handler(my_aex_notify_handler);  
    printArray(Countadd);
    printArray(Countadd_intervals);
    t_print("Count : %lld\n", count);
    t_print("Number of AEX : %d\n", aex_count);

}