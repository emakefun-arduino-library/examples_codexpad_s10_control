/**
 * @~Chinese
 * @brief 使用CodexPad-S10手柄控制4个编码电机。
 * @details 通过手柄按钮控制4个编码电机的运行状态：
 *          - 上方向按钮（按住）：4个电机同时正转（PWM占空比 1023）。
 *          - 下方向按钮（按住）：4个电机同时反转（PWM占空比 -1023）。
 *          - Cross按钮（按住）：刹车（调用Stop方法），刹车优先级最高。
 *          - 无按钮输入时，所有电机停止（PWM占空比 0）。
 *
 * @note 调试说明：
 *       - 硬件串口（D0/D1）已被蓝牙模块占用，无法用于调试输出。
 *       - SoftwareSerial软串口与Emakefun Motor Driver库存在冲突，无法同时使用。
 *       - 因此本示例不包含任何串口调试输出，如需调试建议使用LED指示灯或蜂鸣器等方式。
 */
/**
 * @~English
 * @brief Control 4 encoder motors using CodexPad-S10 gamepad.
 * @details Control the running state of 4 encoder motors via gamepad buttons:
 *          - Up button (holding): All 4 motors run forward (PWM duty 1023).
 *          - Down button (holding): All 4 motors run backward (PWM duty -1023).
 *          - Cross button (holding): Brake (call Stop method), brake has the highest priority.
 *          - No button input: All motors stop (PWM duty 0).
 *
 * @note Debugging Notes:
 *       - The hardware serial (D0/D1) is occupied by the Bluetooth module and cannot be used for debug output.
 *       - The SoftwareSerial soft serial port conflicts with the Emakefun Motor Driver library and cannot be used simultaneously.
 *       - Therefore, this example does not include any serial debug output. For debugging, it is recommended to use LED indicators or buzzers
 * instead.
 */

#include "Emakefun_MotorDriver.h"
#include "codex_pad_frame_decoder.h"

namespace {
// 替换为你的 CodexPad 的 Bluetooth device address。
// Replace with your CodexPad device's Bluetooth device address.
const String kBluetoothDeviceAddress = "16:00:00:00:03:27";

// 与蓝牙模块通信的串口波特率（请查阅模块数据手册，替换为模块对应的波特率）。
// The serial port baud rate for communication with the Bluetooth module (please refer to the module data manual, replace with the corresponding baud
// rate for the module).
constexpr uint32_t kBluetoothModuleSerialBaudRate = 115200;

constexpr uint8_t kMotorDriverI2CAddress = 0x60;  // 驱动板I2C地址。 | Driver board I2C address.
constexpr uint8_t kPwmFrequency = 50;             // PWM频率，单位 Hz。 | PWM frequency in Hz.

constexpr uint8_t kMotorRunSpeed = 255;           // 电机运行速度 (0~255) | Motor running speed (0~255).
constexpr uint32_t kMotorUpdateIntervalMs = 100;  // 更新间隔（毫秒） | Update interval in milliseconds.

uint32_t g_motor_last_update_time = 0;

// 驱动板实例。 | Driver board instance.
Emakefun_MotorDriver g_motor_driver(kMotorDriverI2CAddress);

// 四个编码电机对象。 | Four encoder motor objects.
Emakefun_EncoderMotor *g_encoder_motor_0 = g_motor_driver.getEncoderMotor(E1);
Emakefun_EncoderMotor *g_encoder_motor_1 = g_motor_driver.getEncoderMotor(E2);
Emakefun_EncoderMotor *g_encoder_motor_2 = g_motor_driver.getEncoderMotor(E3);
Emakefun_EncoderMotor *g_encoder_motor_3 = g_motor_driver.getEncoderMotor(E4);

// 此处解码器传入的串口实例，与 Connect() 中传入的为同一个串口实例。
// The serial instance passed in by the decoder here is the same as the one passed in Connect().
CodexPadFrameDecoder g_codex_pad_frame_decoder(Serial);

/**
 * @~Chinese
 * @brief 向蓝牙模块发送 AT 指令以建立 BLE 连接。
 * @details 依次执行 AT 指令序列，将蓝牙模块配置为主机模式并连接到指定 MAC 地址的目标设备。
 *          指令顺序：AT+DISCON → AT+RESET → AT+ECHO=0 → AT+ROLE=0 → AT+AUTOCON=0 → AT+CON=<mac>。
 * @param[in] bluetooth_stream 连接蓝牙模块的 Stream 对象。
 *                             注意：此对象必须与传入 CodexPadFrameDecoder 的 Stream 是同一个实例（如 Serial），
 *                             因为解码器正是从这同一个串口读取手柄发来的数据。
 * @param[in] bluetooth_device_address 目标设备的 MAC 地址，格式为 "XX:XX:XX:XX:XX:XX"。
 */
/**
 * @~English
 * @brief Send AT commands to the Bluetooth module to establish a BLE connection.
 * @details Executes a sequence of AT commands to configure the Bluetooth module as master and connect to the target device
 *          at the specified MAC address.
 *          Command order: AT+DISCON → AT+RESET → AT+ECHO=0 → AT+ROLE=0 → AT+AUTOCON=0 → AT+CON=<mac>.
 * @param[in] bluetooth_stream Stream connected to the Bluetooth module.
 *                             Note: This must be the same Stream instance that is passed to the CodexPadFrameDecoder
 *                             (e.g., Serial), as the decoder reads incoming data from this same serial port.
 * @param[in] bluetooth_device_address MAC address of the target device in format "XX:XX:XX:XX:XX:XX".
 */
void Connect(Stream &bluetooth_stream, const String &bluetooth_device_address) {
  if (bluetooth_device_address.length() != 17 || bluetooth_device_address[2] != ':' || bluetooth_device_address[5] != ':' ||
      bluetooth_device_address[8] != ':' || bluetooth_device_address[11] != ':' || bluetooth_device_address[14] != ':') {
    while (true);
  }

  // 模块可能处于连接状态，先发送断开指令，确保模块是未连接状态。
  // The module may be in a connected state. Send the disconnection command first to ensure the module is in an unconnected state.
  bluetooth_stream.println("AT+DISCON");
  delay(100);

  // 软件复位蓝牙芯片，清除所有配对和配置数据。
  // Software reset BLE chip, clear all pairing and configuration data.
  bluetooth_stream.println("AT+RESET");
  delay(100);

  // 关闭AT信息回显。
  // Close AT information echo.
  bluetooth_stream.println("AT+ECHO=0");
  delay(100);

  // 设置模块为主机模式，使其能够主动连接从机蓝牙。
  // Set the module to host mode so that it can actively connect to the BLE of the slave device.
  bluetooth_stream.println("AT+ROLE=0");
  delay(100);

  // 关闭模块的蓝牙自动连接模式。
  // Disable the module's automatic Bluetooth connection mode.
  bluetooth_stream.println("AT+AUTOCON=0");
  delay(100);

  // 使用指定的MAC地址发起与从机蓝牙连接。
  // Initiate BLE connection with the slave using the specified MAC address.
  bluetooth_stream.print("AT+CON=");
  bluetooth_stream.println(bluetooth_device_address);
  delay(100);
}
}  // namespace

void setup() {
  g_motor_driver.begin(kPwmFrequency);

  g_encoder_motor_0->run(BRAKE);
  g_encoder_motor_1->run(BRAKE);
  g_encoder_motor_2->run(BRAKE);
  g_encoder_motor_3->run(BRAKE);

  Serial.begin(kBluetoothModuleSerialBaudRate);
  Connect(Serial, kBluetoothDeviceAddress);
}

void loop() {
  // 重要：Update()方法必须在循环中尽可能频繁地调用，不能添加延时。
  // 该方法负责处理所有接收到的蓝牙数据包，延时会导致数据丢失和响应延迟。
  // 对于实时控制应用，必须保持高频率调用以确保及时响应手柄输入。
  // Important: Update() method must be called as frequently as possible in the loop, no delays should be added.
  // This method processes all received Bluetooth packets, delays will cause data loss and response lag.
  // For real-time control applications, high-frequency calls are essential to ensure prompt response to gamepad input.
  g_codex_pad_frame_decoder.Update();

  if (millis() - g_motor_last_update_time >= kMotorUpdateIntervalMs) {
    if (g_codex_pad_frame_decoder.holding(CodexPadFrameDecoder::Button::kCrossA)) {
      // Cross按钮：刹车 | Cross button: brake.
      g_encoder_motor_0->run(BRAKE);
      g_encoder_motor_1->run(BRAKE);
      g_encoder_motor_2->run(BRAKE);
      g_encoder_motor_3->run(BRAKE);
      g_motor_last_update_time = millis();
    } else if (g_codex_pad_frame_decoder.holding(CodexPadFrameDecoder::Button::kUp) &&
               !g_codex_pad_frame_decoder.holding(CodexPadFrameDecoder::Button::kDown)) {
      // 上方向键：电机正转 | Up button: move forward.
      g_encoder_motor_0->setSpeed(kMotorRunSpeed);
      g_encoder_motor_0->run(FORWARD);
      g_encoder_motor_1->setSpeed(kMotorRunSpeed);
      g_encoder_motor_1->run(FORWARD);
      g_encoder_motor_2->setSpeed(kMotorRunSpeed);
      g_encoder_motor_2->run(FORWARD);
      g_encoder_motor_3->setSpeed(kMotorRunSpeed);
      g_encoder_motor_3->run(FORWARD);
      g_motor_last_update_time = millis();
    } else if (g_codex_pad_frame_decoder.holding(CodexPadFrameDecoder::Button::kDown) &&
               !g_codex_pad_frame_decoder.holding(CodexPadFrameDecoder::Button::kUp)) {
      // 下方向键：电机反转 | Down button: move backward.
      g_encoder_motor_0->setSpeed(kMotorRunSpeed);
      g_encoder_motor_0->run(BACKWARD);
      g_encoder_motor_1->setSpeed(kMotorRunSpeed);
      g_encoder_motor_1->run(BACKWARD);
      g_encoder_motor_2->setSpeed(kMotorRunSpeed);
      g_encoder_motor_2->run(BACKWARD);
      g_encoder_motor_3->setSpeed(kMotorRunSpeed);
      g_encoder_motor_3->run(BACKWARD);
      g_motor_last_update_time = millis();
    } else {
      // 无输入：刹车 | No input: brake.
      g_encoder_motor_0->run(BRAKE);
      g_encoder_motor_1->run(BRAKE);
      g_encoder_motor_2->run(BRAKE);
      g_encoder_motor_3->run(BRAKE);
      g_motor_last_update_time = millis();
    }
  }
}