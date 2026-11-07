#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unistd.h>
#include "tenkaichi/assets.hpp"
namespace tenkaichi::cache {
struct Fd {
    int n=-1;
    explicit Fd(int value=-1):n(value){}
    ~Fd(){if(n>=0)close(n);}
    Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;
};
struct Directory {
    Fd root,lock,base,dir;
    Directory(const std::string& path,const std::string& profile,const char* name="native-assets-v1");
};
[[noreturn]] void fail(const std::string& why);
bool isHash(const std::string& hash);
bool readFile(int dir,const char* name,std::size_t maximum,std::vector<std::uint8_t>& out);
void syncFile(int dir,const char* name,Bytes bytes);
void clearNativeRecordsAtRoot(int root); // Caller holds native-assets.lock.
}
