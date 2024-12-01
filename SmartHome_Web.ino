#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <MFRC522.h>
#include <ESP32_Servo.h>
#include <FirebaseESP32.h>

// Định nghĩa chân kết nối
#define LED_PIN 32      // Chân kết nối đèn
#define FAN_PIN 33      // Chân kết nối quạt
#define FAN_PIN_2 12    // Chân kết nối quạt 16 chân cũ
#define GAS_PIN 27      // Chân kết nối cảm biến gas
#define BUTTON_PIN1 25  // Chân kết nối nút nhấn 1
#define BUTTON_PIN2 26  // Chân kết nối nút nhấn 2
#define DHT_PIN 5       // Chân kết nối cảm biến nhiệt độ và độ ẩm DHT11
#define RFID_SS 4       // Chân SS của RFID
#define RFID_RST 14     // Chân RST của RFID
#define SERVO_PIN 13    // Chân kết nối servo
#define COI 16          // Chân kết nối còi báo động

// Thông tin WiFi
const char *ssid = "Quang-T2";        // Thay bằng SSID WiFi
const char *password = "0982049863";  // Thay bằng mật khẩu WiFi

// Firebase Config
#define FIREBASE_HOST "smarthomem-de96a-default-rtdb.firebaseio.com"  // Thay bằng host Firebase của bạn
#define FIREBASE_AUTH "cFkIONfn6HrbjSs5VP4JexxqwgRgXvCZE7uYwSbO"      // Thay bằng secret key của bạn
FirebaseConfig config;
FirebaseAuth auth;
FirebaseData firebaseData;

// Định nghĩa UID hợp lệ
const String validUID = "43 61 19 2F";  // Thay bằng UID của thẻ bạn

// Biến trạng thái
bool isDoorOpen = false;
bool isFanOn = false;
bool isLedOn = false;

AsyncWebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Địa chỉ I2C của LCD
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

MFRC522 rfid(RFID_SS, RFID_RST);
Servo servo;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(FAN_PIN_2, OUTPUT);
  pinMode(GAS_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(COI, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(COI, LOW);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  dht.begin();
  servo.attach(SERVO_PIN);
  servo.write(0);  // Servo ở vị trí đóng cửa ban đầu

  SPI.begin();
  rfid.PCD_Init();

  // Kết nối WiFi
  Serial.begin(115200);
  Serial.println("Đang kết nối WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Đang kết nối...");
  }
  Serial.println("WiFi đã kết nối!");
  Serial.print("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());
  config.database_url = FIREBASE_HOST;
  config.api_key = FIREBASE_AUTH;
  config.signer.test_mode = true;
  // Kết nối Firebase
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);
  // Ghi trạng thái ban đầu lên Firebase
  updateAllFirebase();
  // Khởi động máy chủ
  server.begin();
}

// Mở cửa
void openDoor() {
  servo.write(180);
  isDoorOpen = true;
  updateAllFirebase();
  Serial.println("Cửa đã mở.");
  delay(3000);
}

// Đóng cửa
void closeDoor() {
  servo.write(0);
  isDoorOpen = false;
  updateAllFirebase();
  Serial.println("Cửa đã đóng.");
}

// Cập nhật tất cả trạng thái lên Firebase
void updateAllFirebase() {
  if (Firebase.ready()) {
    Firebase.setBool(firebaseData, "/door/status", isDoorOpen);
    Firebase.setBool(firebaseData, "/fan/status", isFanOn);
    Firebase.setBool(firebaseData, "/led/status", isLedOn);
    updateTemperatureHumidity();
  }
}

// Cập nhật nhiệt độ và độ ẩm
void updateTemperatureHumidity() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  if (Firebase.ready()) {
    Firebase.setFloat(firebaseData, "/environment/temperature", temperature);
    Firebase.setFloat(firebaseData, "/environment/humidity", humidity);
  }
}

// Hàm trả về trang HTML
String getPage() {
  String page = "<html lang='en'><head>";
  page += "<meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<title>Nhà Thông Minh</title>";
  page += "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/css/bootstrap.min.css' rel='stylesheet'>";
  page += "<style>body { font-family: Arial, sans-serif; background-color: #f8f9fa; }</style>";
  page += "</head><body>";
  page += "<div class='container'>";
  page += "<h1 class='text-center my-4'>Nhà Thông Minh</h1>";
  page += "<div class='row justify-content-center'>";
  page += "<div class='col-md-3 text-center'>";
  page += "<h3>Nhiệt độ: " + String(dht.readTemperature()) + " &#8451;</h3>";
  page += "<h3>Độ ẩm: " + String(dht.readHumidity()) + " %</h3>";
  page += "<h3>Trạng thái cửa: " + String(isDoorOpen ? "Đang mở" : "Đang đóng") + "</h3>";
  page += "<br>";
  page += "<button class='btn btn-primary m-2' onclick=\"location.href='/led/on'\">Bật Đèn</button>";
  page += "<button class='btn btn-danger m-2' onclick=\"location.href='/led/off'\">Tắt Đèn</button><br>";
  page += "<button class='btn btn-success m-2' onclick=\"location.href='/fan/on'\">Bật Quạt</button>";
  page += "<button class='btn btn-warning m-2' onclick=\"location.href='/fan/off'\">Tắt Quạt</button><br><br>";
  page += "<button class='btn btn-info' onclick=\"location.href='/door/toggle'\">" + String(isDoorOpen ? "Đóng Cửa" : "Mở Cửa") + "</button>";
  page += "</div></div></div>";
  page += "<script src='https://cdn.jsdelivr.net/npm/@popperjs/core@2.11.6/dist/umd/popper.min.js'></script>";
  page += "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/js/bootstrap.min.js'></script>";
  page += "</body></html>";
  return page;
}

void loop() {
  updateLCD();
  // Kiểm tra cảm biến gas
  if (digitalRead(GAS_PIN) == 0) {
    digitalWrite(COI, HIGH);
    digitalWrite(FAN_PIN_2, HIGH);
    isFanOn = true;
  } else {
    digitalWrite(COI, LOW);
    digitalWrite(FAN_PIN_2, LOW);
  }

  // Đọc nút nhấn
  if (digitalRead(BUTTON_PIN1) == LOW) {
    isLedOn = !isLedOn;
    digitalWrite(LED_PIN, isLedOn);
    updateAllFirebase();
    delay(300);
  }

  if (digitalRead(BUTTON_PIN2) == LOW) {
    isFanOn = !isFanOn;
    digitalWrite(FAN_PIN, isFanOn);
    updateAllFirebase();
    delay(300);
  }

  // Đọc thẻ RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String cardUID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      cardUID += String(rfid.uid.uidByte[i], HEX);
      if (i < rfid.uid.size - 1) {
        cardUID += " ";  // Thêm dấu cách giữa các byte
      }
    }
    cardUID.toUpperCase();  // Đổi UID sang chữ in hoa
    Serial.print("UID của thẻ: ");
    Serial.println(cardUID);  // In UID ra Serial Monitor

    if (cardUID == validUID) {  // So sánh UID
      if (isDoorOpen) {
        closeDoor();
      } else {
        openDoor();
      }
    } else {
      Serial.println("Thẻ không hợp lệ!");
    }
    rfid.PICC_HaltA();
  }

  // Cập nhật dữ liệu DHT11 lên Firebase
  updateTemperatureHumidity();

  // Cập nhật trang HTML khi có yêu cầu từ client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", getPage());
  });

  // Điều khiển đèn qua URL
  server.on("/led/on", HTTP_GET, [](AsyncWebServerRequest *request) {
    digitalWrite(LED_PIN, HIGH);
    isLedOn = true;
    updateAllFirebase();
    request->send(200, "text/html", getPage());
  });

  server.on("/led/off", HTTP_GET, [](AsyncWebServerRequest *request) {
    digitalWrite(LED_PIN, LOW);
    isLedOn = false;
    updateAllFirebase();
    request->send(200, "text/html", getPage());
  });

  // Điều khiển quạt qua URL
  server.on("/fan/on", HTTP_GET, [](AsyncWebServerRequest *request) {
    digitalWrite(FAN_PIN, HIGH);
    isFanOn = true;
    updateAllFirebase();
    request->send(200, "text/html", getPage());
  });

  server.on("/fan/off", HTTP_GET, [](AsyncWebServerRequest *request) {
    digitalWrite(FAN_PIN, LOW);
    isFanOn = false;
    updateAllFirebase();
    request->send(200, "text/html", getPage());
  });

  // Điều khiển cửa qua URL
  server.on("/door/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (isDoorOpen) {
      closeDoor();
    } else {
      openDoor();
    }
    request->send(200, "text/html", getPage());
  });

  // Xử lý yêu cầu khác
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "404: Not Found");
  });
  delay(500);
}
void updateLCD() {
  lcd.clear();
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    lcd.setCursor(0, 0);
    lcd.print("DHT11 Error!");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print(" C");
    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(humidity);
    lcd.print(" %");
  }
}
