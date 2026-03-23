#include <nlohmann/json.hpp> // 正确：让编译器去 /usr/include 里找
using json = nlohmann::json;

#include <iostream>
#include <map>
#include <string>
#include <vector>

// json序列化示例1
/*
  json就是一个个的键值对
  jump可以将键值对序列化成字符串
  但是发送的时候需要char* 类型
*/
std::string func1() {
  json js;
  //[键] = 值;
  js["msg_type"] = 2;
  js["from"] = "zhang san";
  js["to"] = "li si";
  js["msg"] = "hello ,what are you doing now？";

  // dump将数据系列化
  std::string sendbuf = js.dump();

  //{"from":"zhang san","msg":"hello ,what are you doing
  //now？","msg_type":2,"to":"li si"}
  // std::cout << sendbuf.c_str() << "\n"; //
  // 在网络发送需要char* 的数据类型

  return sendbuf;
}

// json序列化示例2
/*
  json的值可以有很多种类型，如数组，字符串，同样的json对象也可以作为值
*/
std::string func2() {
  json js;

  // 添加数组
  js["id"] = {1, 2, 3, 4, 5};
  // 添加key - value
  js["name"] = "zhang san";

  // 添加对象
  js["msg"]["zhang san"] = "hello world"; // key - key - value
  js["msg"]["liu shuo"] = "hello China";  // key - key - value
  // 上面等同于下面这句一次性添加数组对象
  js["msg"] = {{"zhang san", "hello world"}, {"liu shuo", "hello China"}};

  std::string sendbuf = js.dump();

  // std::cout << sendbuf.c_str() << "\n";
  return sendbuf;
}

// json序列化代码示例3
/*
  json可以序列化容器
*/
std::string func3() {
  json js;

  std::vector<int> vec;

  vec.push_back(1);
  vec.push_back(2);
  vec.push_back(5);

  js["list"] = vec;

  std::map<int, std::string> m;
  m.insert({1, "黄山"});
  m.insert({2, "华山"});
  m.insert({3, "泰山"});

  js["path"] = m;

  std::string sendbuf = js.dump(); // 将数据对象转成数据字符串，数据序列化

  //{"list":[1,2,5],"path":[[1,"黄山"],[2,"华山"],[3,"泰山"]]
  // std::cout << sendbuf.c_str() << "\n";

  return sendbuf;
}

int main() {

  // 反序列化1
  std::string recvBuf1 = func1();
  json jsbuf1 = json::parse(recvBuf1);

  std::cout << jsbuf1["msg_type"] << "\n";
  std::cout << jsbuf1["from"] << "\n";
  std::cout << jsbuf1["to"] << "\n";
  std::cout << jsbuf1["msg"] << "\n";

  // 反序列化2
  std::string recvBuf2 = func2();
  json jsbuf2 = json::parse(recvBuf2);

  std::cout << jsbuf2["id"] << "\n";
  std::cout << jsbuf2["name"] << "\n";
  std::cout << jsbuf2["msg"] << "\n";

  auto arr = jsbuf2["id"];
  std::cout << arr << "\n";
  auto n = jsbuf2["id"][3];
  std::cout << n << "\n";

  auto masg = jsbuf2["msg"];
  std::cout << masg["zhang san"] << "\n";
  std::cout << masg["liu shuo"] << "\n";

  // 反序列化3
  std::string recvBuf3 = func3();
  json jsbuf3 = json::parse(recvBuf3);

  std::vector<int> list = jsbuf3["list"];
  for (auto &x : list) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  std::map<int, std::string> mp = jsbuf3["path"];
  for (auto &p : mp) {
    std::cout << p.first << " " << p.second << "\n";
  }
  std::cout << "\n";

  return 0;
}