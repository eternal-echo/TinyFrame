# 架构

```
capsule_comm/
├── CMakeLists.txt                # 主CMake文件
├── TinyFrame.h
├── TinyFrame.c
├── examples/
│   ├── common.h                  # 共享定义(消息类型、协议等)
│   ├── main.cpp                  # 使用boost的mq来模拟tinyframe的点对点主从通信底层依赖，初始化pressure和IMU传感器以及control命令的posix mq并模拟收发数据
│   ├── TF_Config.h               # 自定义配置
│   ├── tf_thread.cpp             # 实现tinyframe的应用逻辑，通过mq进行线程间通信
```

# 流程

- 流程：
    - 初始化 board_init：
        - 创建消息队列(`rf_tx_queue`, `rf_rx_queue`)
            - 【tinyframe和RF底层之间的MQ】创建posix的消息队列RF_TX_QUEUE_NAME和RF_RX_QUEUE_NAME，用于实现tinyframe和RF底层之间的消息通信，进行解耦方便我更换RF底层驱动。
        - 【RF底层驱动】rf_dev_handle = &rf_device;
            - 分配RF设备结构体内存
            - 设置操作函数集合(ops)
            - 初始化私有数据(priv)
    - board_start：
        - 创建pthread线程启动
    - _thread：
        - 初始化tinyframe，配置TF_AddTypeListener等
        - 持续运行：
            - rf_rx：
                - rf_device→rx_start: 启动异步接收
                - 若tf_rx_queue里有数据则写入到tinyframe里
            - rf_tx:
                - 若tf_tx_queue里有数据则rf_device→tx
    - main：
        - 初始化其他的依赖，比如模拟数据用的IMU_QUEUE_NAME、PRESSURE_QUEUE_NAME还有模拟控制指令用的CONTROL_QUEUE_NAME的这些mq。

# other
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


# 使用


现在可以使用以下方式发送数据：

1. 发送原始数据：
```bash
./tf_client -t raw:01,02,03,04
```

2. 发送IMU数据：
```bash
./tf_client -t imu:1.0,2.0,3.0,0.1,0.2,0.3,0.01,0.02,0.03
```

3. 发送压力数据：
```bash
./tf_client -t pressure:1013.25
```

4. 发送控制命令：
```bash
./tf_server -t cmd:start
./tf_server -t cmd:stop
```

这样所有的数据发送都统一在-t选项下，使用更简洁的命令格式。如果输入格式错误，会显示帮助信息。

已进行更改。