
#include "redis_client.h"

RedisReplyPtr RedisClient::redisCommand(const char *format, ...) {
  RedisReplyPtr reply;
  va_list ap;

  va_start(ap, format);
  reply = redisvCommand(format, ap);
  va_end(ap);

  return reply;
}

RedisReplyPtr RedisClient::redisvCommand(const char *format, va_list ap) {
  void *reply = 0;

  RedisSocket *socket = inst_->pop_socket();
  if (socket) {
    reply = socket->redis_vcommand(inst_->config(), format, ap);
  } else {
    printf(
        "Can not get socket from redis connection pool, server down? or not "
        "enough connection?\n");
  }

  inst_->push_socket(socket);

  return RedisReplyPtr(reply);
}

int RedisClient::create_inst(const RedisConfig &config) {
  RedisInstance *inst;

  inst = new RedisInstance(config);

  /* Check config */
  if (config.num_endpoints() < 1) {
    printf("Must provide 1 redis endpoint\n");
    delete inst;
    return -1;
  }

  if (config.num_redis_socks() > MAX_REDIS_SOCKS) {
    printf("Number of redis sockets(% d) cannot exceed MAX_REDIS_SOCKS(% d)\n",
           config.num_redis_socks(), MAX_REDIS_SOCKS);
    delete inst;
    return -1;
  }

  printf(
      "Attempting to connect to above endpoints with connect_timeout %dms "
      "net_readwrite_timeout %dms\n",
      config.connect_timeout(), config.net_readwrite_timeout());

  if (inst->create_pool() < 0) {
    delete inst;
    return -1;
  }

  inst_ = inst;
  return 0;
}

void RedisClient::destroy_inst() {
  if (inst_ != NULL) {
    delete inst_;
    inst_ = NULL;
  }
}

int RedisInstance::create_pool() {
  connect_after_ = 0;

  for (int i = 0; i < config_->num_redis_socks(); i++) {
    RedisSocket *socket = new RedisSocket(i);
    socket->set_backup(i % config_->num_endpoints());

    int rcode = pthread_mutex_init(&socket->mutex(), NULL);
    if (rcode != 0) {
      printf("Failed to init lock: returns (%d)\n", rcode);
      delete socket;
      return -1;
    }

    if (socket->connect(config_) != 0) {
      connect_after_ = time(NULL) + config_->connect_failure_retry_delay();
      printf("Failed to connect to any redis server\n");
    }

    /* Add this socket to the list of sockets */
    pool_.push_back(socket);
  }

  return 0;
}

void RedisInstance::destory_pool() {
  for (const auto &socket : pool_) {
    delete socket;
  }

  pool_.clear();
  std::vector<RedisSocket *>().swap(pool_);
}

/**
 * @brief search a available socket from pool by thread lock
 *
 * @return RedisSocket* the return value should not be close
 */
RedisSocket *RedisInstance::pop_socket() {
  int rcode;
  int num_tried_to_connect = 0;
  int num_faild_to_connected = 0;

  for (const auto &socket : pool_) {
    /*
     *  If this socket is in use by another thread,
     *  skip it, and try another socket.
     *
     *  If it isn't used, then grab it ourselves.
     */
    if ((rcode = pthread_mutex_trylock(&socket->mutex())) != 0)
      continue;
    else /* else we now have the lock */
      printf("Obtained lock with socket #%d\n", socket->id());

    if (socket->inuse() == 1) {
      if ((rcode = pthread_mutex_unlock(&socket->mutex())) != 0)
        printf("Failed release lock with socket #%d: returns (%d)\n",
               socket->id(), rcode);
      else
        printf("Released lock with socket #%d\n", socket->id());
    } else
      socket->set_inuse(1);

    /*
     *  If we happen upon an unconnected socket, and
     *  this instance's grace period on
     *  (re)connecting has expired, then try to
     *  connect it.  This should be really rare.
     */
    if ((socket->state() == RedisSocket::unconnected) &&
        (time(NULL) > connect_after_)) {
      printf("Trying to (re)connect unconnected socket #%d ...\n",
             socket->id());
      num_tried_to_connect++;
      if (socket->connect(config_) != 0) {
        connect_after_ = time(NULL) + config_->connect_failure_retry_delay();
      }
    }

    /* if we still aren't connected, ignore this socket */
    if (socket->state() == RedisSocket::unconnected) {
      printf("Ignoring unconnected socket #%d ...\n", socket->id());
      num_faild_to_connected++;

      socket->set_inuse(0);

      if ((rcode = pthread_mutex_unlock(&socket->mutex())) != 0) {
        printf("Failed to release lock with socket #%d: returns (%d)\n",
               socket->id(), rcode);
      } else {
        printf("Released lock with socket #%d\n", socket->id());
      }

      continue;
    }

    /* should be connected, grab it */
    printf("Poped redis socket #%d\n", socket->id());
    if (num_faild_to_connected != 0 || num_tried_to_connect != 0) {
      printf(
          "Got socket #%d after skipping %d unconnected sockets, "
          "tried to reconnect %d though\n",
          socket->id(), num_faild_to_connected, num_tried_to_connect);
    }

    /*
     *  The socket is returned in the locked
     *  state.
     *
     *  We also remember where we left off,
     *  so that the next search can start from
     *  here.
     *
     *  Note that multiple threads MAY over-write
     *  the 'inst->last_used' variable.  This is OK,
     *  as it's a pointer only used for reading.
     */
    // inst->last_used = socket->next;
    return socket;
  }

  /* We get here if every redis socket is unconnected and
   * unconnectABLE, or in use */
  printf("There are no redis sockets to use! skipped %d, tried to connect %d\n",
         num_faild_to_connected, num_tried_to_connect);
  return NULL;
}

void RedisInstance::push_socket(RedisSocket *socket) {
  int rcode;

  if (socket == NULL) return;

  if (socket->inuse() != 1) {
    printf("I'm NOT in use while pushing. Bug?\n");
  }

  socket->set_inuse(0);

  if ((rcode = pthread_mutex_unlock(&socket->mutex())) != 0) {
    printf("Can not release lock with socket %d: returns (%d)\n", socket->id(),
           rcode);
  } else {
    printf("Released lock with socket %d\n", socket->id());
  }

  printf("Pushed redis socket #%d\n", socket->id());

  return;
}

/*
 * @brief
 * Connect to a server.  If error, set this socket's state to be
 * "unconnected" and set a grace period, during which we won't try
 * connecting again (to prevent unduly lagging the server and being
 * impolite to a server that may be having other issues).  If
 * successful in connecting, set state to "connected".
 *
 * @param config
 * @return 0 on successful connect or -1 otherwise
 */
int RedisSocket::connect(const RedisConfig *config) {
  int i;
  redisContext *ctx = NULL;
  struct timeval timeout[2];

  printf("Attempting to connect #%d @%d\n", id_, backup_);

  /* convert timeout (ms) to timeval */
  timeout[0].tv_sec = config->connect_timeout() / 1000;
  timeout[0].tv_usec = 1000 * (config->connect_timeout() % 1000);
  timeout[1].tv_sec = config->net_readwrite_timeout() / 1000;
  timeout[1].tv_usec = 1000 * (config->net_readwrite_timeout() % 1000);

  for (i = 0; i < config->num_endpoints(); i++) {
    /*
     * Get the target host/port or unix path from the backup index
     */
    const string &unix_path = config->endpoints()[backup_].unix_path();
    if (!unix_path.empty()) {
      ctx = redisConnectUnixWithTimeout(unix_path.c_str(), timeout[0]);
    } else {
      const string &host = config->endpoints()[backup_].host();
      int port = config->endpoints()[backup_].port();
      ctx = redisConnectWithTimeout(host.c_str(), port, timeout[0]);
    }

    while (ctx && ctx->err == 0) {
      // Redis authentication,
      const string &authpwd = config->endpoints()[backup_].authpwd();
      if (!authpwd.empty()) {
        redisReply *r =
            (redisReply *)redisCommand(ctx, "AUTH %s", authpwd.c_str());
        if (r == NULL || r->type == REDIS_REPLY_ERROR) {
          if (r) {
            printf("Failed to auth: %s\n", r->str);
            freeReplyObject(r);
          }
          break;
        }

        if (r) freeReplyObject(r);
      }

      printf("Connected new redis socket #%d @%d\n", id_, backup_);
      ctx_ = ctx;
      state_ = connected;
      if (config->num_endpoints() > 1) {
        /* Select the next _random_ endpoint as the new backup if succeed*/
        backup_ = (backup_ + (1 + rand() % (config->num_endpoints() - 1))) %
                  config->num_endpoints();
      }

      if (redisSetTimeout(ctx, timeout[1]) != REDIS_OK) {
        printf("Failed to set timeout: blocking-mode: %d, %s\n",
               (ctx->flags & REDIS_BLOCK), ctx->errstr);
      }

      if (unix_path.empty()) {
        if (redisEnableKeepAlive(ctx) != REDIS_OK) {
          printf("Failed to enable keepalive: %s\n", ctx->errstr);
        }
      }

      return 0;
    }

    /* We have more backups to try */
    if (ctx) {
      printf("Failed to connect redis socket #%d @%d: %s\n", id_, backup_,
             ctx->errstr);
      redisFree(ctx);
    } else {
      printf("Failed to allocate redis socket #%d @%d\n", id_, backup_);
    }

    /* We have tried the last one but still fail */
    if (i == config->num_endpoints() - 1) break;

    /* Select the next endpoint as the new backup to retry if failed*/
    backup_ = (backup_ + 1) % config->num_endpoints();
  }

  /*
   *  Error, or SERVER_DOWN.
   */
  ctx_ = NULL;
  state_ = unconnected;

  return -1;
}

/**
 * @brief disconnect to redis server
 * free redisContext and release thread lock
 */
void RedisSocket::close() {
  printf("Closing redis socket #%d @%d, state=%d\n", id_, backup_, state_);

  if (state_ == connected) {
    redisFree(ctx_);
  }

  if (inuse_) {
    printf("I'm still in use while closing. Bug?\n");
  }

  int rcode = pthread_mutex_destroy(&mutex_);
  if (rcode != 0) {
    printf("Failed to destroy lock: returns (%d)\n", rcode);
  }

  return;
}

void *RedisSocket::redis_vcommand(const RedisConfig *config, const char *format,
                                  va_list ap) {
  va_list ap2;
  void *reply = NULL;

  va_copy(ap2, ap);  // copy va_list for reconnection

  /* forward to hiredis API */
  reply = redisvCommand(ctx_, format, ap);
  if (reply == NULL) {
    /* Once an error is returned the context cannot be reused and you shoud
       set up a new connection.
     */

    /* 如果使用unix socket成功建立redisContext, 在redis server down后,
     * 重用redisContext上执行命令, 会导致hiredis库崩溃. 因此，使用unix
     * socket方式不适合做连接池
     */

    printf("Failed to redisvCommand\n");

    /* close the socket that failed */
    redisFree(ctx_);

    /* reconnect the socket */
    if (connect(config) == 0) {
      /* retry on the newly connected socket */
      reply = redisvCommand(ctx_, format, ap2);
      if (reply == NULL) {
        printf("Failed after reconnect: %s (%d)\n", ctx_->errstr, ctx_->err);
        /* do not need clean up here because the next caller will retry. */
      }
    } else {
      printf("Reconnect failed, server down?\n");
    }
  }

  va_end(ap2);
  return reply;
}
