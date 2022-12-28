#include <string.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "redis_client.h"

using std::string;

void sleep_awhile(int ms) {
  struct timeval delay_time;

  memset(&delay_time, 0, sizeof(struct timeval));
  delay_time.tv_sec += ms / 1000;
  delay_time.tv_usec = (ms % 1000) * 1000;

  select(0, NULL, NULL, NULL, &delay_time);
  return;
}

int redis_set(const string &key, const string &value, int expire = 0) {
  if (key.empty() || value.empty()) return REDIS_ERR;

  RedisReply reply;
  if (expire > 0)
    reply = RedisClient::inst().redisCommand("SET %s %s EX %d", key.c_str(),
                                             value.c_str(), expire);
  else
    reply = RedisClient::inst().redisCommand("SET %s %s", key.c_str(),
                                             value.c_str());
  if (!reply || strcmp(reply->str, "OK")) {
    if (reply)
      x_error("SET key[%s] value[%s] expire[%d], error: %s\n", key.c_str(),
              value.c_str(), expire, reply->str);
    else
      x_error("SET key[%s] value[%s] expire[%d], error: reply is empty\n",
              key.c_str(), value.c_str(), expire);

    return REDIS_ERR;
  }

  x_info("SET key[%s] value[%s] expire[%d] %s\n", key.c_str(), value.c_str(),
         expire, reply->str);

  return REDIS_OK;
}

int redis_get(const string &key, string &value, int del = 0) {
  if (key.empty()) return REDIS_ERR;

  RedisReply reply = RedisClient::inst().redisCommand("GET %s", key.c_str());
  if (!reply || reply->type != REDIS_REPLY_STRING) {
    if (reply)
      if (reply->type == REDIS_REPLY_NIL)
        x_error("GET key[%s], error: key not exist\n", key.c_str());
      else
        x_error("GET key[%s], error: %s\n", key.c_str(), reply->str);
    else
      x_error("GET key[%s], error: reply is empty\n", key.c_str());
    return REDIS_ERR;
  }

  value = reply->str;

  if (del == 1) {  // 从6.2.0版本才支持GETDEL
    RedisReply reply = RedisClient::inst().redisCommand("DEL %s", key.c_str());
    if (!reply || !strcmp(reply->str, "OK")) {
      if (reply)
        x_error("DEL key[%s], error: %s\n", key.c_str(), reply->str);
      else
        x_error("DEL key[%s], error: reply is empty\n", key.c_str());
    }
  }

  x_info("GET key[%s] value[%s] del[%d] %s\n", key.c_str(), value.c_str(), del,
         reply->str);

  return REDIS_OK;
}

void test() {
  {
    std::cout << "Press <ENTER> to continue..." << std::endl;
    std::cin.get();

    RedisReply reply =
        RedisClient::inst().redisCommand("SET %s %s", "key0", "value0");
    if (reply)
      std::cout << "SET: " << reply->str << std::endl;
    else
      std::cout << "SET: Something wrong." << std::endl;
  }

  {
    std::cout << "Press <ENTER> to continue..." << std::endl;
    std::cin.get();

    RedisReply reply = RedisClient::inst().redisCommand("GET %s", "key0");
    if (reply)
      if (reply->type == REDIS_REPLY_NIL)
        std::cout << "GET: Key does not exist." << std::endl;
      else
        std::cout << "GET: " << reply->str << std::endl;
    else
      std::cout << "GET: "
                << "Something wrong." << std::endl;
  }
}

void *run(void *arg) {
  const string key = std::to_string(std::rand());
  const string value = std::to_string(std::rand());
  RedisReply reply =
      RedisClient::inst().redisCommand("SET %s %s", key.c_str(), value.c_str());
  if (reply)
    std::cout << "SET: " << value << " OK" << std::endl;
  else
    std::cout << "SET: Something wrong." << std::endl;

  // sleep_awhile(100);

  reply = RedisClient::inst().redisCommand("GET %s", key.c_str());
  if (reply) {
    if (reply->type == REDIS_REPLY_NIL)
      std::cout << "GET: Key does not exist." << std::endl;
    else
      std::cout << "GET: " << reply->str << " OK" << std::endl;
  } else
    std::cout << "GET: Something wrong." << std::endl;

  return nullptr;
}

int main(int argc, char **argv) {
  int num_redis_socks = 1;
  int connect_timeout = 5000;           // ms
  int net_readwrite_timeout = 1000;     // ms
  int connect_failure_retry_delay = 1;  // seconds

  if (argc >= 2) {
    num_redis_socks = atoi(argv[1]);
  }

  x_info("Start...\n");

  RedisEndpoint endpoints[] = {{"127.0.0.1", 6379, "", "slc360"},
                               {"127.0.0.1", 6379, "", "slc360"},
                               {"", 0, "/var/run/redis.sock", "slc360"}};
  RedisConfig config = {(RedisEndpoint *)&endpoints,
                        (sizeof(endpoints) / sizeof(RedisEndpoint)),
                        num_redis_socks,
                        connect_timeout,
                        net_readwrite_timeout,
                        connect_failure_retry_delay};

  RedisClient::inst(&config);

  redis_set("aaa", "123456");
  string value;
  redis_get("aaa", value);

  return 0;

  while (1) test();

  int num_of_thread = num_redis_socks;
  pthread_t tid[num_of_thread];
  for (int i = 0; i < num_of_thread; i++) {
    pthread_create(&tid[i], NULL, run, NULL);
  }

  for (int i = 0; i < num_of_thread; i++) {
    pthread_join(tid[i], NULL);
  }

  return 0;
}