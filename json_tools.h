#pragma once

#include<json/json.h>
#include<hgl/type/DataType.h>

bool LoadJson(Json::Value &root,const os_char *filename);
bool SaveJson(const Json::Value &root,const os_char *filename);