#include <iostream>
#include <string>

#include "redis_client.h"

using std::cout;
using std::string;

int main(int argc, char** argv) {
  const RedisEndpoint endpoints[] = {{"127.0.0.1", 6379, "slc360"},
                                     {"/var/run/redis.sock", "slc360"}};
  RedisConfig config((const RedisEndpoint*)endpoints,
                     sizeof(endpoints) / sizeof(RedisEndpoint), 10, 10000, 5000,
                     1);
  RedisClient client(config);

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

  return 0;
}