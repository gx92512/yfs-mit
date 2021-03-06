// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#include "rpc.h"

class lock_protocol {
 public:
  enum xxstatus { OK, RETRY, RPCERR, NOENT, IOERR };
  typedef int status;
  typedef unsigned long long lockid_t;
  typedef unsigned long long xid_t;
  enum rpc_numbers {
    acquire = 0x7001,
    release,
    stat
  };
};

class rlock_protocol {
 public:
  enum xxstatus { OK, RPCERR };
  typedef int status;
  enum rpc_numbers {
    revoke = 0x8001,
    retry = 0x8002
  };
};

class lock {
public:
	enum lock_state {
		FREE,
		LOCKED
	}status;
	lock(lock_protocol::lockid_t lock_id): lockid(lock_id) 
	{
		cond = PTHREAD_COND_INITIALIZER;
	}
	~lock();
	pthread_cond_t cond;
private:
	lock_protocol::lockid_t lockid;
};

#endif 
