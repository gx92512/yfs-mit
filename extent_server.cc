// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {
    mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret;
    put(1, "", ret);
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  pthread_mutex_lock(&mutex);
  if (file_map.find(id) != file_map.end())
  {
      file_map[id].data = buf;
      file_map[id].attr.mtime = time(NULL);
      file_map[id].attr.ctime = time(NULL);
      file_map[id].attr.size = buf.size();
  }
  else
  {
      file_cont da;
      da.data = buf;
      da.attr.size = buf.size();
      da.attr.mtime = time(NULL);
      da.attr.atime = time(NULL);
      da.attr.ctime = time(NULL);
      file_map[id] = da;
  }
  pthread_mutex_unlock(&mutex);
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  pthread_mutex_lock(&mutex);
  std::map<extent_protocol::extentid_t, file_cont>::iterator it = file_map.find(id);
  if (it == file_map.end())
  {
    pthread_mutex_unlock(&mutex);
    return extent_protocol::NOENT;
  }
  buf = (it -> second).data;
  (it -> second).attr.atime = time(NULL);
  pthread_mutex_unlock(&mutex);
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  pthread_mutex_lock(&mutex);
  std::map<extent_protocol::extentid_t, file_cont>::iterator it = file_map.find(id);
  if (it == file_map.end())
  {
      pthread_mutex_unlock(&mutex);
      return extent_protocol::IOERR;
  }
  a = (it -> second).attr;
  pthread_mutex_unlock(&mutex);
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  pthread_mutex_lock(&mutex);
  std::map<extent_protocol::extentid_t, file_cont>::iterator it = file_map.find(id);
  if (it == file_map.end())
  {
      pthread_mutex_unlock(&mutex);
      return extent_protocol::IOERR;
  }
  file_map.erase(it);
  pthread_mutex_unlock(&mutex);
  return extent_protocol::OK;
}

