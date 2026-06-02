/**
 * @~Chinese
 * @brief 使用CodexPad-S10手柄控制4个直流电机。
 * @details 通过手柄按钮控制4个直流电机的运行状态：
 *          - 上方向按钮（按住）：4个电机同时正转（PWM占空比 1023）。
 *          - 下方向按钮（按住）：4个电机同时反转（PWM占空比 -1023）。
 *          - Cross按钮（按住）：刹车（调用Stop方法），刹车优先级最高。
 *          - 无按钮输入时，所有电机停止（PWM占空比 0）。
 */
/**
 * @~English
 * @brief Control 4 DC motors using CodexPad-S10 gamepad.
 * @details Control the running state of 4 DC motors via gamepad buttons:
 *         - Up button (holding): All 4 motors run forward (PWM duty 1023).
 *         - Down button (holding): All 4 motors run backward (PWM duty -1023).
 *         - Cross button (holding): Brake (call Stop method), brake has the highest priority.
 *         - No button input: All motors stop (PWM duty 0).
 */

#include "codex_pad.h"
#include "motor.h"

namespace {
// 替换为你的 CodexPad 的 Bluetooth device address。
// Replace with your CodexPad device's Bluetooth device address.
const std::string kBluetoothDeviceAddress = "16:00:00:00:03:27";

constexpr gpio_num_t kMotor0PinPositivePin = GPIO_NUM_27;  // 电机0正极引脚。 | Motor 0 positive pin.
constexpr gpio_num_t kMotor0PinNegativePin = GPIO_NUM_13;  // 电机0负极引脚。 | Motor 0 negative pin.

constexpr gpio_num_t kMotor1PinPositivePin = GPIO_NUM_4;  // 电机1正极引脚。 | Motor 1 positive pin.
constexpr gpio_num_t kMotor1PinNegativePin = GPIO_NUM_2;  // 电机1负极引脚。 | Motor 1 negative pin.

constexpr gpio_num_t kMotor2PinPositivePin = GPIO_NUM_17;  // 电机2正极引脚。 | Motor 2 positive pin.
constexpr gpio_num_t kMotor2PinNegativePin = GPIO_NUM_12;  // 电机2负极引脚。 | Motor 2 negative pin.

constexpr gpio_num_t kMotor3PinPositivePin = GPIO_NUM_15;  // 电机3正极引脚。 | Motor 3 positive pin.
constexpr gpio_num_t kMotor3PinNegativePin = GPIO_NUM_14;  // 电机3负极引脚。 | Motor 3 negative pin.

constexpr uint32_t kMotorUpdateIntervalMs = 100;  // 电机更新间隔时间，单位毫秒。 | Motor update interval in milliseconds.

constexpr uint32_t kConnectionTimeoutMs = 5000;  // 连接手柄超时时间，单位毫秒。 | Connection timeout for gamepad in milliseconds.

uint32_t g_motor_last_update_time = 0;

// 四个电机对象。 | Four motor objects.
em::Motor g_motor_0(kMotor0PinPositivePin, kMotor0PinNegativePin);
em::Motor g_motor_1(kMotor1PinPositivePin, kMotor1PinNegativePin);
em::Motor g_motor_2(kMotor2PinPositivePin, kMotor2PinNegativePin);
em::Motor g_motor_3(kMotor3PinPositivePin, kMotor3PinNegativePin);

CodexPad g_codex_pad;

void Connect() {
  printf("Start to connect %s\n", kBluetoothDeviceAddress.c_str());
  // 连接到指定蓝牙设备地址的手柄。
  // Connect to the CodexPad with specified Bluetooth device address.
  while (!g_codex_pad.Connect(kBluetoothDeviceAddress, kConnectionTimeoutMs)) {
    printf("Retry to connect %s\n", kBluetoothDeviceAddress.c_str());
  }

  if (const auto ble_client = g_codex_pad.ble_client(); ble_client != nullptr) {
    printf("Remote Bluetooth Device Address: %s\n", ble_client->getPeerAddress().toString().c_str());
  } else {
    printf("Remote Bluetooth Device Address: unknown.\n");
  }

  // 设置发射功率为0dBm。
  // 发射功率影响通信距离和功耗：功率越高，通信距离越远，但功耗也越大。
  // 建议根据实际应用场景选择合适的功率等级以平衡距离和电池寿命。
  // Set transmission power to 0dBm.
  // Transmission power affects communication range and power consumption:
  // Higher power provides longer range but consumes more battery.
  // Choose appropriate power level based on your application to balance range and battery life.
  if (g_codex_pad.set_remote_tx_power(CodexPad::TxPower::k0dBm)) {
    printf("Set remote tx power to 0dBm successfully.\n");
  }

  printf("Connected.\n");
}

void MotorInit() {
  printf("Motor driver init.\n");
  g_motor_0.Init();
  g_motor_1.Init();
  g_motor_2.Init();
  g_motor_3.Init();

  g_motor_0.RunPwmDuty(0);
  g_motor_1.RunPwmDuty(0);
  g_motor_2.RunPwmDuty(0);
  g_motor_3.RunPwmDuty(0);
}
}  // namespace

void setup() {
  MotorInit();

  printf("CodexPad Init.\n");
  g_codex_pad.Init();

  Connect();
}

void loop() {
  // 重要：Update()方法必须在循环中尽可能频繁地调用，不能添加延时。
  // 该方法负责处理所有接收到的蓝牙数据包，延时会导致数据丢失和响应延迟。
  // 对于实时控制应用，必须保持高频率调用以确保及时响应手柄输入。
  // Important: Update() method must be called as frequently as possible in the loop, no delays should be added.
  // This method processes all received Bluetooth packets, delays will cause data loss and response lag.
  // For real-time control applications, high-frequency calls are essential to ensure prompt response to gamepad input.
  g_codex_pad.Update();

  if (!g_codex_pad.is_connected()) {
    printf("Disconnected, start to reconnect.\n");
    Connect();
    return;
  }

  if (millis() - g_motor_last_update_time >= kMotorUpdateIntervalMs) {
    if (g_codex_pad.holding(CodexPad::Button::kCrossA)) {
      // Cross按钮：刹车 | Cross button: brake.
      printf("Brake.\n");
      g_motor_0.Stop();
      g_motor_1.Stop();
      g_motor_2.Stop();
      g_motor_3.Stop();
      g_motor_last_update_time = millis();
    } else if (g_codex_pad.holding(CodexPad::Button::kUp) && !g_codex_pad.holding(CodexPad::Button::kDown)) {
      // 上方向键：电机正转 | Up button: move forward.
      printf("Forward.\n");
      g_motor_0.RunPwmDuty(em::Motor::kMaxPwmDuty);
      g_motor_1.RunPwmDuty(em::Motor::kMaxPwmDuty);
      g_motor_2.RunPwmDuty(em::Motor::kMaxPwmDuty);
      g_motor_3.RunPwmDuty(em::Motor::kMaxPwmDuty);
      g_motor_last_update_time = millis();
    } else if (g_codex_pad.holding(CodexPad::Button::kDown) && !g_codex_pad.holding(CodexPad::Button::kUp)) {
      // 下方向键：电机反转 | Down button: move backward.
      printf("Reverse.\n");
      g_motor_0.RunPwmDuty(-em::Motor::kMaxPwmDuty);
      g_motor_1.RunPwmDuty(-em::Motor::kMaxPwmDuty);
      g_motor_2.RunPwmDuty(-em::Motor::kMaxPwmDuty);
      g_motor_3.RunPwmDuty(-em::Motor::kMaxPwmDuty);
      g_motor_last_update_time = millis();
    } else {
      // 无输入：停止 | No input: stop.
      printf("Stop.\n");
      g_motor_0.RunPwmDuty(0);
      g_motor_1.RunPwmDuty(0);
      g_motor_2.RunPwmDuty(0);
      g_motor_3.RunPwmDuty(0);
      g_motor_last_update_time = millis();
    }
  }
}