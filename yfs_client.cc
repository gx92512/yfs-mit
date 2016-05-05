// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client_cache(lock_dst);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

int
yfs_client::create(inum parent, const char *name, inum &num)
{
    MutexLockGuard mlg(lc, parent);
    int r = OK;
    std::string pdata;
    if (ec -> get(parent, pdata) != extent_protocol::OK)
    {
        r = IOERR;
        return r;
    }
    std::string sname = std::string(name);
    if (pdata.find("/"+sname+"/") != std::string::npos)
    {
        r = EXIST;
        return r;
    }
    num =(inum) ((rand() & 0xffffffff) | 0x80000000);
    pdata += "/" + sname + "/" + filename(num) + "/";
    std::string emdata;
    if (ec -> put(num, emdata) != extent_protocol::OK)
    {
        r = IOERR;
        return r;
    }
    if (ec -> put(parent, pdata) != extent_protocol::OK)
    {
        r = IOERR;
        return r;
    }
    return r;
}

int 
yfs_client::mkdir(inum parent, const char *name, inum &num)
{
    MutexLockGuard mlg(lc, parent);
    std::string pdata;
    if (ec -> get(parent, pdata) != extent_protocol::OK)
        return IOERR;
    std::string sname = std::string(name);
    if (pdata.find("/"+sname+"/") != std::string::npos)
        return EXIST;
    num = (inum) (rand() & 0x7fffffff);
    pdata += "/" + sname + "/" + filename(num) + "/";
    std::string emdata;
    if (ec -> put(num, emdata) != extent_protocol::OK)
        return IOERR;
    if (ec -> put(parent, pdata) != extent_protocol::OK)
        return IOERR;
    return OK;
}

int 
yfs_client::unlink(inum parent, const char *name)
{
    MutexLockGuard mlg(lc, parent);
    std::string pdata;
    if (ec -> get(parent, pdata) != extent_protocol::OK)
        return IOERR;
    std::string sname = "/" + std::string(name) + "/";
    size_t pos = pdata.find(sname);
    if (pos == std::string::npos)
        return EXIST;
    size_t pos2 = pdata.find("/", pos + sname.length());
    inum num = n2i(pdata.substr(pos + sname.length(), pos2 - pos - sname.length()).c_str());
    if (!isfile(num))
        return EXIST;
    if (pos2 == pdata.length() - 1)
        pdata = pdata.substr(0, pos);
    else
        pdata = pdata.substr(0, pos) + pdata.substr(pos2+1, pdata.length());
    if (ec -> put(parent, pdata) != extent_protocol::OK)
        return IOERR;
    if (ec -> remove(num) != extent_protocol::OK)
        return IOERR;
    return OK;
}

bool
yfs_client::lookup(inum parent, const char* name, inum &num)
{
    std::string pdata;
    if (ec -> get(parent, pdata) != extent_protocol::OK)
        return false;
    std::string sname = "/" + std::string(name) + "/";
    size_t pos = pdata.find(sname);
    if (pos == std::string::npos)
        return false;
    size_t pos2 = sname.find("/", pos + sname.length());
    num = n2i(pdata.substr(pos + sname.length(), pos2 - pos - sname.length()).c_str());
    return true;
}

int   
yfs_client::readdir(inum parent, std::list<dirent> &c)
{
    std::string pdata;
    if (ec -> get(parent, pdata) != extent_protocol::OK)
        return IOERR;
    size_t pos1  = 0, pos2 = 0;
    while(pos1 < pdata.length())
    {
        pos1 = pdata.find("/", pos1);
        if (pos1 == std::string::npos)
            return OK;
        dirent d;
        d.inum = 0;
        pos2 = pdata.find("/", pos1 + 1);
        d.name = pdata.substr(pos1 + 1, pos2 - pos1 - 1);
        pos1 = pdata.find("/", pos2 + 1);
        d.inum = n2i(pdata.substr(pos2+1, pos1-pos2-1));
        c.push_back(d);
        ++pos1;
    }
    return OK;
}

int 
yfs_client::setattr(inum ino, struct stat *attr)
{
    MutexLockGuard mlg(lc, ino);
    std::string pdata;
    if (ec -> get(ino, pdata) != extent_protocol::OK)
        return IOERR;
    pdata.resize(attr -> st_size, '\0');
    if (ec -> put(ino, pdata) != extent_protocol::OK)
        return IOERR;
    return OK;
}

int 
yfs_client::read(inum ino, off_t off, size_t size, std::string &buf)
{
    std::string pdata;
    if (ec -> get(ino, pdata) != extent_protocol::OK)
        return IOERR;
    if (pdata.length() <= off)
        return OK;
    buf = pdata.substr(off, size);
    return OK;
}

int 
yfs_client::write(inum ino, off_t off, size_t size, const char* buf)
{
    MutexLockGuard mlg(lc, ino);
    std::string pdata;
    if (ec -> get(ino, pdata) != extent_protocol::OK)
        return IOERR;
    if (pdata.length() < off + size)
        pdata.resize(off + size, '\0');
    for (size_t i = 0; i < size; ++i)
        pdata[off + i] = buf[i];
    if (ec -> put(ino, pdata) != extent_protocol::OK)
        return IOERR;
    return OK;
}
