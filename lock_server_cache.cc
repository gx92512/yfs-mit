// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
    mutex = PTHREAD_MUTEX_INITIALIZER;
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  lock_protocol::status ret = lock_protocol::OK;
  bool should_revoke= false;
  pthread_mutex_lock(&mutex);
  std::map<lock_protocol::lockid_t, lock_cached>::iterator 
      it = lock_pool.find(lid);
  if (it == lock_pool.end())
      it = (lock_pool.insert(std::make_pair(lid, lock_cached())).first);
  //printf("%s %d %d\n", id.c_str(), lid, it->second.stat);
  //printf("waiters size %d\n", it -> second.waiters.size());
  if (it->second.stat == FREE)
  {
      it -> second.user = id;
      it -> second.stat = LOCKED;
  }
  else if (it -> second.stat == LOCKED)
  {
      it -> second.stat = WAITED;
      it -> second.waiters.push_back(id);
      ret = lock_protocol::RETRY;
      //printf("waiters size %d\n", it -> second.waiters.size());
      should_revoke = true;
  }
  else if (it -> second.stat == WAITED)
  {
      it -> second.waiters.push_back(id);
      ret = lock_protocol::RETRY;
  }
  else if (it -> second.stat == RETRY)
  {
      std::string needid = it -> second.waiters.front();
      if (id == needid)
      {
          it -> second.waiters.pop_front();
          it -> second.user = id;
          ret = lock_protocol::OK;
          if (it -> second.waiters.size())
          {
              it -> second.stat = WAITED;
              should_revoke = true;
          }
          else
              it -> second.stat = LOCKED;
      }
      else
      {
          it -> second.waiters.push_back(id);
          ret = lock_protocol::RETRY;
      }
  }
  pthread_mutex_unlock(&mutex);
  if (should_revoke)
  {
      int r;
      //printf("revoke %s %d\n", it->second.user.c_str(), lid);
      handle(it->second.user).safebind()->call(rlock_protocol::revoke, lid, r);
  }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::string client_id;
  pthread_mutex_lock(&mutex);
  std::map<lock_protocol::lockid_t, lock_cached>::iterator 
      it = lock_pool.find(lid);
  //printf("release called %s, %d, %d\n", id.c_str(), lid, it -> second.stat);

  if (it == lock_pool.end())
      ret = lock_protocol::NOENT;
  if (it -> second.stat == FREE)
      ret = lock_protocol::IOERR;
  else if (it -> second.stat == LOCKED)
  {
      it -> second.stat = FREE;
      ret = lock_protocol::OK;
  }
  else if (it -> second.stat == WAITED)
  {
      client_id = it -> second.waiters.front();
      it -> second.stat = RETRY;
      pthread_mutex_unlock(&mutex);
      handle(client_id).safebind()->call(rlock_protocol::retry, lid, r);
      pthread_mutex_lock(&mutex);
      ret = lock_protocol::OK;
  }
  else
  {
      ret = lock_protocol::IOERR;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
  /*
  if (it -> second.waiters.size() == 0)
      it -> second.stat = FREE;
  else //if (it -> second.stat == WAITED)
  {
      client_id = *it->second.waiters.begin();
      it -> second.waiters.erase(client_id);
      it -> second.stat = FREE;
      pthread_mutex_unlock(&mutex);
      //printf("retry %s %d %d\n", client_id.c_str(), lid, it -> second.waiters.size());
      handle(client_id).safebind()->call(rlock_protocol::retry, lid, r);
      pthread_mutex_lock(&mutex);
      if (it -> second.waiters.size())
      {
          pthread_mutex_unlock(&mutex);
          //printf("revoke %s %d\n", client_id.c_str(), lid);
          handle(client_id).safebind()->call(rlock_protocol::revoke, lid, r);
          pthread_mutex_lock(&mutex);
      }
      //if (!it -> second.waiters.size())
      //    it -> second.stat == LOCKED;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
  */
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

