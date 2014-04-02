/**
 * @file   peb_util.c
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief  A collection of misc. utility functions.
 */

#define MODULE UTIL

#include <global.h>

char* peb_stpcpy(char *dest, const char *src){
    while(*src != '\0')
        *dest++ = *src++;
    *dest = '\0';
    return dest;
}

void report_err(){
    EWTF("Called a callback which we shouldn't call back.");
}

void no_op(){
    return;
}

