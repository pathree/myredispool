
#include <string.h>

#include "redis_client.h"
#include "redis_utils.h"

int redis_set(const string &key, const string &value, int expire) {
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

int redis_get(const string &key, string &value, int del) {
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