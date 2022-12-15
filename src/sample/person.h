#include <string>

class Person {
 public:
  Person(const std::string &name);
  std::string name() const;

 private:
  int age;
  std::string name_;
  std::string sirname_;
};