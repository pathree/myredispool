#include <iostream>
#include <string>

#include "person.h"

int run_person() {
  Person p("dang");
  std::cout << p.name() << std::endl;

  return 0;
}