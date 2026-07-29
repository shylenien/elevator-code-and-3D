#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// --- THÔNG TIN WIFI ---
#define WIFI_SSID "Hung Pham"
#define WIFI_PASSWORD "00851855"

// --- THÔNG TIN FIREBASE ---
#define API_KEY " "
// Bỏ chữ "https://" và dấu "/" ở cuối nhé!
#define DATABASE_URL "elavatorcontrol-default-rtdb.asia-southeast1.firebasedatabase.app"

FirebaseData stream; // Biến chuyên dụng để Lắng nghe (Stream)
FirebaseData fbdo;   // Biến chuyên dụng để gửi/ghi dữ liệu
FirebaseAuth auth;
FirebaseConfig config;

// Chân giao tiếp với Arduino (Đã chốt)
#define RXD2 16
#define TXD2 17

int currentSpeed = 0;
int currentDir = 1;

// Biến quản lý kết nối của Uno
unsigned long lastUnoHeartbeat = 0;
bool unoOnline = false;

// Biến quản lý nhịp tim ESP32 lên Firebase
unsigned long lastFirebaseHeartbeat = 0;
const unsigned long espHeartbeatInterval = 1000; // 1 giây (Nhanh hơn)

// Biến quản lý trạng thái mạng an toàn
unsigned long lastNetworkOkTime = 0;
bool networkIsDead = false;

// ==============================================================
// HÀM NÀY TỰ ĐỘNG CHẠY MỖI KHI BẠN BẤM NÚT HOẶC KHI VỪA KHỞI ĐỘNG XONG
// ==============================================================
void streamCallback(FirebaseStream data)
{
  String path = data.dataPath(); // Lấy tên biến bị thay đổi trên Firebase

  // 1. BẮT GÓI TIN TỔNG (JSON) Ở LẦN CHẠY ĐẦU TIÊN (INITIAL STATE)
  if (data.dataType() == "json")
  {
    FirebaseJson json;
    json.setJsonData(data.jsonString());
    FirebaseJsonData result;

    // Trích xuất speed
    json.get(result, "speed");
    if (result.success)
      currentSpeed = result.intValue;

    // Trích xuất direction
    json.get(result, "direction");
    if (result.success)
      currentDir = result.intValue;

    // Gửi lệnh đồng bộ đầu tiên xuống Arduino Uno
    char command[20];
    sprintf(command, "%d:%d\n", currentSpeed, currentDir);
    Serial2.print(command);

    Serial.print("⚡ Dong bo trang thai khoi dong -> Gui xuong Uno: ");
    Serial.print(command);
  }
  // 2. BẮT CÁC LỆNH LẺ KHI BẤM NÚT TRÊN WEB (Nâng cấp thêm chống hụt lệnh)
  else if (data.dataType() == "int" || data.dataType() == "float" || data.dataType() == "double")
  {
    // Nếu biến thay đổi là speed
    if (path == "/speed")
    {
      currentSpeed = data.intData();
    }
    // Nếu biến thay đổi là direction
    else if (path == "/direction")
    {
      currentDir = data.intData();
    }
    else
    {
      // Bỏ qua nếu sự kiện đổi là heartbeat hoặc status (Tránh lỗi gửi lại lệnh cũ)
      return;
    }

    // Ngay lập tức đóng gói lệnh gửi UART xuống Arduino Uno
    char command[20];
    sprintf(command, "%d:%d\n", currentSpeed, currentDir);
    Serial2.print(command);

    // In ra màn hình máy tính để bạn giám sát
    Serial.print("⚡ Lenh moi tu Web -> Gui xuong Uno: ");
    Serial.print(command);
  }
}

// Hàm cảnh báo khi đường truyền mạng bị lag
void streamTimeoutCallback(bool timeout)
{
  if (timeout)
    Serial.println("Cảnh báo: Stream timeout, dang khoi dong lai...");
}

// ==============================================================
// SETUP HỆ THỐNG
// ==============================================================
void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Dang ket noi WiFi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi OK!");

  // Cấu hình Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.signer.test_mode = true;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Dang ket noi may chu Firebase Stream...");

  // Lưu ý: Phiên bản thư viện Firebase hiện tại của bạn không hỗ trợ hàm onDisconnect() trực tiếp.
  // Báo cáo ESP32 đã lên mạng
  Firebase.RTDB.setString(&fbdo, "/cabin/status/esp32", "online");
  // Báo cáo Arduino đang offline chờ kết nối
  Firebase.RTDB.setString(&fbdo, "/cabin/status/arduino", "offline");

  // Mở cổng Lắng nghe liên tục trên nhánh /cabin
  if (!Firebase.RTDB.beginStream(&stream, "/cabin"))
  {
    Serial.printf("❌ LOI KET NOI: %s\n", stream.errorReason().c_str());
  }
  else
  {
    Serial.println("✅ KET NOI THANH CONG! San sang nhan lenh tu Web...");
    // Khởi động trình lắng nghe nền (Chạy ngầm)
    Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
  }
}

void loop()
{
  // Lắng nghe phản hồi từ Arduino Uno
  if (Serial2.available() > 0)
  {
    String msg = Serial2.readStringUntil('\n');
    msg.trim();
    if (msg == "OK")
    {
      lastUnoHeartbeat = millis();
      if (!unoOnline)
      {
        unoOnline = true;
        Firebase.RTDB.setStringAsync(&fbdo, "/cabin/status/arduino", "online");
        Serial.println("✅ Arduino Uno da ket noi UART!");
      }
    }
    else if (msg.startsWith("DIR:"))
    {
      int newDir = msg.substring(4).toInt();
      currentDir = newDir;
      Firebase.RTDB.setIntAsync(&fbdo, "/cabin/direction", newDir);
      Serial.printf("🔄 Arduino tu dong dao chieu sang %d, cap nhat Firebase!\n", newDir);
    }
  }

  // Kiểm tra timeout (3 giây không thấy Uno trả lời)
  if (unoOnline && (millis() - lastUnoHeartbeat > 3000))
  {
    unoOnline = false;
    Firebase.RTDB.setStringAsync(&fbdo, "/cabin/status/arduino", "offline");
    Serial.println("❌ Mat ket noi UART voi Arduino Uno!");
  }

  // --- WATCHDOG LỚP 1: BẢO VỆ MẤT MẠNG ---
  // Giám sát kết nối WiFi và Firebase
  if (Firebase.ready())
  {
    lastNetworkOkTime = millis();
    if (networkIsDead)
    {
      networkIsDead = false;
    }
  }
  else
  {
    // Nếu rớt mạng quá 3 giây
    if (!networkIsDead && (millis() - lastNetworkOkTime > 3000))
    {
      networkIsDead = true;
      Serial.println("❌ CẢNH BÁO MẤT MẠNG: Ra lệnh phanh khẩn cấp động cơ!");
      Serial2.print("0:0\n"); // Gửi lệnh tốc độ 0 để phanh gấp
    }
  }

  // Gửi nhịp tim lên Firebase mỗi 1 giây để chứng minh ESP32 còn sống (Gửi Không Đồng Bộ)
  if (millis() - lastFirebaseHeartbeat >= espHeartbeatInterval)
  {
    lastFirebaseHeartbeat = millis();
    Firebase.RTDB.setIntAsync(&fbdo, "/cabin/status/heartbeat", millis());

    // Đồng thời phát PING báo bình an xuống Arduino Uno
    Serial2.print("PING\n");
  }
}
