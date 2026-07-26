// Khai báo chân cho L298N
const int IN1 = 8;
const int IN2 = 9;
const int ENA = 10;

// Khai báo chân Công tắc hành trình (Cách 2: Dùng chân NC)
const int LS_UP = 2;   // Hành trình TRÊN (Mở hết)
const int LS_DOWN = 3; // Hành trình DƯỚI (Đóng hết)

// Biến lưu trạng thái hướng đi (1: Đang lên, 2: Đang xuống)
int direction = 1; 

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  // Quan trọng: Dùng INPUT_PULLUP cho công tắc hành trình NC
  pinMode(LS_UP, INPUT_PULLUP);
  pinMode(LS_DOWN, INPUT_PULLUP);

  Serial.begin(9600);
  
  // Bắt đầu cho chạy lên để tìm điểm hành trình đầu tiên
  goUp(200); 
}

void loop() {
  // Đọc trạng thái công tắc: HIGH là CHẠM (hoặc đứt dây), LOW là CHƯA CHẠM
  bool isUpTouched = digitalRead(LS_UP);
  bool isDownTouched = digitalRead(LS_DOWN);

  // 1. Nếu đang đi lên mà chạm giới hạn TRÊN (hoặc đứt dây LS_UP)
  if (direction == 1 && isUpTouched == HIGH) {
    Serial.println("Da cham hanh trinh TREN. Dung va doi chieu...");
    stopMotor();
    //delay(2000);    // Nghỉ 2 giây
    direction = 2;  // Chuyển sang trạng thái đi xuống
    goDown(200);
  }

  // 2. Nếu đang đi xuống mà chạm giới hạn DƯỚI (hoặc đứt dây LS_DOWN)
  if (direction == 2 && isDownTouched == HIGH) {
    Serial.println("Da cham hanh trinh DUOI. Dung va doi chieu...");
    stopMotor();
    //delay(2000);    // Nghỉ 2 giây
    direction = 1;  // Chuyển sang trạng thái đi lên
    goUp(200);
  }
}

void goUp(int speed) {
  // Chỉ cho phép quay thuận nếu công tắc trên chưa bị chạm
  if (digitalRead(LS_UP) == LOW) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, speed);
  }
}

void goDown(int speed) {
  // Chỉ cho phép quay nghịch nếu công tắc dưới chưa bị chạm
  if (digitalRead(LS_DOWN) == LOW) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    analogWrite(ENA, speed);
  }
}

void stopMotor() {
  // Phanh động cơ bằng cách cho cả 2 chân về LOW hoặc HIGH
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);
}