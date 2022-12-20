#include <string.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "redis_client.h"

using std::cout;
using std::string;

void sleep_awhile(int ms) {
  struct timeval delay_time;

  memset(&delay_time, 0, sizeof(struct timeval));
  delay_time.tv_sec += ms / 1000;
  delay_time.tv_usec = (ms % 1000) * 1000;

  select(0, NULL, NULL, NULL, &delay_time);
  return;
}

void test(RedisClient &client) {
  {
    std::cout << "Press <ENTER> to continue..." << std::endl;
    std::cin.get();

    RedisReply reply = client.redisCommand("SET %s %s", "key0", "value0");
    if (reply)
      std::cout << "SET: " << reply->str << std::endl;
    else
      std::cout << "SET: Something wrong." << std::endl;
  }

  sleep_awhile(1);

  {
    std::cout << "Press <ENTER> to continue..." << std::endl;
    std::cin.get();

    RedisReply reply = client.redisCommand("GET %s", "key0");
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
  RedisClient *client = (RedisClient *)arg;

  const string key = std::to_string(std::rand());
  const string value = std::to_string(std::rand());
  RedisReply reply =
      client->redisCommand("SET %s %s", key.c_str(), value.c_str());
  if (reply)
    std::cout << "SET: " << value << " OK" << std::endl;
  else
    std::cout << "SET: Something wrong." << std::endl;

  reply = client->redisCommand("GET %s", key.c_str());
  if (reply) {
    if (reply->type == REDIS_REPLY_NIL)
      std::cout << "GET: Key does not exist." << std::endl;
    else
      std::cout << "GET: " << reply->str << " OK" << std::endl;
  } else
    std::cout << "GET: "
              << "Something wrong." << std::endl;

  return NULL;
}

int main(int argc, char **argv) {
  int num_redis_socks = 1;
  int connect_timeout = 5000;           // ms
  int net_readwrite_timeout = 1000;     // ms
  int connect_failure_retry_delay = 1;  // seconds

  RedisEndpoint endpoints[] = {{"127.0.0.1", 6379, "", "slc360"},
                               {"127.0.0.1", 6379, "", "slc360"},
                               {"", 0, "/var/run/redis.sock", "slc360"}};

  RedisConfig config = {(RedisEndpoint *)&endpoints,
                        (sizeof(endpoints) / sizeof(RedisEndpoint)),
                        num_redis_socks,
                        connect_timeout,
                        net_readwrite_timeout,
                        connect_failure_retry_delay};

  RedisClient client(config);

  // while (1) test(client);

  int num_of_thread = num_redis_socks;
  pthread_t tid[num_of_thread];
  for (int i = 0; i < num_of_thread; i++) {
    pthread_create(&tid[i], NULL, run, &client);
  }

  for (int i = 0; i < num_of_thread; i++) {
    pthread_join(tid[i], NULL);
  }

  return 0;
}