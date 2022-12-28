#ifndef REDIS_UTILS_H_
#define REDIS_UTILS_H_

#include <string>

using std::string;

int redis_set(const string &key, const string &value, int expire = 0);
int redis_get(const string &key, string &value, int del = 0);

#endif  // REDIS_UTILS_H_
