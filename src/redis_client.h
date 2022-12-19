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

/**
 * @brief Redis服务端点
 *    + 支持地址端口和UnixSocket接入
 *    + 支持Auth认证
 *
 */
class RedisEndpoint {
 public:
  RedisEndpoint() {}
  RedisEndpoint(const string& host, int port, const string& authpwd = "")
      : host_(host), port_(port), authpwd_(authpwd){};
  RedisEndpoint(const string& unix_path, const string& authpwd = "")
      : unix_path_(unix_path), authpwd_(authpwd){};

  const string& host() const { return host_; }
  const int& port() const { return port_; }
  const string& unix_path() const { return unix_path_; }
  const string& authpwd() const { return authpwd_; }

 private:
  string host_;
  int port_;
  string unix_path_;
  string authpwd_;
};

/**
 * @brief Redis配置
 *    + 包括端点、连接池大小、连接超时、读写超时、重试延迟
 *
 */
class RedisConfig {
 public:
  RedisConfig(const RedisEndpoint* endpoints, int num_endpoints,
              int num_redis_socks = 5, int connect_timeout = 10000,
              int net_readwrite_timeout = 5000,
              int connect_failure_retry_delay = 1)
      : endpoints_(endpoints),
        num_endpoints_(num_endpoints),
        num_redis_socks_(num_redis_socks),
        connect_timeout_(connect_timeout),
        net_readwrite_timeout_(net_readwrite_timeout),
        connect_failure_retry_delay_(connect_failure_retry_delay) {}

  ~RedisConfig() {}

  const RedisEndpoint* endpoints() const { return endpoints_; }
  void set_endpoints(RedisEndpoint* endpoints, int num_endpoints) {
    endpoints_ = endpoints;
    num_endpoints_ = num_endpoints;
  }
  int num_endpoints() const { return num_endpoints_; }
  int num_redis_socks() const { return num_redis_socks_; }
  int connect_timeout() const { return connect_timeout_; }
  int net_readwrite_timeout() const { return net_readwrite_timeout_; }
  int connect_failure_retry_delay() const {
    return connect_failure_retry_delay_;
  }

 private:
  const RedisEndpoint* endpoints_;
  int num_endpoints_;
  int num_redis_socks_;
  int connect_timeout_;              // ms
  int net_readwrite_timeout_;        // ms
  int connect_failure_retry_delay_;  // seconds
};

/**
 * @brief 连接Socket，封装了hiredis的redisContext
 *    + 使用线程锁机制支持多线程
 */
class RedisSocket {
 public:
  enum SocketStatus { unconnected = 0, connected };
  RedisSocket(int id) : id_(id), state_(unconnected), ctx_(NULL) {}
  ~RedisSocket() { close(); }

  int connect(const RedisConfig* config);
  void close();

  void* redis_vcommand(const RedisConfig* config, const char* format,
                       va_list ap);

  int id() { return id_; }
  int backup() { return backup_; }
  void set_backup(int backup) { backup_ = backup; }
  int state() { return state_; }
  std::mutex& mutex() { return mutex_; }

 private:
  int id_;      // socket序号
  int backup_;  // 备用endpoints序号
  std::mutex mutex_;
  enum SocketStatus state_;
  redisContext* ctx_;
};

/**
 * @brief Redis连接池实例，每个实例对应一个连接池
 *
 */
class RedisInstance {
 public:
  RedisInstance(const RedisConfig& config) { set_config(config); }
  ~RedisInstance() {
    free_config();
    destory_pool();
  }

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
  void set_config(const RedisConfig& config) {
    free_config();

    config_ = new RedisConfig(config);
    config_->set_endpoints(NULL, 0);

    if (config.num_endpoints() > 0) {
      RedisEndpoint* endpoints = new RedisEndpoint[config.num_endpoints()];
      for (int i = 0; i < config.num_endpoints(); i++) {
        endpoints[i] = config.endpoints()[i];
      }
      config_->set_endpoints(endpoints, config.num_endpoints());
    }
  }

  void free_config() {
    if (config_ != NULL) {
      if (config_->endpoints() != NULL) delete[] config_->endpoints();
      delete config_;
      config_ = NULL;
    }
  }

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
    ptr_.reset((redisReply*)reply, [](redisReply* r) {
      /*引用计数为0时，调用删除器释放redisReply*/
      // printf("Released redis reply %p\n", (void*)r);
      freeReplyObject(r);
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
