#include <iostream>
#include <string>

#include "redis_client.h"

using std::cout;
using std::string;

void test(RedisClient &client) {
  {
    std::cout << "Press <ENTER> to continue..." << std::endl;
    std::cin.get();

    RedisReplyPtr reply = client.redisCommand("SET %s %s", "key0", "value0");
    if (reply)
      std::cout << "SET: " << reply->str << std::endl;
    else
      std::cout << "SET: Something wrong." << std::endl;
  }

  {
    std::cout << "Press <ENTER> to continue..." << std::endl;
    std::cin.get();

    RedisReplyPtr reply = client.redisCommand("GET %s", "key0");
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
  RedisReplyPtr reply =
      client->redisCommand("SET %s %s", key.c_str(), value.c_str());
  if (reply)
    std::cout << "SET: " << value << std::endl;
  else
    std::cout << "SET: Something wrong." << std::endl;

  reply = client->redisCommand("GET %s", key.c_str());
  if (reply)
    if (reply->type == REDIS_REPLY_NIL)
      std::cout << "GET: Key does not exist." << std::endl;
    else {
      std::cout << "GET: " << reply->str << std::endl;
    }
  else
    std::cout << "GET: "
              << "Something wrong." << std::endl;

  return NULL;
}

int main(int argc, char **argv) {
  int num_redis_socks = 1;
  const RedisEndpoint endpoints[] = {
      {"127.0.0.1", 6379, "slc360"},
      //{"/var/run/redis.sock", "slc360"}
  };
  RedisConfig config((const RedisEndpoint *)endpoints,
                     sizeof(endpoints) / sizeof(RedisEndpoint), num_redis_socks,
                     10000, 5000, 1);
  RedisClient client(config);

  while (1) test(client);
  return 0;

  int num_of_thread = 1;
  pthread_t tid[num_of_thread];
  for (int i = 0; i < num_of_thread; i++) {
    pthread_create(&tid[i], NULL, run, &client);
  }

  for (int i = 0; i < num_of_thread; i++) {
    pthread_join(tid[i], NULL);
  }

  return 0;
}