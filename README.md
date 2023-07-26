# pressure_test
用于tcp服务器的压力测试

# 编译[需支持c++11]
cd bulid
cmake ..
make

# 配置文件[client.cof]
属性 = 数值

# 运行
./client ip port scale

# 安全关闭
kill -10 uid