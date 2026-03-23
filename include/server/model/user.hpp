#ifndef USER_H
#define USER_H

#include <string>

// User表的ORM类，映射表的字段
class User {
public:
  User(int id = -1, std::string name = "", std::string pwd = "",
       std::string state = "offline")
      : id(id), name(name), password(pwd), state(state) {}

  void setId(int id) { this->id = id; }
  void setName(std::string name) { this->name = std::move(name); }
  void setPassword(std::string pwd) { this->password = std::move(pwd); }
  void setState(std::string state) { this->state = std::move(state); }

  int getId() const { return this->id; }
  std::string getName() const { return this->name; }
  std::string getPassword() const { return this->password; }
  std::string getState() const { return this->state; }

protected:
  int id;
  std::string name;
  std::string password;
  std::string state;
};

#endif