/**
 * @file   peb_util.c
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief  A collection of misc. utility functions.
 */

#define MODULE UTIL

#include <global.h>

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

/* I use the following functions when I need to fill in a function pointer
   with something generic. */

void report_err(){
    EWTF("Called a callback which we shouldn't call back.");
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
    RARITY("I'm a surprising and rare callback.");
    return 0;
}
