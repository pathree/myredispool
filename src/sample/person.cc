#include "person.h"

Person::Person(const std::string &name) : name_(name), sirname_("kkk") {}
std::string Person::name() const { return name_ + sirname_; }