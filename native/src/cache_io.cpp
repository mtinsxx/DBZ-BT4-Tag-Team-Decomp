#include "cache_io.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
namespace tenkaichi::cache {
[[noreturn]] void fail(const std::string& why){throw std::runtime_error(why+": "+std::strerror(errno));}
    Directory::Directory(const std::string& path,const std::string& profile,const char* name):
        root(open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW)),
        lock(root.n<0?-1:openat(root.n,"native-assets.lock",O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600)) {
        if(root.n<0||lock.n<0)fail("Cache lock open");
        if(flock(lock.n,LOCK_EX|LOCK_NB))fail("Cache busy (preparation/viewer/cleanup)");
        if(mkdirat(root.n,name,0700)&&errno!=EEXIST)fail("Cache directory create");
        base.n=openat(root.n,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
        if(base.n<0)fail("Cache directory open");
        if(profile!="ENG"&&profile!="ESP")throw std::runtime_error("Cache requires verified ENG/ESP profile");
        if(mkdirat(base.n,profile.c_str(),0700)&&errno!=EEXIST)fail("Cache profile create");
        dir.n=openat(base.n,profile.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
        if(dir.n<0)fail("Cache profile open");
        if(fsync(base.n)||fsync(root.n))fail("Cache directory sync");
    }
bool isHash(const std::string& hash){return hash.size()==64&&hash.find_first_not_of("0123456789abcdef")==std::string::npos;}
bool readFile(int dir,const char* name,std::size_t maximum,std::vector<std::uint8_t>& out){
    out.clear();Fd file(openat(dir,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW));struct stat st{};
    if(file.n<0||fstat(file.n,&st)||!S_ISREG(st.st_mode)||st.st_size<0||std::uint64_t(st.st_size)>maximum)return false;
    out.resize(std::size_t(st.st_size));std::size_t at=0;
    while(at<out.size()) {auto n=read(file.n,out.data()+at,out.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){out.clear();return false;}at+=std::size_t(n);}
    return true;
}
void syncFile(int dir,const char* name,Bytes bytes){
    const std::string temporary=std::string(name)+".tmp";
    // Remove only our fixed temporary name; never follow a stale symlink.
    if(unlinkat(dir,temporary.c_str(),0)&&errno!=ENOENT)fail("Cache temporary cleanup");
    Fd fd(openat(dir,temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
    if(fd.n<0)fail("Cache temporary open");
    std::size_t at=0;
    while(at<bytes.size){auto n=write(fd.n,bytes.data+at,bytes.size-at);if(n<0&&errno==EINTR)continue;if(n<=0)fail("Cache write");at+=std::size_t(n);}
    if(fsync(fd.n)||renameat(dir,temporary.c_str(),dir,name)||fsync(dir))fail("Cache durable commit");
}
}
