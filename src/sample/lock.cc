
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

class Widget {
 public:
  Widget() { std::cout << "Widget()" << std::endl; }
  ~Widget() { std::cout << "~Widget()" << std::endl; }
};

class Lock {
 public:
  explicit Lock(std::mutex *mtx)
      : mtx_(mtx, [](std::mutex *mtx) { mtx->unlock(); }),
        /*为成员变量mtx_带上删除器，否则Lock析构的时候会调用默认删除器释放mtx_内存*/
        w_(new Widget,
           [](Widget *w) { delete w; }) /*等同于成员变量w_使用默认删除器*/
  {
    mtx_.get()->lock();
  }

  ~Lock() {
    std::cout << "~Lock()" << std::endl;
    // pm_.get()->unlock();
  }

 private:
  // mutex *mtx_; /*不使用共享指针std::shared_ptr，但是需要预防copy函数*/
  std::shared_ptr<std::mutex> mtx_;
  // Widget *w_; /*Widget只是用来说明std::shared_ptr会在析构的时候释放*/
  std::shared_ptr<Widget> w_;
};

void sleep_awhile(int ms) {
  struct timeval delay_time;

  memset(&delay_time, 0, sizeof(struct timeval));
  delay_time.tv_sec += ms / 1000;
  delay_time.tv_usec = (ms % 1000) * 1000;

  select(0, NULL, NULL, NULL, &delay_time);
  return;
}

std::mutex mtx;
Lock lock(&mtx);

void print_block(int n, char c) {
  Lock lock_t(lock);
  for (int i = 0; i < n; ++i) {
    sleep_awhile(1);
    std::cout << c;
  }
  std::cout << '\n';
}

int run_lock() {
  std::thread th1(print_block, 50, '1');
  std::thread th2(print_block, 50, '2');
  std::thread th3(print_block, 50, '3');

  th1.join();
  th2.join();
  th3.join();
  return 0;
}
