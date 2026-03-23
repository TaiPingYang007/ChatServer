#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

class GroupUser : public User {
public:
  void setRole(std::string role) { this->role = role; }
  std::string getRole() const { return this->role; }

private:
  std::string role;
};

#endif