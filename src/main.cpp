/*ไลบรารี่ TridentTD_LineNotify version 2.1
ใช้สำหรับ ส่ง แจ้งเตือนไปยัง LINE สำหรับ ESP8266 และ ESP32
สามารถส่งได้ทั้ง ข้อความ , สติกเกอร์ และรูปภาพ(ด้วย url)
---------------------------------------------------- -
ให้ save เป็น file ต่างหากก่อนถึงจะส่ง Line Notify ภาษาไทยได้
*/

#include <Arduino.h>
#include <Wire.h>
// #include <ESP8266WiFi.h>
#include <TridentTD_LineNotify.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// SSID และ Password ของ Wifi ให้ ESP8266 ได้เชื่อมต่อ
#define SSID "IOT" 
#define PASSWORD "abac3782133" 

// กำหนด Port สำหรับสร้าง Server
const int port = 8888;
WiFiServer server(port);

// กำหนดข้อมูลเกี่ยวกับ Line
#define LINE_TOKEN "p5i3cA07rUkisAioIR5knfSaHoYMOIyyIVUjAGGRJ9v" 

HardwareSerial mySoftwareSerial(1);

//SoftwareSerial mySoftwareSerial(16, 17); // RX, TX ใช้รับส่งข้อมูล (bit) กับ dfplayermini

DFRobotDFPlayerMini myDFPlayer;

void printDetail(uint8_t type, int value); //ฟังก์ชั่น รับ Status ของ dfplayermini

String sentNoti_ledShow(int clientNo); // ฟังก์ชั่น รับข้อมูลว่า Client ตัวไหนส่งมา เพื่อแจ้งเตือนทาง line และ เปิด LED

bool LIGHT_SIGNAL = false; // จะให้แสดงไฟสถานะของกริ่งประตูหรือไม่

//สร้าง stuct ไว้เก็บข้อมูล client โดย มีตัวแปรขาบนบอร์ด และ ข้อความที่จะให้ส่งผ่านไลน์
struct wifiClient
{
 int ledClientSignal; // ขาที่จำให้ทำงาน
 String LineNoti; // ข้อความที่ต้องการให้ส่งผ่าน line
};

// สร้าง clint กริ่งประตู เป็น array
wifiClient doorBell[] = {  // ขาของ ESP8266 ที่จะให้ส่งสัญญาณ Digital int , "ข้อความที่จะให้ส่งไปทาง line"
                          {32,"มีผู้มาพบประตู Cafe"},     // 0 ประตูร้าน 
                          {33,"มีผู้มาพบประตูใหญ่"},       // 1 ประตูใหญ่
                          {25,"มีผู้มาพบประตูหลังบ้าน"}     // 2 ประตูหลัง
};

void setup() {

  mySoftwareSerial.begin(9600, SERIAL_8N1, 16, 17);  // สร้าง Serial สำหรับติดต่อกับ dfPlayer RX, TX 

  Serial.begin(115200); 

  Serial.println(F("Initializing DFPlayer ..."));

    if (!myDFPlayer.begin(mySoftwareSerial)) {  //ตรวจสอบกับการเชื่อมต่อกับ dfPlayer 
      Serial.println("Unable to begin:");
      Serial.println("1.Please recheck the connection!");
      Serial.println("2.Please insert the SD card!");
    while(true);
  }

  Serial.println("DFPlayer Mini online.");

  myDFPlayer.setTimeOut(500); //Set serial communictaion time out 500ms
  myDFPlayer.volume(30);
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  Serial.println(myDFPlayer.readState()); //read mp3 state
  Serial.println(myDFPlayer.readVolume()); //read current volume
  Serial.println(myDFPlayer.readEQ()); //read EQ setting
  Serial.println(myDFPlayer.readFileCounts()); //read all file counts in SD card



  // กำหนด Line Token
  LINE.setToken(LINE_TOKEN); 

  // ตั้งค่า LED PIN
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Loop เพื่อตั้งค่าไฟ LED แสดงสถานะ กริ่งประตูแต่ละจุด
  for(int i = 0; i<sizeof(doorBell)/sizeof(doorBell[0]); i++) { // Loop จนเท่าจำนวน Array ของ ledClientSignal
    pinMode(doorBell[i].ledClientSignal, OUTPUT); // Loop เพื่อตั้งค่า PIN ของ ประตู
    digitalWrite(doorBell[i].ledClientSignal, LOW); // เปิดไฟ Led บนบอร์ดค้างแสดงสถานะ รอการเชื่อมต่อ
  }

  Serial.println();
  Serial.println(LINE.getVersion());
  Serial.print("Number Array of Client = ");
  Serial.println(sizeof(doorBell)/sizeof(doorBell[0])); // ตรวจจำนวน Client โดยดูจากข้อมูลใน Array
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.printf("WiFi connecting to %s\n", SSID);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }
  
  Serial.printf("\nWiFi connected\nIP : ");
  Serial.println(WiFi.localIP());

  // สร้าง Server 
  server.begin();
  Serial.print("Open Server and Connect to IP : ");
  Serial.print(WiFi.localIP());
  Serial.print("  Port : ");
  Serial.println(port);

 // digitalWrite(LED_BUILTIN, LOW); // เปิดไฟ Led บนบอร์ดค้างแสดงสถานะ รอการเชื่อมต่อ

}


void loop() {

  WiFiClient client = server.available();

  if (client) {

    // ตรวจจับว่าถ้ามีการเชื่อมต่อ (ทำครั้งเดียว)
    if (client.connected()) Serial.println("Client Connected");

    // ระหว่างมีการเชื่อมต่ออยู่ (ทำวนไปเรื่อยๆ จนกว่า Client จะออกไป)
    while (client.connected()) {

      // เอาไว้รวมข้อมูลที่ส่งมาทีละ Byte
      String message = ""; 
        
        // อ่านข้อมูลที่ส่งมา จะหมด ทีละ Byte
        while (client.available()) { 
          char ch = client.read(); // รับข้อมูลทีละ Byte
          if (ch != '\r' || ch != '\n') message += ch; //ถ้า ข้อความที่รับมาไม่ใช่ ต่าขึ้นบรรทัดใหม่ หรือ Return ให้จัดเก็บลง Message
        }
        
        // ตรวจว่า Message ไม่ใช่ข้อมูลว่าง ให้ส่งข้อมูลแปลงเป็น int (toInt()) ไปยัง ฟังก​์ชั่น ตรวจสอบตำแหน่ง Client
        if(message != "")  client.println(sentNoti_ledShow(message.toInt())); 
        
        // ส่งข้อมูลกลับไปหา Client
        while (Serial.available() > 0) client.write(Serial.read());

    }
  

    client.stop();
    Serial.println("Client disconnected");
    // digitalWrite(LED_BUILTIN, LOW);

  }
}
// ************************************************************************************

// ฟังก์ชั่น รับข้อมูลว่า Client ตัวไหนส่งมา เพื่อแจ้งเตือนทาง line และ เปิด LED และเล่นเสียง
String sentNoti_ledShow(int clientNo){

  if(clientNo < sizeof(doorBell)/sizeof(doorBell[0])){ //ตรวจสอบ ว่าใส่เลข Cline เกินจำนวนที่มีหรือไม่

    myDFPlayer.play(clientNo+1); // เล่นเสียง

    LINE.notify(doorBell[clientNo].LineNoti);

// แสดงสัญญาณไฟ
  if (LIGHT_SIGNAL){
      digitalWrite(doorBell[clientNo].ledClientSignal, HIGH);    
      delay(3000);
      digitalWrite(doorBell[clientNo].ledClientSignal, LOW);
  }
    
    //Serial.println("Check data = Data is not NULL");

    return doorBell[clientNo].LineNoti;

  } 
  // Serial.println("Check data = Data is  NULL");

}

// ฟังก์ชั่น Print Status ของ dfplayer
void printDetail(uint8_t type, int value){
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}