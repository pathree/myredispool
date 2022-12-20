/* Author:
 * Date:     2022-10
 * Revision: 1.0
 * Function: 轻量级的hiredis连接池
 *     + 线程安全的连接池
 *     + 请求时触发重连
 *     + 多Redis服务支持
 *     + 支持地址端口和UnixSocket
 * Usage:
 */

#ifndef REDIS_CLIENT_H_
#define REDIS_CLIENT_H_

#include <pthread.h>
#include <stdarg.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "hiredis/hiredis.h"

using std::string;

#define MAX_REDIS_SOCKS 1000

// #define x_debug_lock(...) printf(__VA_ARGS__)
#define x_debug_lock(...)
// #define x_debug_reply(...) printf(__VA_ARGS__)
#define x_debug_reply(...)
#define x_debug_socket(...) printf(__VA_ARGS__)

/**
 * @brief Redis服务端点
 *    + 支持地址端口和UnixSocket接入
 *    + 支持Auth认证
 *
 */
struct RedisEndpoint {
  char host[256];
  int port;
  char unix_path[256];
  char authpwd[256];
};

/**
 * @brief Redis配置
 *    + 包括端点、连接池大小、连接超时、读写超时、重试延迟
 *
 */
struct RedisConfig {
  RedisEndpoint* endpoints;
  int num_endpoints;
  int num_redis_socks;
  int connect_timeout;              // ms
  int net_readwrite_timeout;        // ms
  int connect_failure_retry_delay;  // seconds
};

/**
 * @brief 连接Socket，封装了hiredis的redisContext
 *    + 使用线程锁机制支持多线程
 */
class RedisSocket {
 public:
  enum SocketStatus { unconnected = 0, connected };
  enum SocketType { ipsocket = 0, unixsocket };

  RedisSocket(int id) : id_(id), state_(unconnected), ctx_(NULL) {}
  ~RedisSocket() { disconnect(); }

  int connect(const RedisConfig* config);
  void disconnect();

  void* redis_vcommand(const RedisConfig* config, const char* format,
                       va_list ap);

  int id() { return id_; }
  int master() { return master_; }
  int backup() { return backup_; }
  void set_master(int master) { master_ = master; }
  void set_backup(int backup) { backup_ = backup; }

  int state() { return state_; }
  std::mutex& mutex() { return mutex_; }

 private:
  int id_;      // socket序号
  int master_;  // 当前endpoint序号
  int backup_;  // 备用endpoint序号
  std::mutex mutex_;
  enum SocketType type_;
  enum SocketStatus state_;
  redisContext* ctx_;
};

/**
 * @brief Redis连接池实例，每个实例对应一个连接池
 *
 */
class RedisInstance {
 public:
  RedisInstance(const RedisConfig& config);
  ~RedisInstance();

  const RedisConfig* config() const { return config_; }

  time_t connect_after() const { return connect_after_; }
  void set_connect_after(time_t connect_after) {
    connect_after_ = connect_after;
  }

  int create_pool();
  void destory_pool();

  RedisSocket* pop_socket();
  void push_socket(RedisSocket* socket);

 private:
  time_t connect_after_;
  RedisConfig* config_;
  std::vector<RedisSocket*> pool_;
};

/**
 * @brief
 * 把redisReply封装到RedisReply对象中,
 * 放入到共享智能指针std::shared_ptr避免RedisReply对象析构过程中被释放
 * 当引用计数为0时才释放redisReply
 * 参考《Effective C++》条款31
 */
class RedisReply {
 public:
  explicit RedisReply() {}
  explicit RedisReply(void* reply) {
    x_debug_reply("Got redis reply %p\n", (void*)reply);
    ptr_.reset((redisReply*)reply, [](redisReply* reply) {
      /*引用计数为0时，调用删除器释放redisReply*/
      x_debug_reply("Released redis reply %p\n", reply);
      freeReplyObject(reply);
    });
  }

  redisReply* operator->() const { return ptr_.get(); }
  redisReply* operator&() const { return ptr_.get(); }

  operator bool() const { return ptr_.get(); }

 private:
  std::shared_ptr<redisReply> ptr_;
};

/**
 * @brief  连接池客户端对象，隐藏连接池细节，调用者通过redisCommand执行命令
 *
 */
class RedisClient {
 public:
  RedisClient(const RedisConfig& config) {
    inst_ = NULL;
    create_inst(config);
  }

  ~RedisClient() {
    destroy_inst();
    inst_ = NULL;
  }

  // ----------------------------------------------------
  // Thread-safe command
  // ----------------------------------------------------

  // redisCommand is a thread-safe wrapper of that function in hiredis
  // It first get a connection from pool, execute the command on that
  // connection and then release the connection to pool.
  // the command's reply is returned as a smart pointer,
  // which can be used just like raw redisReply pointer.
  RedisReply redisCommand(const char* format, ...);
  RedisReply redisvCommand(const char* format, va_list ap);

 private:
  RedisClient(const RedisClient&);
  RedisClient& operator=(const RedisClient&);

  int create_inst(const RedisConfig& config);
  void destroy_inst();

  RedisInstance* inst_;
};

#endif  // REDIS_CLIENT_H_
