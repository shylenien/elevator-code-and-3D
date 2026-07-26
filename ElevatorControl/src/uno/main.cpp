#include <Arduino.h>
#include <SoftwareSerial.h>

// RX = 2 (nối với TX 43 của ESP32)
// TX = 3 (nối với RX 44 của ESP32)
SoftwareSerial espSerial(2, 3);

// Chân điều khiển động cơ cabin L298N
const int enA = 9;
const int in1 = 8;
const int in2 = 7;

// Chân điều khiển cho công tắc hành trình

const int lsFwd = 5;
const int lsRev = 13;

// --- CÁC BIẾN CHO VIỆC NHẬN DỮ LIỆU TỐI ƯU ---
const byte numChars = 32;     // Kích thước bộ đệm (tối đa 32 ký tự)
char receivedChars[numChars]; // Mảng ký tự C chứa dữ liệu nhận được
boolean newData = false;      // Cờ báo hiệu có dữ liệu mới nguyên vẹn

int currentSpeed = 0;
int currentDir = 1;

// Biến đếm thời gian gửi Heartbeat
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 1000; // 1 giây

// Biến quản lý Watchdog
unsigned long lastEspPing = 0;
bool isEspDead = false;

// ==============================================================
// KHAI BÁO NGUYÊN MẪU HÀM (FUNCTION PROTOTYPES)
// Bắt buộc trong C++ để sửa lỗi "not declared in this scope"
// ==============================================================
void recvWithEndMarker();
void parseData();
void updateMotor();

void setup()
{
    Serial.begin(9600);
    espSerial.begin(9600);

    pinMode(enA, OUTPUT);
    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);

    // Quan trọng: Dùng INPUT_PULLUP cho công tắc hành trình NC
    pinMode(lsFwd, INPUT_PULLUP);
    pinMode(lsRev, INPUT_PULLUP);

    // Khởi tạo trạng thái dừng an toàn
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(enA, 0);

    // Thiết lập thời điểm khởi động
    lastHeartbeat = millis();
    lastEspPing = millis(); // Khởi tạo mốc thời gian PING ban đầu

    Serial.println("Arduino da san sang (Phien ban Toi Uu)...");
}

void loop()
{
    // 1. Nhận dữ liệu liên tục không chờ (Non-blocking)
    recvWithEndMarker();

    // 2. Chỉ xử lý khi đã nhận đủ một gói tin hoàn chỉnh (có ký tự \n)
    if (newData == true)
    {
        parseData();     // Bóc tách dữ liệu và in log
        newData = false; // Reset cờ để sẵn sàng nhận gói tin tiếp theo
    }

    // 2.5 Luôn luôn giám sát công tắc hành trình và điều khiển động cơ
    updateMotor();

    // 3. Gửi tín hiệu Heartbeat phản hồi lên ESP32 mỗi giây để báo "còn sống"
    if (millis() - lastHeartbeat >= heartbeatInterval)
    {
        lastHeartbeat = millis();
        espSerial.println("OK");
    }
}

// -------------------------------------------------------------
// Hàm nhận dữ liệu theo từng byte, không làm treo chương trình
// -------------------------------------------------------------
void recvWithEndMarker()
{
    static byte ndx = 0;
    char endMarker = '\n'; // Ký tự chốt gói tin từ ESP32 gửi xuống
    char rc;

    // Chỉ đọc khi có tín hiệu, nếu không có thì vòng loop() vẫn chạy mượt qua các tác vụ khác
    while (espSerial.available() > 0 && newData == false)
    {
        rc = espSerial.read();

        if (rc != endMarker)
        {
            receivedChars[ndx] = rc;
            ndx++;
            // Ngăn tràn bộ nhớ RAM nếu chuỗi bị nhiễu quá dài
            if (ndx >= numChars)
            {
                ndx = numChars - 1;
            }
        }
        else
        {
            receivedChars[ndx] = '\0'; // Thêm ký tự kết thúc chuỗi chuẩn C
            ndx = 0;

            // Lọc lệnh PING từ ESP32
            if (strcmp(receivedChars, "PING") == 0)
            {
                lastEspPing = millis();
                if (isEspDead)
                    isEspDead = false;
            }
            else
            {
                // Các lệnh điều khiển khác
                newData = true;
                lastEspPing = millis(); // Bất kỳ lệnh nào cũng chứng tỏ cáp vẫn thông
                if (isEspDead)
                    isEspDead = false;
            }
        }
    }
}

// -------------------------------------------------------------
// Hàm bóc tách chuỗi C (nhẹ và an toàn hơn class String rất nhiều)
// -------------------------------------------------------------
void parseData()
{
    char *strtokIndx; // Con trỏ để tìm dấu phân cách

    // Mẫu chuỗi nhận được từ ESP32: "255:1"

    // Lấy phần đầu tiên (trước dấu :)
    strtokIndx = strtok(receivedChars, ":");
    if (strtokIndx != NULL)
    {
        currentSpeed = atoi(strtokIndx); // Chuyển chuỗi chữ thành số nguyên
    }

    // Lấy phần thứ hai (sau dấu :)
    strtokIndx = strtok(NULL, ":");
    if (strtokIndx != NULL)
    {
        currentDir = atoi(strtokIndx);
    }

    // In ra màn hình máy tính để giám sát (chỉ in khi có lệnh mới)
    Serial.print("Nhan lenh -> Toc do: ");
    Serial.print(currentSpeed);
    Serial.print(" | Chieu: ");
    Serial.println(currentDir == 1 ? "TIEN" : "LUI");
}

// Biến toàn cục lưu trạng thái cũ của công tắc
bool lastFwdTouched = LOW;
bool lastRevTouched = LOW;
bool isStoppedByLimit = false;

// -------------------------------------------------------------
// Hàm thực thi điều khiển L298N và bảo vệ hành trình
// -------------------------------------------------------------
void updateMotor()
{
    // --- WATCHDOG LỚP 2: BẢO VỆ ĐỨT CÁP HOẶC ESP32 CHẾT ---
    if (!isEspDead && (millis() - lastEspPing > 3000))
    {
        isEspDead = true;
        currentSpeed = 0; // Cắt nguồn động cơ
        Serial.println("❌ CẢNH BÁO: Mất tín hiệu ESP32. Đã phanh khẩn cấp!");
    }

    // Đọc trạng thái công tắc hành trình (NC + INPUT_PULLUP -> HIGH là chạm)
    bool isFwdTouched = digitalRead(lsFwd);
    bool isRevTouched = digitalRead(lsRev);

    // 1. Đang chạy TIẾN
    if (currentDir == 1)
    {
        if (isFwdTouched == HIGH)
        {
            isStoppedByLimit = true; // Chạm thì dừng
        }
        else if (isFwdTouched == LOW && lastFwdTouched == HIGH)
        {
            // Vừa nhả tay ra -> Đảo chiều sang LÙI
            currentDir = 0;
            isStoppedByLimit = false;
            espSerial.println("DIR:0"); // Báo lên ESP32 để đồng bộ Web
        }
    }

    // 2. Đang chạy LÙI
    if (currentDir == 0)
    {
        if (isRevTouched == HIGH)
        {
            isStoppedByLimit = true; // Chạm thì dừng
        }
        else if (isRevTouched == LOW && lastRevTouched == HIGH)
        {
            // Vừa nhả tay ra -> Đảo chiều sang TIẾN
            currentDir = 1;
            isStoppedByLimit = false;
            espSerial.println("DIR:1"); // Báo lên ESP32 để đồng bộ Web
        }
    }

    // Lưu lại trạng thái cho chu kỳ sau
    lastFwdTouched = isFwdTouched;
    lastRevTouched = isRevTouched;

    // Giới hạn tốc độ an toàn tuyệt đối
    int safeSpeed = constrain(currentSpeed, 0, 255);
    if (isStoppedByLimit)
    {
        safeSpeed = 0; // Ép dừng nếu đang bị khóa
    }

    // Đảo chiều và Bơm xung PWM
    if (safeSpeed == 0)
    {
        // PHANH CHỦ ĐỘNG (Active Brake): Giữ chặt cửa không cho trôi/nảy ngược
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW);
        analogWrite(enA, 255);
    }
    else
    {
        if (currentDir == 1)
        {
            digitalWrite(in1, HIGH);
            digitalWrite(in2, LOW);
        }
        else
        {
            digitalWrite(in1, LOW);
            digitalWrite(in2, HIGH);
        }
        analogWrite(enA, safeSpeed);
    }
}