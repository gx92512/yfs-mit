#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include <set>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
  enum status{ FREE, LOCKED, WAITED, RETRY};
  struct lock_cached
  {
      lock_cached()
      {
          stat = FREE;
      }
      status stat;
      std::string user;
      std::set<std::string> waiters;
  };
 private:
  int nacquire;
  pthread_mutex_t mutex;
  std::map<lock_protocol::lockid_t, lock_cached> lock_pool;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
