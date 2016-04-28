// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <pthread.h>
#include <map>
#include "extent_protocol.h"

class extent_server {

 public:
  extent_server();

  struct file_cont
  {
      std::string data;
      extent_protocol::attr attr;
  };
  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
 private:
  pthread_mutex_t mutex;
  std::map<extent_protocol::extentid_t, file_cont> file_map;
};

#endif 







