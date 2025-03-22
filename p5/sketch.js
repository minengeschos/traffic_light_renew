// 전역 변수 선언
let port;
let portConnected = false;
let connectButton;
let latestData = ""; // 누적된 시리얼 데이터
let serial;        // p5.SerialPort 객체

// LED 상태 및 추가 정보 (모드, 밝기)
let red = 0, yellow = 0, green = 0, ledBrightness = 0, mode = 0;
let brightnessValue = "";
let modeValue = "";
let ledState = [0, 0, 0]; // (옵션: 사용중인 경우)

// 슬라이더 요소 (HTML에 정의되어 있음)
let rSlider, ySlider, gSlider;

function setup() {
  noCanvas();

  // "Connect to Arduino" 버튼 참조 및 이벤트 등록
  connectButton = document.getElementById("connectButton");
  connectButton.addEventListener("click", connectSerial);

  // 슬라이더 요소 참조
  rSlider = document.getElementById("rSlider");
  ySlider = document.getElementById("ySlider");
  gSlider = document.getElementById("gSlider");

  // 슬라이더 값 변경 시, 오른쪽에 ms 표시를 업데이트하고 전송함
  rSlider.addEventListener("input", updateSliderDisplay);
  ySlider.addEventListener("input", updateSliderDisplay);
  gSlider.addEventListener("input", updateSliderDisplay);

  // p5.SerialPort 객체 생성 및 이벤트 핸들러 등록
  serial = new p5.SerialPort();
  serial.on('data', gotData);
  serial.on('open', onConnect);
  serial.on('close', onDisconnect);
  serial.on('error', onError);
}

/**
 * 슬라이더의 현재 값을 오른쪽의 <span>에 표시하고,
 * 변경된 값을 전송합니다.
 */
function updateSliderDisplay() {
  document.getElementById("rSliderValue").textContent = rSlider.value + " ms";
  document.getElementById("ySliderValue").textContent = ySlider.value + " ms";
  document.getElementById("gSliderValue").textContent = gSlider.value + " ms";
  sendSliderValues();
}

/**
 * 슬라이더 값(빨강, 노랑, 초록 지속시간)과 현재 모드(mode)를 CSV 형식으로 Arduino에 전송하는 비동기 함수
 * 형식 예: "500,500,500,0\n"
 */
async function sendSliderValues() {
  if (portConnected && port && port.writable) {
    const encoder = new TextEncoder();
    // 슬라이더 값들 (프로퍼티 접근)
    let rValue = rSlider.value;
    let yValue = ySlider.value;
    let gValue = gSlider.value;
    let dataToSend = rValue + "," + yValue + "," + gValue + "," + mode + "\n";
    
    console.log("Sending slider values:");
    console.log("  Red Time:", rValue, "ms");
    console.log("  Yellow Time:", yValue, "ms");
    console.log("  Green Time:", gValue, "ms");
    console.log("  Mode:", mode);
    console.log("  Data Sent:", dataToSend);
    
    try {
      await writerWrite(dataToSend, encoder);
    } catch (error) {
      console.error("Error sending data to Arduino:", error);
    }
  } else {
    console.warn("Port not connected or writable. Cannot send slider values.");
  }
}

/**
 * 도우미 함수: 주어진 문자열을 인코딩하고 시리얼 포트에 씁니다.
 */
async function writerWrite(text, encoder) {
  const writer = port.writable.getWriter();
  await writer.write(encoder.encode(text));
  writer.releaseLock();
}

/**
 * 시리얼 포트를 선택하고 연결하는 async 함수  
 * 연결 후 9600 baud로 포트를 열고 상태를 업데이트합니다.
 */
async function connectSerial() {
  try {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 9600 });
    portConnected = true;
    connectButton.innerHTML = "Serial Connected";
    document.getElementById("serialInfo").textContent = "Status: Connected";
    console.log("Successfully connected to serial device!");
    readLoop();
  } catch (error) {
    console.log("Serial connection error: " + error);
  }
}

/**
 * 시리얼 데이터를 지속적으로 읽어들이는 async 함수  
 * 데이터를 텍스트로 디코딩한 후 개행 문자 기준으로 분리하여 processSerialData() 호출
 */
async function readLoop() {
  const decoder = new TextDecoder();
  while (port.readable) {
    const reader = port.readable.getReader();
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) {
          latestData += decoder.decode(value);
          if (latestData.indexOf("\n") !== -1) {
            let lines = latestData.split("\n");
            for (let i = 0; i < lines.length - 1; i++) {
              let line = lines[i].trim();
              if (line.length > 0) {
                processSerialData(line);
              }
            }
            latestData = lines[lines.length - 1];
          }
        }
      }
    } catch (error) {
      console.error("Error reading from serial port: " + error);
    } finally {
      reader.releaseLock();
    }
  }
}

/**
 * 시리얼로부터 받은 JSON 문자열 데이터를 파싱하는 함수  
 * 예상 데이터 형식: {"mode":0,"red":1,"yellow":0,"green":0,"brightness":255}
 */
function processSerialData(dataStr) {
  console.log("Received serial data:", dataStr);
  try {
    let state = JSON.parse(dataStr);
    mode = parseInt(state.mode, 10);
    red = parseInt(state.red, 10);
    yellow = parseInt(state.yellow, 10);
    green = parseInt(state.green, 10);
    ledBrightness = parseInt(state.brightness, 10);
    updateInfoDisplay();
    updateModeDisplay();
    updateIndicators();
    updateBrightnessDisplay();
  } catch (error) {
    console.error("Error parsing JSON:", error);
  }
}

/**
 * 디버깅 및 정보 출력 함수
 */
function updateInfoDisplay() {
  console.log("Brightness:", ledBrightness, "Mode (num):", mode);
}

/**
 * 현재 모드를 화면에 표시하는 함수  
 * 모드에 따라 사용자 친화적인 문자열로 업데이트합니다.
 */
function updateModeDisplay() {
  const modeInfo = document.getElementById("modeInfo");
  let modeText = "";
  switch (mode) {
    case 0:
      modeText = "Default Mode";
      break;
    case 1:
      modeText = "Emergency Mode (Red)";
      break;
    case 2:
      modeText = "Blinking Mode";
      break;
    case 3:
      modeText = "On/Off Mode";
      break;
    default:
      modeText = "Unknown Mode";
      break;
  }
  modeInfo.textContent = "Mode: " + modeText;
}

/**
 * 가변저항(밝기) 값을 0~225 범위의 값으로 100% 환산하여 화면에 표시하는 함수  
 * (예: ledBrightness가 225이면 100%, 112이면 약 50%)
 */
function updateBrightnessDisplay() {
  const brightnessInfo = document.getElementById("brightnessInfo");
  let brightnessPercent = (ledBrightness / 225) * 100;
  if (brightnessPercent > 100) brightnessPercent = 100;
  brightnessInfo.textContent = "Brightness: " + brightnessPercent.toFixed(0) + "%";
}

/**
 * 신호등 인디케이터를 업데이트하는 함수  
 * 각 LED 상태에 따라 인디케이터의 배경색을 설정합니다.
 */
function updateIndicators() {
  console.log("Updating indicators - Red:", red, "Yellow:", yellow, "Green:", green);
  
  // 만약 ledState 배열이 사용된다면 아래 두 줄은 주석처리하고, 전역 변수 사용:
  // const redIndicator = document.getElementById("red-indicator");
  // const yellowIndicator = document.getElementById("yellow-indicator");
  // const greenIndicator = document.getElementById("green-indicator");
  
  // 여기서는 전역 변수 red, yellow, green을 사용하여 색상을 결정합니다.
  const redIndicator = document.getElementById("red-indicator");
  const yellowIndicator = document.getElementById("yellow-indicator");
  const greenIndicator = document.getElementById("green-indicator");

  redIndicator.style.backgroundColor = (red === 1) ? "red" : "gray";
  yellowIndicator.style.backgroundColor = (yellow === 1) ? "yellow" : "gray";
  greenIndicator.style.backgroundColor = (green === 1) ? "green" : "gray";
}

/**
 * 시리얼 포트가 열리면 호출되어 연결 상태 UI를 업데이트합니다.
 */
function onConnect() {
  const status = document.getElementById("serialInfo");
  status.textContent = "Status: Connected";
  console.log("Serial port connected.");
}

/**
 * 시리얼 포트가 닫히면 호출되어 연결 상태 UI를 업데이트합니다.
 */
function onDisconnect() {
  const status = document.getElementById("serialInfo");
  status.textContent = "Status: Disconnected";
  console.log("Serial port disconnected.");
}

/**
 * 시리얼 포트 오류 발생 시 호출됩니다.
 */
function onError(error) {
  console.error("Serial Port Error:", error);
}
