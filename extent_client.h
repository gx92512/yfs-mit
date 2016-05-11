// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <map>
#include "pthread.h"
#include "extent_protocol.h"
#include "rpc.h"

class extent_client {
 public:
  enum cache_stat{NONE, CACHED, CHANGED, REMOVED};
  struct cached_file
  {
      cache_stat stat;
      std::string content;
      extent_protocol::attr attr;
  };
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status flush(extent_protocol::extentid_t eid);
 
 private:
  rpcc *cl;
  pthread_mutex_t mutex;
  std::map<extent_protocol::extentid_t, cached_file> cache_pool;
};

#endif 

