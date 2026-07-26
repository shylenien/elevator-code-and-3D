const int latchPin = 10; // Chân LOAD trên mạch
const int clockPin = 13; // Chân SCLK trên mạch
const int dataPin = 11;  // Chân SDI trên mạch

// Bảng mã LED 7 thanh Anode chung (từ số 0-9)
byte maLed[] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90};

void setup() {
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
}

// Hàm truyền vào 2 số riêng biệt cho 2 mạch
void dieuKhien2Mach(int soMach2, int soMach1) {
  // --- BƯỚC 1: Tách chữ số của Mạch 2 (Mạch ở xa Arduino nhất) ---
  int chuc2  = soMach2 / 10;
  int donVi2 = soMach2 % 10;

  // --- BƯỚC 2: Tách chữ số của Mạch 1 (Mạch ở gần Arduino nhất) ---
  int chuc1  = soMach1 / 10;
  int donVi1 = soMach1 % 10;

  // Bắt đầu mở chốt để nạp dữ liệu
  digitalWrite(latchPin, LOW);
  
  // 1. GỬI CHO MẠCH 2 TRƯỚC (Dữ liệu này sẽ bị đẩy trôi về module cuối hàng)
  shiftOut(dataPin, clockPin, MSBFIRST, maLed[donVi2]); // Hàng đơn vị mạch 2
  shiftOut(dataPin, clockPin, MSBFIRST, maLed[chuc2]);  // Hàng chục mạch 2
  
  // 2. GỬI CHO MẠCH 1 SAU (Dữ liệu này nạp sau nên sẽ nằm lại ở module đầu tiên)
  shiftOut(dataPin, clockPin, MSBFIRST, maLed[donVi1]); // Hàng đơn vị mạch 1
  shiftOut(dataPin, clockPin, MSBFIRST, maLed[chuc1]);  // Hàng chục mạch 1
  
  // Đóng chốt LOAD để xuất toàn bộ dữ liệu ra màn hình cùng lúc
  digitalWrite(latchPin, HIGH);
}

void loop() {
  dieuKhien2Mach(02, 01); 
  delay(3000); // Giữ nguyên trong 3 giây
  
}