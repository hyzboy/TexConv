#pragma once
#include<hgl/util/cmd/CmdParse.h>
#include"ImageConvertConfig.h"

using namespace hgl;
using namespace hgl::util;

const PixelFormat *ParseParamFormat(const CmdParse &cmd,const os_char *flag,const PixelFormat *default_format);
void ParseParamFormat(ImageConvertConfig *icc,const CmdParse &cmd);
void ParseParamColorKey(ImageConvertConfig *icc,const CmdParse &cmd);