#include"cmd_parse.h"

cmd_parse::cmd_parse(int argc,char **argv)
{
    for(int i=0;i<argc;i++)
        args.push_back(std::string(argv[i]));
}

int cmd_parse::Find(const std::string &flag)const
{
    const int count=(const int)args.size();

    for(int i=0;i<count;i++)
        if(args[i]==flag)
            return i;

    return -1;
}

bool cmd_parse::GetInteger(const std::string &flag,int *result)const
{
    int index=Find(flag);

    if(index==-1)return(false);

    const std::string &str=args[index];

    if(str.length()>flag.length())
    {
        *result=atoi(str.c_str()+flag.length());
    }
    else
    {
        *result=atoi(str.c_str());
    }

    return(true);
}

bool cmd_parse::GetString(const std::string &flag,std::string &result)const
{
    int index=Find(flag);
    const char *p=nullptr;

    if(index==-1)return(false);

    const std::string &str=args[index];

    if(str.length()>flag.length())
    {
        p=str.c_str()+flag.length();
    }
    else
    {
        p=args[index+1].c_str();
    }

    if(*p=='"')    //有引号
    {
        auto len=strchr(p+1,L'"')-p;

        if(len>1)
            result.append(p+1,len-1);
        else
            result=p+1;
    }
    else            //无引号
    {
        result=p;
    }

    return(true);
}
