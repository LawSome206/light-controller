#define BLINKER_WIFI
#define BLINKER_ESP_TASK
#define BLINKER_WIDGET

#include <Blinker.h>
#include <ctime>
#include <EEPROM.h>
#include "driver/ledc.h"

// 点灯科技密钥，wifi账号和密码
char auth[] = "b24cc43ef02e";
char ssid[] = "LZL";
char pswd[] = "luozhaolin";

// 定义GPIO
const u8_t key_pin = 33;
const u8_t pwmFanRead_pin = 14;
const u8_t pwmFanWrite_pin = 15;
const u8_t ntc0_pin = 34;
const u8_t ntc1_pin = 35;
const u8_t fan0_pin = 12;
const u8_t fan1_pin = 13;

// 定义各个LED对应的GPIO
const u8_t c1_pin = 23;
const u8_t c2_pin = 22;
const u8_t c3_pin = 21;
const u8_t c4_pin = 19;
const u8_t c5_pin = 18;
const u8_t c6_pin = 5;
const u8_t c7_pin = 4;
const u8_t c8_pin = 2;

// 定义使用的esp32 led pwm通道
const u8_t c1_channel = 0;
const u8_t c2_channel = 1;
const u8_t c3_channel = 2;
const u8_t c4_channel = 3;
const u8_t c5_channel = 4;
const u8_t c6_channel = 5;
const u8_t c7_channel = 6;
const u8_t c8_channel = 7;

// 定义PWM频率以及分辨率
#define freq 24000
#define resolution 8

/****** 新建组件对象 **********/
BlinkerNumber Number1("num-abc");
// 滑动条
BlinkerSlider slider_c1("slider_c1");
BlinkerSlider slider_c2("slider_c2");
BlinkerSlider slider_c3("slider_c3");
BlinkerSlider slider_c4("slider_c4");
BlinkerSlider slider_c5("slider_c5");
BlinkerSlider slider_c6("slider_c6");
BlinkerSlider slider_c7("slider_c7");
BlinkerSlider slider_c8("slider_c8");

// 按键
BlinkerButton button_sw_main("main-sw");
BlinkerButton button_time_select_add("time-select-add");
BlinkerButton button_time_select_sub("time-select-sub");
BlinkerButton button_refresh("refresh");
BlinkerButton button_save_setting("save-setting");
// 文本显示
BlinkerText text_time("tex-time");

int counter = 0;

// 函数声明
void refreshSlider(void);
void getNtpTime(void);
void calculatePwm(uint8_t inArrary[24][8], uint8_t outArrary[24][8][60]);
void writeSettingToEEPROM(void);
void readSettingFromEEPROM(void);

void printInArrary(uint8_t inArrary[24][8]);

struct time_tt {
  int8_t hour;
  int8_t min;
};

class data_t
{
public:
  // 构造函数
  data_t()
  {
    // 初始化数据
    this->setting_hour_index = 0;
    this->ledStatus = LOW;
    this->systemTime.hour = 0;
    this->systemTime.min = 0;
    for (uint8_t i = 0; i < 24; i++)
    {
      for (uint8_t j = 0; j < 8; j++)
      {
        hourLedData[i][j] = 0;
      }   
    }
  }

  // 记录一天24小时的数据，记录每个小时，8通道的数据
  uint8_t hourLedData[24][8];
  String hour_str[24] = {
      "00:00",
      "01:00",
      "02:00",
      "03:00",
      "04:00",
      "05:00",
      "06:00",
      "07:00",
      "08:00",
      "09:00",
      "10:00",
      "11:00",
      "12:00",
      "13:00",
      "14:00",
      "15:00",
      "16:00",
      "17:00",
      "18:00",
      "19:00",
      "20:00",
      "21:00",
      "22:00",
      "23:00",
  };
  uint8_t minPwmData[24][8][60]; // 存储每个时间点的pwm数据
  time_tt systemTime;
  uint8_t setting_hour_index; // 索引: 记录"设置"当前选择的时间
  uint8_t curr_hour_index; // 索引: 记录系统当前选择的时间，根具该索引提取灯光参数
  uint8_t curr_minute_index; // 索引: 记录系统当前选择的时间，根具该索引提取灯光参数
  bool ledStatus; // 灯的总开关
};


// 全局变量
data_t ctl_data;
TimerHandle_t myTimer1; // freerots定时器

// 心跳包函数
void heartbeat()
{

}

// "开关灯"按键 回调函数
void button_mainSwitch_callback(const String &state)
{
  BLINKER_LOG("get button state: ", state);
  if (state == "on")
  {
    ctl_data.ledStatus = HIGH;
    // 反馈开关状态
    button_sw_main.print("on");
  }
  else if (state == "off")
  {
    ctl_data.ledStatus = LOW;
    // 反馈开关状态
    button_sw_main.print("off");
  }
}
// "刷新数据"按键 回调函数
void button_refresh_callback(const String &state)
{
  if (ctl_data.ledStatus)
  {
    button_sw_main.print("on");
  }
  else
  {
    button_sw_main.print("off");
  }
  text_time.print(ctl_data.hour_str[ctl_data.setting_hour_index], "当前时间"); // 显示当前选择的时间
  refreshSlider();  // 刷新app滑动条数据

  /********测试代码*********/
  getNtpTime();
  calculatePwm(ctl_data.hourLedData, ctl_data.minPwmData);
  
  
  BLINKER_LOG("eeprom data************************");
  
}
// "刷新数据"按键 回调函数
void button_save_setting_callback(const String &state)
{
  writeSettingToEEPROM();
  readSettingFromEEPROM();
}

// 时间选择++按键 回调函数
void button_time_select_add_callback(const String &state)
{
  BLINKER_LOG("get button state: ", state);
  if(++ctl_data.setting_hour_index >= 24)
  {
    ctl_data.setting_hour_index = 0; // 超过24小时，索引归0
  }
  text_time.print(ctl_data.hour_str[ctl_data.setting_hour_index], "当前时间"); // 显示当前选择的时间
  // 刷新app滑动条数据
  refreshSlider();
}

// 时间选择--按键 回调函数
void button_time_select_sub_callback(const String &state)
{
  BLINKER_LOG("get button state: ", state);
  if (--ctl_data.setting_hour_index >= 24)
  {
    ctl_data.setting_hour_index = 23; // 超过24小时，索引归0
  }
  text_time.print(ctl_data.hour_str[ctl_data.setting_hour_index], "当前时间"); // 切换时间选择
  // 刷新app滑动条数据
  refreshSlider();
}

// 滑动条-通道1-回调函数
void slider_c1_callback(int32_t value)
{
  // 检查数据正确性
  if ((value < 0) || (value >255))
  {
    BLINKER_LOG("get error range value [0,255]: ", value);
    return;
  }
  // 写入通道1数据
  ctl_data.hourLedData[ctl_data.setting_hour_index][0] = value;
  BLINKER_LOG("get c1 value: ", value);
}

// 滑动条-通道2-回调函数
void slider_c2_callback(int32_t value)
{
  // 检查数据正确性
  if ((value < 0) || (value > 255))
  {
    BLINKER_LOG("get error range value [0,255]: ", value);
    return;
  }
  // 写入通道2数据
  ctl_data.hourLedData[ctl_data.setting_hour_index][1] = value;
  BLINKER_LOG("get c2 value: ", value);
}

// 滑动条-通道3-回调函数
void slider_c3_callback(int32_t value)
{
  // 检查数据正确性
  if ((value < 0) || (value > 255))
  {
    BLINKER_LOG("get error range value [0,255]: ", value);
    return;
  }
  // 写入通道3数据
  ctl_data.hourLedData[ctl_data.setting_hour_index][2] = value;
  BLINKER_LOG("get c3 value: ", value);
}

// 滑动条-通道4-回调函数
void slider_c4_callback(int32_t value)
{
  // 检查数据正确性
  if ((value < 0) || (value > 255))
  {
    BLINKER_LOG("get error range value [0,255]: ", value);
    return;
  }
  // 写入通道4数据
  ctl_data.hourLedData[ctl_data.setting_hour_index][3] = value;
  BLINKER_LOG("get c4 value: ", value);
}

// 滑动条-通道5-回调函数
void slider_c5_callback(int32_t value)
{
  // 检查数据正确性
  if ((value < 0) || (value > 255))
  {
    BLINKER_LOG("get error range value [0,255]: ", value);
    return;
  }
  // 写入通道4数据
  ctl_data.hourLedData[ctl_data.setting_hour_index][4] = value;
  BLINKER_LOG("get c5 value: ", value);
}

// 滑动条-通道6-回调函数
void slider_c6_callback(int32_t value)
{
  // 检查数据正确性
  if ((value < 0) || (value > 255))
  {
    BLINKER_LOG("get error range value [0,255]: ", value);
    return;
  }
  // 写入通道4数据
  ctl_data.hourLedData[ctl_data.setting_hour_index][5] = value;
  BLINKER_LOG("get c6 value: ", value);
}

// 滑动条-通道7-回调函数
void slider_c7_callback(int32_t value)
{
  // 检查数据正确性
  if ((value < 0) || (value > 255))
  {
    BLINKER_LOG("get error range value [0,255]: ", value);
    return;
  }
  // 写入通道4数据
  ctl_data.hourLedData[ctl_data.setting_hour_index][6] = value;
  BLINKER_LOG("get c7 value: ", value);
}

// 滑动条-通道8-回调函数
void slider_c8_callback(int32_t value)
{
  // 检查数据正确性
  if ((value < 0) || (value > 255))
  {
    BLINKER_LOG("get error range value [0,255]: ", value);
    return;
  }
  // 写入通道4数据
  ctl_data.hourLedData[ctl_data.setting_hour_index][7] = value;
  BLINKER_LOG("get c8 value: ", value);
}

// 如果未绑定的组件被触发，则会执行其中内容
void dataRead(const String &data)
{
  BLINKER_LOG("Blinker readString: ", data);
  counter++;
  Number1.print(counter);
}

// 刷新 app所有滑动条 数据
void refreshSlider(void)
{
  slider_c1.print(ctl_data.hourLedData[ctl_data.setting_hour_index][0]);
  slider_c2.print(ctl_data.hourLedData[ctl_data.setting_hour_index][1]);
  slider_c3.print(ctl_data.hourLedData[ctl_data.setting_hour_index][2]);
  slider_c4.print(ctl_data.hourLedData[ctl_data.setting_hour_index][3]);
  slider_c5.print(ctl_data.hourLedData[ctl_data.setting_hour_index][4]);
  slider_c6.print(ctl_data.hourLedData[ctl_data.setting_hour_index][5]);
  slider_c7.print(ctl_data.hourLedData[ctl_data.setting_hour_index][6]);
  slider_c8.print(ctl_data.hourLedData[ctl_data.setting_hour_index][7]);
}

// 将24小时，每个小时8通道的数据，计算出具体一天中每分钟的数据
void calculatePwm(uint8_t inArrary[24][8], uint8_t outArrary[24][8][60])
{
  // 每天24小时，按每小时之间计算
  for (uint8_t hour = 0; hour < 24; hour++)
  { 
    // 每个小时的每一个通道遍历计算
    for (uint8_t channel = 0; channel < 8; channel++)
    { 
      // 计算每个小时之间的pwm数值差
      float step = 0.0;
      if (hour == 23)
      {
        step = (inArrary[0][channel] - inArrary[23][channel]) / (60.0 - 1);
      }
      else
      {
        step = (inArrary[hour + 1][channel] - inArrary[hour][channel]) / (60.0 - 1);
      }
      // 具体到一小时的60分钟
      for (uint8_t min = 0; min < 60; min++)
      {
        uint8_t value = inArrary[hour][channel] + min * step;
        outArrary[hour][channel][min] = value;
      }
    } 
  } 
}

// 将灯光配置数据写入EEPROM
void writeSettingToEEPROM(void)
{
  EEPROM.begin(BLINKER_EEP_SIZE);
  EEPROM.put(3500, ctl_data.hourLedData);
  EEPROM.commit();
  EEPROM.end();
}

// 从EEPROM读取灯光数据
void readSettingFromEEPROM(void)
{
  // uint8_t testArrary[24][8];
  EEPROM.begin(BLINKER_EEP_SIZE);
  EEPROM.get(3500, ctl_data.hourLedData);
  EEPROM.end();
  // printInArrary(testArrary);
  BLINKER_LOG("save setting to EEPROM success...");
}

// 获取ntp时间
void getNtpTime(void)
{
  int8_t min = Blinker.minute();
  int8_t hour = Blinker.hour();
  if ((min == -1) || (hour == -1))
  {
    BLINKER_LOG("get ntp time error !!!");
    return;
  }
  else
  {
    ctl_data.systemTime.hour = hour;
    ctl_data.systemTime.min = min;
    BLINKER_LOG("current ntp time:", hour, ":", min);
  }
}

bool xyz = 0;
// 定义定时器回调函数
void myTimerCallback(TimerHandle_t xTimer)
{
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c1_channel, ctl_data.minPwmData[ctl_data.curr_hour_index][ctl_data.curr_minute_index][0]);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c2_channel, ctl_data.minPwmData[ctl_data.curr_hour_index][ctl_data.curr_minute_index][1]);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c3_channel, ctl_data.minPwmData[ctl_data.curr_hour_index][ctl_data.curr_minute_index][2]);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c4_channel, ctl_data.minPwmData[ctl_data.curr_hour_index][ctl_data.curr_minute_index][3]);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c5_channel, ctl_data.minPwmData[ctl_data.curr_hour_index][ctl_data.curr_minute_index][4]);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c6_channel, ctl_data.minPwmData[ctl_data.curr_hour_index][ctl_data.curr_minute_index][5]);
  // ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c7_channel, ctl_data.minPwmData[ctl_data.curr_hour_index][ctl_data.curr_minute_index][6]);
  // ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c8_channel, ctl_data.minPwmData[ctl_data.curr_hour_index][ctl_data.curr_minute_index][7]);

  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c1_channel);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c2_channel);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c3_channel);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c4_channel);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c5_channel);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c6_channel);
  // ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c7_channel);
  // ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c8_channel);
}

// arduino设置函数
void setup()
{
  // 初始化串口
  Serial.begin(115200);

  // 初始化驱动LED的PWM
  ledcSetup(c1_channel, freq, resolution);
  ledcSetup(c2_channel, freq, resolution);
  ledcSetup(c3_channel, freq, resolution);
  ledcSetup(c4_channel, freq, resolution);
  ledcSetup(c5_channel, freq, resolution);
  ledcSetup(c6_channel, freq, resolution);
  ledcSetup(c7_channel, freq, resolution);
  ledcSetup(c8_channel, freq, resolution);
  // 添加LED通道要被控的GPIO
  ledcAttachPin(c1_pin, c1_channel);
  ledcAttachPin(c2_pin, c2_channel);
  ledcAttachPin(c3_pin, c3_channel);
  ledcAttachPin(c4_pin, c4_channel);
  ledcAttachPin(c5_pin, c5_channel);
  ledcAttachPin(c6_pin, c6_channel);
  ledcAttachPin(c7_pin, c7_channel);
  ledcAttachPin(c8_pin, c8_channel);
  // 启用esp32 ledc渐变模式
  ledc_fade_func_install(0);
  Serial.printf("init ledc finish\n");

  // 开启blinker的debug模式
  BLINKER_DEBUG.stream(Serial);
  BLINKER_DEBUG.debugAll();
  // 初始化blinker
  Blinker.begin(auth, ssid, pswd);
  BLINKER_TAST_INIT(); // 创建blinker freertos任务
  Blinker.attachData(dataRead);
  slider_c1.attach(slider_c1_callback);
  slider_c2.attach(slider_c2_callback);
  slider_c3.attach(slider_c3_callback);
  slider_c4.attach(slider_c4_callback);
  slider_c5.attach(slider_c5_callback);
  slider_c6.attach(slider_c6_callback);
  slider_c7.attach(slider_c7_callback);
  slider_c8.attach(slider_c8_callback);
  button_sw_main.attach(button_mainSwitch_callback);
  button_refresh.attach(button_refresh_callback);
  button_time_select_add.attach(button_time_select_add_callback);
  button_time_select_sub.attach(button_time_select_sub_callback);
  button_save_setting.attach(button_save_setting_callback);
  Blinker.attachHeartbeat(heartbeat); // 注册一个心跳包
  Blinker.setTimezone(8.0); // 设定ntp时区

  myTimer1 = xTimerCreate("MyTimer", pdMS_TO_TICKS(3000), pdTRUE, (void *)0, myTimerCallback);
  // 启动定时器
  xTimerStart(myTimer1, 0);
}



// 循环函数
void loop()
{
  // Blinker.run();
  // refreshPwm();
  // ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c1_channel, 255, 10000);
  // ledc_fade_start(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c1_channel, LEDC_FADE_NO_WAIT);
  // vTaskDelay(pdMS_TO_TICKS(3000)); // delay 1000ms
  // ledc_fade_stop();
  // ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c1_channel, 0);
  // ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c1_channel);
  vTaskDelay(pdMS_TO_TICKS(3000)); // delay 1000ms

}

void printInArrary(uint8_t inArrary[24][8])
{
  for (int i = 0; i < 24; i++)
  {
    Serial.printf("current hour: %d\n", i);
    for (int j = 0; j < 8; j++)
    {
      Serial.printf("%d ", inArrary[i][j]);
    }
    Serial.printf("\n");
  }
}