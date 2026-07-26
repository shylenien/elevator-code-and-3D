/*************************************************************
  Download latest ERa library here:
    https://github.com/eoh-jsc/era-lib/releases/latest
    https://www.arduino.cc/reference/en/libraries/era
    https://registry.platformio.org/libraries/eoh-ltd/ERa/installation

    ERa website:                https://e-ra.io
    ERa blog:                   https://iotasia.org
    ERa forum:                  https://forum.eoh.io
    Follow us:                  https://www.fb.com/EoHPlatform
 *************************************************************/

// Enable debug console
y #define ERA_DEBUG

/* Define MQTT host */
#define DEFAULT_MQTT_HOST "mqtt1.eoh.io"

// You should get Auth Token in the ERa App or ERa Dashboard
#define ERA_AUTH_TOKEN "dc493a95-8c77-4b56-bf57-ed5825be7436"

#include <Arduino.h>
#include <ERa.hpp>
#include <ModbusMaster.h> // Bắt buộc phải cài thư viện này

    const char ssid[] = "Hung Pham";
const char pass[] = "00851855";

#define RX_PIN 16   // Chân nhận dữ liệu (RO)
#define TX_PIN 17   // Chân truyền dữ liệu (DI)
#define DE_RE_PIN 4 // Chân điều khiển luồng (DE & RE)

HardwareSerial ModbusSerial(2);
ModbusMaster node; // Khai báo đối tượng Modbus

// =================================================================
// 1. CẤU HÌNH DANH SÁCH CÁC THANH GHI CẦN ĐỌC TỪ S7-1200
// =================================================================
struct ModbusTask
{
    uint8_t slaveID;         // Địa chỉ của PLC S7-1200 (ví dụ: 1)
    uint16_t startAddress;   // Thanh ghi bắt đầu đọc (ví dụ: 0 tương ứng 40001)
    uint16_t length;         // Số lượng thanh ghi cần đọc (ví dụ: 2)
    uint8_t startDatastream; // ID của Datastream (Virtual Pin) trên E-Ra (ví dụ: 0)
};

// Bạn có thể thêm/bớt thanh ghi tùy ý vào danh sách này
ModbusTask taskList[] = {
    // Đọc Slave 1, từ thanh ghi 0 (40001), lấy 2 giá trị -> Đẩy lên E-Ra Datastream V0 và V1
    {1, 0, 2, 0}};

const int taskCount = sizeof(taskList) / sizeof(taskList[0]);

// =================================================================
// 2. HÀM KIỂM SOÁT LUỒNG DỮ LIỆU MAX485
// =================================================================
void preTransmission()
{
    digitalWrite(DE_RE_PIN, HIGH); // Bật chế độ truyền
}

void postTransmission()
{
    digitalWrite(DE_RE_PIN, LOW); // Bật chế độ nhận
}

// =================================================================
// 3. CÁC HÀM CỦA HỆ THỐNG ERA
// =================================================================
#if defined(ERA_AUTOMATION)
#include <Automation/ERaSmart.hpp>
#if defined(ESP32) || defined(ESP8266)
#include <Time/ERaEspTime.hpp>
ERaEspTime syncTime;
#else
#define USE_BASE_TIME
#include <Time/ERaBaseTime.hpp>
ERaBaseTime syncTime;
#endif
ERaSmart smart(ERa, syncTime);
#endif

ERA_CONNECTED()
{
    ERA_LOG(ERA_PSTR("ERa"), ERA_PSTR("ERa connected!"));
}

ERA_DISCONNECTED()
{
    ERA_LOG(ERA_PSTR("ERa"), ERA_PSTR("ERa disconnected!"));
}

/* Hàm này sẽ được E-Ra gọi mỗi giây để in ra thời gian chạy */
void timerEvent()
{
    ERA_LOG(ERA_PSTR("Timer"), ERA_PSTR("Uptime: %d"), ERaMillis() / 1000L);
}

// =================================================================
// 4. HÀM TỰ ĐỘNG ĐỌC MODBUS VÀ ĐẨY LÊN E-RA
// =================================================================
void readModbusData()
{
    for (int i = 0; i < taskCount; i++)
    {
        // Cập nhật Slave ID
        node.begin(taskList[i].slaveID, ModbusSerial);

        // Gọi lệnh đọc Function 03 (Holding Registers)
        uint8_t result = node.readHoldingRegisters(taskList[i].startAddress, taskList[i].length);

        if (result == node.ku8MBSuccess)
        {
            // Nếu đọc thành công, tách dữ liệu và đẩy lên E-Ra
            for (int j = 0; j < taskList[i].length; j++)
            {
                int value = node.getResponseBuffer(j);
                int currentDatastream = taskList[i].startDatastream + j;

                // Cập nhật dữ liệu lên Web/App E-Ra
                ERa.virtualWrite(currentDatastream, value);

#if defined(ERA_DEBUG)
                Serial.printf("Thanh ghi %d -> V%d: %d\n", taskList[i].startAddress + j, currentDatastream, value);
#endif
            }
        }
        else
        {
#if defined(ERA_DEBUG)
            Serial.printf("Lỗi đọc Modbus Slave %d, Mã lỗi: 16#%X\n", taskList[i].slaveID, result);
#endif
        }

        // Trễ một chút giữa các lần hỏi để module MAX485 không bị nghẽn
        delay(50);
    }
}

// =================================================================
// 5. HÀM SETUP & LOOP
// =================================================================
void setup()
{
#if defined(ERA_DEBUG)
    Serial.begin(115200);
#endif

    // 5.1 Cấu hình phần cứng MAX485
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);

    // 5.2 Khởi tạo cổng Serial2 cho Modbus (Baudrate phải khớp với PLC)
    ModbusSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    // 5.3 Đăng ký callback cho ModbusMaster để tự động bật tắt DE/RE
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    /* Set scan WiFi */
    ERa.setScanWiFi(true);

    /* Khởi tạo ERa với WiFi và Token */
    ERa.begin(ssid, pass);

    /* Tạo các bộ định thời (Timer) tích hợp sẵn của ERa */
    ERa.addInterval(1000L, timerEvent); // In Uptime mỗi 1 giây

    // Yêu cầu ERa tự động gọi hàm đọc Modbus mỗi 1 giây (1000ms)
    // Bạn có thể chỉnh 1000L thành 2000L nếu muốn đọc chậm lại
    ERa.addInterval(1000L, readModbusData);
}

void loop()
{
    // Vòng lặp chỉ cần duy nhất hàm này, ERa sẽ tự lo kết nối mạng và Timer
    ERa.run();
}