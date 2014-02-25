/**
 * @file   vip_fun.c
 * @author Alex Podolsky <apodolsk@andrew.cmu.edu>
 * 
 * @brief  Selectively supersede logger settings inside of functions whose
 * names are on the vip_list. Logger plugin.
 * 
 */

#include <peb_macros.h>
#include <string.h>

const char *vip_list[] = {
    "alloc",
    "shave",
};

int fun_is_very_important(const char *fun_name){
    for(int i = 0; i < ARR_LEN(vip_list); i++)
        if(strcmp(fun_name, vip_list[i]) == 0)
            return 1;
    return 0;
}
