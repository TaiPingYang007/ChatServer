#ifndef GROUP_H
#define GROUP_H

#include "groupuser.hpp"
#include <string>
#include <vector>

class Group {
public:
  Group(int id = -1, std::string name = "", std::string desc = "") {
    this->m_Id = id;
    this->m_Name = name;
    this->m_Desc = desc;
  }

  void setId(int id) { this->m_Id = id; }
  void setName(std::string name) { this->m_Name = name; }
  void setDesc(std::string desc) { this->m_Desc = desc; }

  int getId() const { return this->m_Id; }
  std::string getName() const { return this->m_Name; }
  std::string getDesc() const { return this->m_Desc; }
  std::vector<GroupUser> &getUsers() { return this->users; }
  const std::vector<GroupUser> &getUsers() const { return this->users; }

private:
  int m_Id;
  std::string m_Name;
  std::string m_Desc;
  std::vector<GroupUser> users; // 存储组中的成员
};

#endif