#include <Arduino.h>
#include <TaskScheduler.h>

#define RED_LED 9
#define YELLOW_LED 10
#define GREEN_LED 11
#define BUTTON_B1 2
#define BUTTON_B2 3
#define BUTTON_B3 4
#define POTENTIOMETER A0

int mode = 0;
unsigned long redTime = 2000, yellowTime = 500, greenTime = 2000;
int ledBrightness = 255;
int red = 0, yellow = 0, green = 0;

unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long lastDebounceTime3 = 0;


const unsigned long debounceDelay = 50;

Scheduler runner;

// 공용 함수 선언
void disableAllTasks();
void readBrightness();
void CheckSerial();
void checkButtons();

// --- default 모드용 5단계 Task 함수 선언 ---
void taskRedLED();
void taskYellowLED();
void taskGreenLED();
void taskGreenBlink();
void taskYellowLED2();

// 모드 1 전용 Task (Red LED의 밝기를 실시간 갱신)
void taskMode1();
// 모드 2용 (모든 LED 깜빡이기) Task
void taskAllLED();

// ----------------- Task 객체 생성 --------------
Task tRedLED       (10, TASK_FOREVER, &taskRedLED,       &runner, false);
Task tYellowLED    (10, TASK_FOREVER, &taskYellowLED,    &runner, false);
Task tGreenLED     (10, TASK_FOREVER, &taskGreenLED,     &runner, false);
Task tGreenBlink   (10, TASK_FOREVER, &taskGreenBlink,   &runner, false);
Task tYellowLED2   (10, TASK_FOREVER, &taskYellowLED2,   &runner, false);
Task tBlinkAllLED  (10, TASK_FOREVER, &taskAllLED,       &runner, false);
Task tCheckSerial  (100, TASK_FOREVER, &CheckSerial,      &runner, true);
Task tMode1        (10, TASK_FOREVER, &taskMode1,        &runner, false);

void setup() {
  Serial.begin(9600);
  
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  
  pinMode(BUTTON_B1, INPUT_PULLUP);
  pinMode(BUTTON_B2, INPUT_PULLUP);
  pinMode(BUTTON_B3, INPUT_PULLUP);
  pinMode(POTENTIOMETER, INPUT);
  
  tRedLED.enable();
}

void loop() {
  runner.execute();
  readBrightness();
  checkButtons();
  CheckSerial();

  // 단일 JSON 메시지만 전송하도록 수정 (변수 기반 출력)
  Serial.print("{\"mode\":");
  Serial.print(mode);
  Serial.print(",\"red\":");
  Serial.print(red);
  Serial.print(",\"yellow\":");
  Serial.print(yellow);
  Serial.print(",\"green\":");
  Serial.print(green);
  Serial.print(",\"brightness\":");
  Serial.print(ledBrightness);
  Serial.println("}");
  

  delay(100); // 0.1초 간격으로 전송
}

void checkButtons() {
  // 각 버튼의 현재 읽은 값
  int reading1 = digitalRead(BUTTON_B1);
  int reading2 = digitalRead(BUTTON_B2);
  int reading3 = digitalRead(BUTTON_B3);
  
  // --- BUTTON_B1 디바운스 처리 (비상 버튼 → 모드1) ---
  static int stableButton1 = HIGH;
  static int lastStableButton1 = HIGH;
  if (reading1 != stableButton1) {
    lastDebounceTime1 = millis();
    stableButton1 = reading1;
  }
  if ((millis() - lastDebounceTime1) > debounceDelay) {
    if (stableButton1 != lastStableButton1) {
      lastStableButton1 = stableButton1;
      if (stableButton1 == LOW) {
        if (mode != 1) {
          mode = 1;
          disableAllTasks();
          tMode1.enable();
        } else {
          mode = 0;
          disableAllTasks();
          tRedLED.enable();
        }
      }
    }
  }
  
  // --- BUTTON_B2 디바운스 처리 (깜빡임 버튼 → 모드2) ---
  static int stableButton2 = HIGH;
  static int lastStableButton2 = HIGH;
  if (reading2 != stableButton2) {
    lastDebounceTime2 = millis();
    stableButton2 = reading2;
  }
  if ((millis() - lastDebounceTime2) > debounceDelay) {
    if (stableButton2 != lastStableButton2) {
      lastStableButton2 = stableButton2;
      if (stableButton2 == LOW) {
        if (mode != 2) {
          mode = 2;
          disableAllTasks();
          tBlinkAllLED.enable();
        } else {
          mode = 0;
          disableAllTasks();
          tRedLED.enable();
        }
      }
    }
  }
  
  // --- BUTTON_B3 디바운스 처리 (on/off 버튼 → 모드3 토글) ---
  static int stableButton3 = HIGH;
  static int lastStableButton3 = HIGH;
  if (reading3 != stableButton3) {
    lastDebounceTime3 = millis();
    stableButton3 = reading3;
  }
  if ((millis() - lastDebounceTime3) > debounceDelay) {
    if (stableButton3 != lastStableButton3) {
      lastStableButton3 = stableButton3;
      if (stableButton3 == LOW) {
        mode = (mode == 3) ? 0 : 3;
        disableAllTasks();
        if (mode == 0) {
          tRedLED.enable();
        }
      }
    }
  }
}

void CheckSerial() {
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();

    // 수신 데이터 출력
    Serial.print("Received Data: ");
    Serial.println(data);

    if (data.startsWith("{")) {
      if (data.indexOf("mode1") != -1) {
        mode = 1;
        disableAllTasks();
        tMode1.enable();
      } else if (data.indexOf("mode2") != -1) {
        mode = 2;
        disableAllTasks();
        tBlinkAllLED.enable();
      } else if (data.indexOf("mode3") != -1) {
        mode = 3;
        disableAllTasks();
      } else if (data.indexOf("mode0") != -1) {
        mode = 0;
        disableAllTasks();
        tRedLED.enable();
      }
    } else {
      int newRed, newYellow, newGreen, newmode;
      if (sscanf(data.c_str(), "%d,%d,%d,%d", &newRed, &newYellow, &newGreen, &newmode) == 4) {
        // 슬라이더 값 업데이트
        redTime = newRed;
        yellowTime = newYellow;
        greenTime = newGreen;

        // 디버깅용 값 출력
        Serial.print("Updated Times - Red: ");
        Serial.print(redTime);
        Serial.print(" ms, Yellow: ");
        Serial.print(yellowTime);
        Serial.print(" ms, Green: ");
        Serial.println(greenTime);

        // 새로운 모드 설정 및 로그 출력
        if (newmode != mode) { 
          mode = newmode;
          Serial.print("Updated Mode: ");
          Serial.println(mode);

          disableAllTasks();
          if (mode == 1) {
            tMode1.enable();
          } else if (mode == 2) {
            tBlinkAllLED.enable();
          } else if (mode == 3) {
            // 모드 3 실행 시 추가 동작 필요 없음
          } else if (mode == 0) {
            tRedLED.enable();
          }
        }
      } else {
        // 데이터 형식이 잘못된 경우
        Serial.println("Error: Invalid data format received.");
      }
    }
  }
}



void readBrightness() {
  int potValue = analogRead(POTENTIOMETER);
  ledBrightness = map(potValue, 0, 1023, 0, 255);
}

void disableAllTasks() {
  tRedLED.disable();
  tYellowLED.disable();
  tGreenLED.disable();
  tGreenBlink.disable();
  tYellowLED2.disable();
  tBlinkAllLED.disable();
  tMode1.disable();
  
  analogWrite(RED_LED, 0);
  analogWrite(YELLOW_LED, 0);
  analogWrite(GREEN_LED, 0);
  
  red = yellow = green = 0;
}

void taskRedLED() {
  static unsigned long startTime = 0;
  static bool isOn = false;

  if (mode != 0) {
    isOn = false;
    return;
  }

  if (!isOn) {
    analogWrite(RED_LED, ledBrightness); // 빨강 LED 켜기
    red = 1;
    startTime = millis();
    isOn = true;
  } else {
    if (millis() - startTime >= redTime) { // unsigned long 간 비교
      analogWrite(RED_LED, 0); // 빨강 LED 끄기
      red = 0;
      isOn = false;
      tRedLED.disable();
      tYellowLED.enable();
    }
  }
}


void taskYellowLED() {
  static unsigned long startTime = 0;
  static bool isOn = false;

  if (mode != 0) {
    isOn = false;
    return;
  }

  if (!isOn) {
    analogWrite(YELLOW_LED, ledBrightness); // 노랑 LED 켜기
    yellow = 1;
    startTime = millis();
    isOn = true;
  } else {
    if (millis() - startTime >= yellowTime) { // unsigned long 간 비교
      analogWrite(YELLOW_LED, 0); // 노랑 LED 끄기
      yellow = 0;
      isOn = false;
      tYellowLED.disable();
      tGreenLED.enable();
    }
  }
}


void taskGreenLED() {
  static unsigned long startTime = 0;
  static bool isOn = false;

  if (mode != 0) {
    isOn = false;
    return;
  }

  if (!isOn) {
    analogWrite(GREEN_LED, ledBrightness); // 초록 LED 켜기
    green = 1;
    startTime = millis();
    isOn = true;
  } else {
    if (millis() - startTime >= greenTime) { // unsigned long 간 비교
      analogWrite(GREEN_LED, 0); // 초록 LED 끄기
      green = 0;
      isOn = false;
      tGreenLED.disable();
      tGreenBlink.enable();
    }
  }
}


void taskGreenBlink() {
  static int toggleCount = 0;       // 깜빡임 횟수
  static unsigned long lastToggle = 0;
  const unsigned long interval = 333; // 333ms 간격

  if (mode != 0) {
    toggleCount = 0;
    return;
  }

  if (millis() - lastToggle >= interval) {
    lastToggle = millis();
    if (toggleCount % 2 == 0) { 
      analogWrite(GREEN_LED, 0);
      green = 0;
    } else { 
      analogWrite(GREEN_LED, ledBrightness);
      green = 1;
    }
    toggleCount++;
  }

  if (toggleCount >= 6) { // 총 3번 깜빡임 (on/off 6회)
    analogWrite(GREEN_LED, 0);
    green = 0;
    toggleCount = 0;
    tGreenBlink.disable();  
    tYellowLED2.enable(); // 노랑 LED 마지막 켜기 실행
  }
}

void taskYellowLED2() {
  static unsigned long startTime = 0;
  static bool isOn = false;

  if (mode != 0) {
    isOn = false;
    return;
  }

  if (!isOn) {
    // 노랑 LED 켜기
    analogWrite(YELLOW_LED, ledBrightness);
    yellow = 1;
    startTime = millis();
    isOn = true;
  } else {
    // 500ms 후 노랑 LED 끄기
    if (millis() - startTime >= yellowTime) {
      analogWrite(YELLOW_LED, 0);
      yellow = 0;
      isOn = false;
      tYellowLED2.disable();
      tRedLED.enable(); // 다시 빨강 LED로 루프
    }
  }
}


void taskMode1() {
  if (mode == 1) {
    analogWrite(RED_LED, ledBrightness);
    red = 1;
  }
}

void taskAllLED() {
  static unsigned long toggleTimer = 0;
  static bool ledOn = false;
  
  if (mode != 2) { ledOn = false; return; }
  
  if (millis() - toggleTimer >= 500UL) {
    toggleTimer = millis();
    ledOn = !ledOn;
    if (ledOn) {
      analogWrite(RED_LED, ledBrightness);
      analogWrite(YELLOW_LED, ledBrightness);
      analogWrite(GREEN_LED, ledBrightness);
      red = yellow = green = 1;
    } else {
      analogWrite(RED_LED, 0);
      analogWrite(YELLOW_LED, 0);
      analogWrite(GREEN_LED, 0);
      red = yellow = green = 0;
    }
  } else {
    if (ledOn) {
      analogWrite(RED_LED, ledBrightness);
      analogWrite(YELLOW_LED, ledBrightness);
      analogWrite(GREEN_LED, ledBrightness);
    }
  }
}
