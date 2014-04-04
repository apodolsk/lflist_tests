#include <peb_util.h>
#include <string.h>

__thread int mute_flag;
__thread uint tid_;

const char *vip_list[] = {
    "casx", "casx_ok"
};

bool fun_is_vip(const char *fun_name){
    for(uint i = 0; i < ARR_LEN(vip_list); i++)
        if(strcmp(fun_name, vip_list[i]) == 0)
            return 1;
    return 0;
}
