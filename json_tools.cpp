#include<string>
#include<sstream>
#include<iostream>
#include<json/json.h>
#include<hgl/filesystem/FileSystem.h>

using namespace std;

const std::string JsonToString(const Json::Value &jv_root)
{
    Json::StreamWriterBuilder builder;
    Json::StreamWriter *writer=builder.newStreamWriter();

    JSONCPP_OSTRINGSTREAM result;

    writer->write(jv_root,&result);

    delete writer;

    return std::string(result.str());
}

bool ParseJson(Json::Value &root,const char *txt,const int size,std::string *error_info)
{
    Json::CharReaderBuilder builder;
    Json::CharReader *reader=builder.newCharReader();

    const bool result=reader->parse(txt,txt+size,&root,error_info);

    delete reader;

    return result;
}

bool LoadJson(Json::Value &root,const os_char *filename)
{
    char *txt;
    int size;

    size=hgl::filesystem::LoadFileToMemory(filename,(void **)&txt);

    if(size<=0)
    {
        std::cerr<<"load json file failed,filename: "<<filename<<std::endl;
        return(false);
    }

    bool result;

    std::string error_info;

    result=ParseJson(root,txt,size,&error_info);
    delete[] txt;

    if(!result)
    {
        std::cerr<<"parse json file failed,filename: "<<filename<<std::endl;
        return(false);
    }

    return(true);
}

bool SaveJson(const Json::Value &root,const os_char *filename)
{
    const std::string txt=JsonToString(root);

    return hgl::filesystem::SaveMemoryToFile(filename,txt.c_str(),txt.size());
}
