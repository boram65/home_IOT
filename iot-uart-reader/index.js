const { SerialPort, ReadlineParser } = require("serialport");

const awsIoT = require("aws-iot-device-sdk");

let deviceConnectFlag = false;

console.log("Create AWS IoT Device");
const device = awsIoT.device({
  keyPath: "config/IOT_2018475007.private.key",
  certPath: "config/IOT_2018475007.cert.pem",
  caPath: "config/root-CA.crt",
  clientId: "IOT_2018475007",
  host: "a3qnuwxrv86n7d-ats.iot.ap-northeast-2.amazonaws.com",
  keepalive: 10,
});

device.on("connect", (connack) => {
  console.log("AWS Connected");
  deviceConnectFlag = true;
  device.subscribe("Arduino/ResponseRfidVerify");
});

device.on("message", (topic, payload) => {
  console.log("Received Topic: " + topic);
  console.log("Received Message: " + payload);

  if (topic === "Arduino/ResponseRfidVerify") {
    arduinoPort.write(payload);
    arduinoPort.write("\n");
  }
});

device.on("close", (err) => {
  console.log("Device Close: " + err);
  deviceConnectFlag = false;
});

device.on("reconnect", () => {
  console.log("Device Reconnect");
  deviceConnectFlag = true;
});

device.on("offline", () => {
  console.log("Device Offline");
  deviceConnectFlag = false;
});

device.on("error", (err) => {
  console.log("Device Error: " + err);
  deviceConnectFlag = false;
});

console.log("Create Arduino Port");
const arduinoPort = new SerialPort(
  { path: "COM6", baudRate: 9600, autoOpen: false },
  (err) => {
    console.log("Error: " + err.message);
  }
);

console.log("Open Arduino Port");
arduinoPort.open((err) => {
  if (err) {
    console.log("Open Error: " + err.message);
  }
});

console.log("Add Open Message Listener");
arduinoPort.on("open", () => {
  console.log("Arduino Port Opened");
});

const lineParser = new ReadlineParser();
arduinoPort.pipe(lineParser);
lineParser.on("data", (data) => {
  try {
    data = JSON.parse(data.trim());
  } catch (e) {
    return;
  }

  if (!deviceConnectFlag) return;

  const messageType = data.messageType;
  if (messageType === undefined) return true;
  if (messageType === "readHumidity") {
    device.publish("Arduino/SendTempHumidityLog", JSON.stringify(data));
  } else if (messageType === "readRfid") {
    device.publish("Arduino/RequestRfidVerify", JSON.stringify(data));
  } else if (messageType === "readDust") {
    device.publish("Arduino/SendDustLog", JSON.stringify(data));
  } else if (messageType == "WindowControl") {
    device.publish("Arduino/WindowControl", JSON.stringify(data));
  }
});
