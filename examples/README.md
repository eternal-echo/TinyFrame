# 架构

```
capsule_comm/
├── CMakeLists.txt                # 主CMake文件
├── TinyFrame.h
├── TinyFrame.c
├── TF_Config.h               # 自定义配置
├── examples/
│   ├── common.h                  # 共享定义(消息类型、协议等)
│   ├── tf_transport.cpp          # 使用pipe模拟外部收发器和胶囊的tinyframe的点对点通信底层依赖
│   ├── capsule.cpp               # 胶囊应用，通过mq和底层交互，通过tcp_transport和上层交互
│   └── transceiver.cpp           # 外部收发器应用，通过mq和底层交互，通过tcp_transport和上层交互
│    └── simulator.cpp             # 简单的传感器和控制模拟器
```

注意事项：
- capsule.cpp 和 transceiver.cpp 的代码需要搬运到嵌入式设备上，所以尽量使用posix接口和c语言代码。
- 其余代码可以尽可能使用现有的新特性或库的方式简单实现，力图高效简单

# 组件功能

1. **common.h**
   - 传感器数据结构定义(压力、IMU九轴)
   - 控制命令定义(开始/停止)
   - TinyFrame消息类型ID定义

2. **tf_transport.cpp**
   - 使用POSIX pipe实现TinyFrame的读写接口
   - 提供TF_WriteImpl()和读取函数
   - 实现互斥锁(可选)

3. **capsule.cpp**
   - 使用POSIX mqueue从队列读取传感器数据
   - 构造TinyFrame帧并通过transport发送
   - 接收并处理控制命令
   - 纯C/POSIX实现，便于移植到嵌入式设备

4. **transceiver.cpp**
   - 从TinyFrame接收传感器数据并写入消息队列
   - 从消息队列读取控制命令并通过TinyFrame发送
   - 纯C/POSIX实现，便于移植到嵌入式设备

5. **simulator.cpp**
   - 生成模拟传感器数据并写入消息队列
   - 提供发送控制命令的简单接口
