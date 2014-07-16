bool mute_log = 0;

const char *vips[] = {
    "casx", "casx_ok"
};

bool fun_is_vip(const char *fun_name){
    for(uint i = 0; i < ARR_LEN(vips); i++)
        if(strcmp(fun_name, vips[i]) == 0)
            return 1;
    return 0;
}
