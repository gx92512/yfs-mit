// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
  enum lock_stat{ NONE, FREE, LOCKED, ACQUIRING, RELEASING};
  struct cached_lock
  {
      cached_lock()
      {
          stat = NONE;
          retry = false;
          revoke = false;
          retry_con = PTHREAD_COND_INITIALIZER;
          wait_con = PTHREAD_COND_INITIALIZER;
          release_con = PTHREAD_COND_INITIALIZER;
      }
      bool retry;
      bool revoke;
      lock_stat stat;
      pthread_cond_t retry_con;
      pthread_cond_t wait_con;
      pthread_cond_t release_con;
  };
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  pthread_mutex_t mutex;
  std::map<lock_protocol::lockid_t, cached_lock> lock_pool;
 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
};


#endif
