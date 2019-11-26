#include"config.h"
#include"json_tools.h"
#include<iostream>
#include<hgl/log/LogInfo.h>
#include<hgl/type/BaseString.h>

using namespace hgl;

namespace
{
    static FormatConfig fc[FormatType::RANGE_SIZE];
}//namespace

bool LoadConfig(const OSString &filename)
{
    Json::Value js;
    
    if(!LoadJson(js,filename))
    {
        LOG_ERROR(OS_TEXT("load config file<")+filename+OS_TEXT("> failed."));
        return(false);
    }


}
