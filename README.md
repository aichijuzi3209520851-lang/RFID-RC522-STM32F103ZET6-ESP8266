# RFID 考勤系统 — STM32 固件

基于 STM32F103ZET6、RC522、ESP8266 和 OLED 的 RFID 刷卡考勤硬件端固件。读取 RFID 卡号后通过串口发送给 Qt 上位机，同时通过 ESP8266 连接 ThingsCloud MQTT 平台上传打卡数据。

> 本仓库仅包含 STM32 固件源码。Qt 上位机请参考：[RFID-PC](https://github.com/aichijuzi3209520851-lang/RFID-PC)

## 功能

- 通过 RC522 读取 MIFARE Classic RFID 卡的 4 字节 UID
- 在 0.96 寸 OLED 上显示卡号和系统状态
- 通过 USART1 以 `CARD,<UID>` 格式向上位机发送刷卡事件
- 通过 ESP8266 连接 WiFi，以 AT-MQTT 协议连接 ThingsCloud，上报打卡 JSON 数据
- 卡片移除检测（连续 5 次读取失败后判定移除）

## 硬件平台

- **MCU**: STM32F103ZET6（72 MHz，512 KB Flash，64 KB SRAM）
- **开发板**: STM32 开拓者 2 号

## 硬件接线

### RC522 RFID 模块（SPI）

| RC522 引脚 | STM32 引脚 | 说明 |
| --- | --- | --- |
| SDA (CS) | PG7 | SPI 片选 |
| SCK | PB13 | SPI 时钟 |
| MOSI | PB15 | SPI 主出从入 |
| MISO | PB14 | SPI 主入从出 |
| RST | PG6 | 复位 |
| 3.3V | 3.3V | 供电 |
| GND | GND | 地 |

### ESP8266 WiFi 模块（UART4）

| ESP8266 引脚 | STM32 引脚 | 说明 |
| --- | --- | --- |
| TXD | PC11 (UART4_RX) | 接收 AT 指令响应 |
| RXD | PC10 (UART4_TX) | 发送 AT 指令 |
| VCC | 3.3V | 供电 |
| GND | GND | 地 |

波特率：`115200 8N1`

### USB-TTL 调试串口（USART1）

| USB-TTL 引脚 | STM32 引脚 | 说明 |
| --- | --- | --- |
| RXD | PA9 (USART1_TX) | 接收刷卡数据和日志 |
| TXD | PA10 (USART1_RX) | 可选，接收上位机指令 |
| GND | GND | 地 |

波特率：`115200 8N1`，无校验，无流控。

### OLED 显示屏

通过软件 I2C 或 SPI 连接，具体引脚定义见 `Hardware/OLED.h`。

## 串口输出协议

固件通过 USART1 发送以下格式的文本行：

```
SYS,BOOT
WIFI,JOINING
WIFI,OK
MQTT,JOINING
MQTT,OK
CARD,33EB4D05
CARD,REMOVED
```

Qt 上位机只处理 `CARD,<8位十六进制UID>` 格式，其余行仅显示在原始日志区。

### 卡片移除检测

采用轮询方式（50ms 间隔），连续 5 次读卡失败后判定卡片已移除，发送 `CARD,REMOVED`。

## ThingsCloud MQTT

固件通过 ESP8266 AT-MQTT 指令连接 ThingsCloud 平台：

| 配置 | 值 |
| --- | --- |
| MQTT Host | `sh-1-mqtt.iot-api.com` |
| MQTT Port | `1883` |
| Topic | `attributes` |
| Client ID | `rfid_stm32` |

读到新卡后上报 JSON 属性：

```json
{
  "rfid_uid": "33EB4D05",
  "rfid_employee_no": "UNKNOWN",
  "rfid_employee_name": "UNREGISTERED",
  "rfid_department": "RFID",
  "rfid_result": "CHECKIN",
  "rfid_seq": 1
}
```

> 当前 STM32 端不维护员工表，默认上传 `UNKNOWN` / `UNREGISTERED`。员工信息由 Qt 上位机管理。

## 项目结构

```
├── Project.uvprojx            # Keil uVision 5 工程文件
├── Hardware/                  # 硬件驱动
│   ├── RC522.c/.h             # RC522 RFID 读卡驱动（SPI）
│   ├── ESP8266.c/.h           # ESP8266 WiFi/MQTT 驱动（UART4 AT 指令）
│   ├── DebugSerial.c/.h       # 调试串口（USART1），提供 PrintCardID / ReadLine
│   ├── OLED.c/.h              # OLED 显示驱动
│   ├── OLED_Font.h            # OLED 字库
│   ├── Key.c/.h               # 按键驱动
│   └── LED.c/.h               # LED 驱动
├── User/                      # 用户代码
│   ├── main.c                 # 主循环：初始化 → 卡片轮询 → 串口发送 → MQTT 上报
│   ├── stm32f10x_conf.h       # 外设库配置
│   └── stm32f10x_it.c/.h      # 中断处理（USART1、UART4 IRQHandler）
├── Start/                     # 启动文件和系统头文件
│   ├── startup_stm32f10x_hd.s # 启动汇编（High Density）
│   ├── core_cm3.c/.h          # ARM Cortex-M3 核心支持
│   ├── stm32f10x.h            # STM32F10x 寄存器定义
│   └── system_stm32f10x.c/.h  # 系统初始化
├── System/                    # 系统工具
│   └── Delay.c/.h             # 毫秒延时（SysTick）
└── Library/                   # STM32 标准外设库（STM32F10x_StdPeriph）
```

## 构建与烧录

### 环境要求

- Keil uVision 5（MDK-ARM）
- 编译器：ARMCC V5（ARM Compiler 5）
- 目标芯片：STM32F103ZE（High Density）

### 构建步骤

1. 用 Keil 打开 `Project.uvprojx`
2. 选择目标芯片为 STM32F103ZE
3. 点击 **Build**（F7）或 **Rebuild** 编译
4. 编译成功后在 `Objects/` 目录下生成 `.hex` / `.bin` 文件

### 烧录

- 使用 ST-Link 或 J-Link 连接开发板
- 在 Keil 中点击 **Download** 烧录，或使用 STM32CubeProgrammer

## 主循环工作流程

```
初始化（OLED / RC522 / DebugSerial / ESP8266）
    ↓
发送 SYS,BOOT
    ↓
连接 WiFi（AT+CWJAP）
    ↓
连接 ThingsCloud MQTT（AT+MQTTCONN）
    ↓
┌──────────────────────────────────────────┐
│  主循环（50ms 轮询）                       │
│                                          │
│  1. 读取串口指令（MQTT,<json> → 发布）     │
│  2. 读取 RC522 卡号                        │
│     ├─ 新卡 → 打印 CARD,<UID>             │
│     └─ 移除 → 打印 CARD,REMOVED           │
│  3. 延时 50ms                             │
└──────────────────────────────────────────┘
```

## 调试

串口调试工具（如 SSCOM、串口调试助手）连接 USB-TTL，波特率 `115200`，可观察完整启动日志和刷卡事件：

```
SYS,BOOT
[ESP8266] >> AT
[ESP8266] << OK
WIFI,JOINING
[ESP8266] joining WiFi SSID: 666
WIFI,OK
MQTT,JOINING
[MQTT] ThingsCloud connected
MQTT,OK
CARD,33EB4D05
```
