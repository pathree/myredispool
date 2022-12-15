
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

int run_lock();
int run_person();

int main(int argc, char const *argv[]) {
  // run_lock();

  run_person();
  return 0;
}
