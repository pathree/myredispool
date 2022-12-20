
#include "redis_client.h"

RedisReply RedisClient::redisCommand(const char *format, ...) {
  RedisReply reply;
  va_list ap;

  va_start(ap, format);
  reply = redisvCommand(format, ap);
  va_end(ap);

  return reply;
}

RedisReply RedisClient::redisvCommand(const char *format, va_list ap) {
  void *reply = 0;

  RedisSocket *socket = inst_->pop_socket();
  if (socket) {
    reply = socket->redis_vcommand(inst_->config(), format, ap);
  } else {
    printf(
        "Can not get socket from redis connection pool, "
        "server down or not enough connection\n");
  }

  inst_->push_socket(socket);

  return RedisReply(reply);
}

int RedisClient::create_inst(const RedisConfig &config) {
  RedisInstance *inst = NULL;

  /* Check config */
  if (config.num_endpoints < 1) {
    printf("Must provide 1 redis endpoint\n");
    return -1;
  }

  if (config.num_redis_socks > MAX_REDIS_SOCKS) {
    printf("Number of redis sockets(% d) cannot exceed MAX_REDIS_SOCKS(% d)\n",
           config.num_redis_socks, MAX_REDIS_SOCKS);
    return -1;
  }

  inst = new RedisInstance(config);

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

RedisInstance::RedisInstance(const RedisConfig &config) {
  config_ = new RedisConfig(config);

  if (config.num_endpoints > 0) {
    RedisEndpoint *endpoints = new RedisEndpoint[config.num_endpoints];
    for (int i = 0; i < config.num_endpoints; i++) {
      endpoints[i] = config.endpoints[i];
    }

    config_->endpoints = endpoints;
  }

  connect_after_ = 0;
}

RedisInstance::~RedisInstance() {
  if (config_ != NULL) {
    if (config_->endpoints != NULL) delete[] config_->endpoints;
    delete config_;
    config_ = NULL;
  }

  destory_pool();
}

int RedisInstance::create_pool() {
  printf(
      "Attempting to connect to endpoints with connect_timeout %dms "
      "net_readwrite_timeout %dms\n",
      config_->connect_timeout, config_->net_readwrite_timeout);
  for (int i = 0; i < config_->num_endpoints; i++)
    printf("[%d] %s:%d %s\n", i, config_->endpoints[i].host,
           config_->endpoints[i].port, config_->endpoints[i].unix_path);

  for (int i = 0; i < config_->num_redis_socks; i++) {
    RedisSocket *socket = new RedisSocket(i);
    socket->set_master(i % config_->num_endpoints);
    socket->set_backup(i % config_->num_endpoints);

    if (socket->connect(config_) != 0) {
      connect_after_ = time(NULL) + config_->connect_failure_retry_delay;
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
 * @brief 从连接池中找到一个空闲的RedisSocket
 * 连接池是一个临界资源，多线程中需要加锁
 * 加锁的方式有两种：
 * 1、一个全局锁，每次进入时锁上退出时释放，缺点是获取锁时可能重连操作导致线程阻塞
 * 2、每个Socket一个锁，好处时并发性能更好，代价是所有的socket都要被try_lock一次
 *
 * @return RedisSocket* the return value should not be close
 */
RedisSocket *RedisInstance::pop_socket() {
  int num_tried_to_connect = 0;
  int num_faild_to_connected = 0;

  for (const auto &socket : pool_) {
    /*
     *  If this socket is in use by another thread,
     *  skip it, and try another socket.
     *
     *  If it isn't used, then grab it ourselves.
     */
    if (!socket->mutex().try_lock())
      continue;
    else /* else we now have the lock */
      x_debug_lock("Obtained lock of socket #%d\n", socket->id());

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
        connect_after_ = time(NULL) + config_->connect_failure_retry_delay;
      }
    }

    /* if we still aren't connected, ignore this socket */
    if (socket->state() == RedisSocket::unconnected) {
      printf("Ignoring unconnected socket #%d ...\n", socket->id());
      num_faild_to_connected++;

      socket->mutex().unlock();
      x_debug_lock("Released lock of socket #%d\n", socket->id());

      continue;
    }

    /* should be connected, grab it */
    printf("Poped redis socket #%d @%d-%d\n", socket->id(), socket->master(),
           socket->backup());
    if (num_faild_to_connected != 0 || num_tried_to_connect != 0) {
      printf(
          "Got socket #%d after skipping %d unconnected sockets, "
          "tried to reconnect %d though\n",
          socket->id(), num_faild_to_connected, num_tried_to_connect);
    }

    /*
     *  The socket is returned in the locked state.
     */
    return socket;
  }

  /* We get here if every redis socket is unconnected and
   * unconnectABLE, or in use */
  printf(
      "There are no redis sockets to use while skipped %d unconnected sockets, "
      "tried to connect %d\n",
      num_faild_to_connected, num_tried_to_connect);
  return NULL;
}

void RedisInstance::push_socket(RedisSocket *socket) {
  if (socket == NULL) return;

  socket->mutex().unlock();
  x_debug_lock("Released lock of socket #%d\n", socket->id());

  printf("Pushed redis socket #%d @%d-%d\n", socket->id(), socket->master(),
         socket->backup());

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

  /* convert timeout (ms) to timeval */
  timeout[0].tv_sec = config->connect_timeout / 1000;
  timeout[0].tv_usec = 1000 * (config->connect_timeout % 1000);
  timeout[1].tv_sec = config->net_readwrite_timeout / 1000;
  timeout[1].tv_usec = 1000 * (config->net_readwrite_timeout % 1000);

  for (i = 0; i < config->num_endpoints; i++) {
    if (master_ != backup_) master_ = backup_;
    printf("Attempting to connect #%d @%d\n", id_, master_);

    /*
     * Get the target host/port or unix path from the backup index
     */
    const char *unix_path = config->endpoints[master_].unix_path;
    if (unix_path[0]) {
      ctx = redisConnectUnixWithTimeout(unix_path, timeout[0]);
      type_ = unixsocket;
    } else {
      const char *host = config->endpoints[master_].host;
      int port = config->endpoints[master_].port;
      ctx = redisConnectWithTimeout(host, port, timeout[0]);
      type_ = ipsocket;
    }

    while (ctx && ctx->err == 0) {
      // Redis authentication
      const char *authpwd = config->endpoints[master_].authpwd;
      if (authpwd[0]) {
        redisReply *r = (redisReply *)redisCommand(ctx, "AUTH %s", authpwd);
        if (r == NULL || r->type == REDIS_REPLY_ERROR) {
          if (r) {
            printf("Failed to auth: %s\n", r->str);
            freeReplyObject(r);
          }
          break;
        }

        if (r) freeReplyObject(r);
      }

      printf("Connected new redis socket #%d @%d\n", id_, master_);
      ctx_ = ctx;
      state_ = connected;
      if (config->num_endpoints > 1) {
        /* Select the next _random_ endpoint as the new backup if succeed*/
        backup_ = (master_ + (1 + rand() % (config->num_endpoints - 1))) %
                  config->num_endpoints;
      }

      if (redisSetTimeout(ctx, timeout[1]) != REDIS_OK) {
        printf("Failed to set timeout: blocking-mode: %d, %s\n",
               (ctx->flags & REDIS_BLOCK), ctx->errstr);
      }

      if (type_ == ipsocket) {
        if (redisEnableKeepAlive(ctx) != REDIS_OK) {
          printf("Failed to enable keepalive: %s\n", ctx->errstr);
        }
      }

      return 0;
    }

    /* We have more backups to try */
    if (ctx) {
      printf("Failed to connect redis socket #%d @%d: %s\n", id_, master_,
             ctx->errstr);
      redisFree(ctx);
    } else {
      printf("Failed to allocate redis socket #%d @%d\n", id_, master_);
    }

    /* We have tried the last one but still fail */
    if (i == config->num_endpoints - 1) break;

    /* Select the next endpoint as the new backup to retry if failed*/
    backup_ = (master_ + 1) % config->num_endpoints;
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
void RedisSocket::disconnect() {
  printf("Disconnect redis socket #%d @%d, state=%d\n", id_, master_, state_);

  if (state_ == connected) {
    redisFree(ctx_);
    ctx_ = NULL;
  }

  state_ = unconnected;
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
      printf("Reconnect failed, maybe server down\n");
    }
  }

  va_end(ap2);

  /*
   使用unix socket成功建立redisContext后, 如果redis server down,
   在已有的redisContext上执行命令会导致hiredis库崩溃.
   因此，使用unix socket每次命令后都要关闭redisContext
  */
  if (type_ == unixsocket) disconnect();

  return reply;
}
