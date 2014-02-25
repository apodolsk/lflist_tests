/**
 * @file   peb_util.c
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief  A collection of misc. utility functions.
 */

#define MODULE UTIL

#include <global.h>

inline ptrdiff_t ptrdiff(void *a, void *b){
    assert(a >= b);
    return (uintptr_t) a - (uintptr_t) b;
}

/** 
 * @brief Copy the string src to the buffer at dest. Dest must be large
 * enough to hold src, including its null byte.
 *
 * @return A pointer to the terminating null-byte of the newly copied string
 * inside dest.
 */
char* peb_stpcpy(char *dest, const char *src){
    while(*src != '\0')
        *dest++ = *src++;
    *dest = '\0';
    return dest;
}


char *itobs_8(int num, itobsbuf8_t *bin){
    trace2(num, d, bin, p);
    for(int i = 7; i >= 0; i--, num /= 2)
        (*bin)[i] = (num & 1) ? '1' : '0';
    (*bin)[8] = '\0';                 
    return *bin;
}    

char *itobs_16(int num, itobsbuf16_t *bin){
    trace2(num, d, bin, p);
    for(int i = 15; i >= 0; i--, num /= 2)
        (*bin)[i] = (num & 1) ? '1' : '0';
    (*bin)[16] = '\0';                 
    return *bin;
}    

char *itobs_32(int num, itobsbuf32_t *bin){
    trace2(num, d, bin, p);
    for(int i = 31; i >= 0; i--, num /= 2)
        (*bin)[i] = (num & 1) ? '1' : '0';
    (*bin)[32] = '\0';                 
    return *bin;
}    

/* I use the following functions when I need to fill in a function pointer
   with something generic. */

void report_err(){
    LOGIC_ERROR("Called a callback which we shouldn't call back.");
}

void no_op(){
    /* Poor no_op, with his deformed ear. I can understand why he looks so
       alarmed, what with all the people out to steal his lips and eyeball. */
    return;
}

int return_neg(){
    return -1;
}

void *return_null(){
    return NULL;
}

int return_zero(){
    return 0;
}

int return_zero_rare_event(){
    RARE_EVENT("I'm a surprising and rare callback.");
    return 0;
}
