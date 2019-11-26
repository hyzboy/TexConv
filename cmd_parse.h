#pragma once

#include<string>
#include<vector>

using stringlist=std::vector<std::string>;

/**
* 命令行参数解晰辅助类
*/
class cmd_parse                                                          ///命令行参数解晰辅助类
{
    stringlist args;

public:

    cmd_parse(int argc,char **argv);
    virtual ~cmd_parse()=default;

    int Find(const std::string &)const;                                    ///<查找一个指定字串开头的参数是否存在

    bool GetInteger(const std::string &,int *)const;                       ///<取得一个数值参数
    bool GetString(const std::string &,std::string &)const;                   ///<取得一个字符串参数
};//class CmdParse