#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include "lock_client.h"
#include "pthread.h"
#include <vector>


class yfs_client {
  extent_client *ec;
  lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int create(inum parent, const char *name, inum &inum);
  bool lookup(inum parent, const char* name, inum &num);
  int readdir(inum parent, std::list<dirent> &c);
  int setattr(inum ino, struct stat *attr);
  int read(inum ino, off_t off, size_t size, std::string &buf);
  int write(inum ino, off_t off, size_t size, const char *buf);
  int mkdir(inum parent, const char *name, inum &num);
  int unlink(inum parent, const char *name);
};

class MutexLockGuard
{
private:
    lock_client *_lc;
    lock_protocol::lockid_t _lid;
public:
    MutexLockGuard(lock_client *lc, lock_protocol::lockid_t lid): _lc(lc), _lid(lid)
    {
        _lc -> acquire(_lid);
    }
    ~MutexLockGuard()
    {
        _lc -> release(_lid);
    }
};

#endif 
