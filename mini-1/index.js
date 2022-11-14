const express = require("express");
const mysql = require("mysql");
const dbconfig = require("./config/database");
const connection = mysql.createConnection(dbconfig);
const awsIoT = require("aws-iot-device-sdk");

const app = express();
const bodyParser = require("body-parser");

let deviceConnectFlag = false;

const device = awsIoT.device({
  keyPath: "config/IOT_2018475007.private.key",
  certPath: "config/IOT_2018475007.cert.pem",
  caPath: "config/root-CA.crt",
  clientId: "IOT_2018475007/arduino",
  host: "a3qnuwxrv86n7d-ats.iot.ap-northeast-2.amazonaws.com",
  keepalive: 10,
});

device.on("connect", (connack) => {
  console.log("AWS IoT Connected");
  deviceConnectFlag = true;
  device.subscribe("Arduino/SendTempHumidityLog");
  device.subscribe("Arduino/RequestRfidVerify");
  device.subscribe("Arduino/SendDustLog");
  device.subscribe("Arduino/WindowControl");
});

device.on("message", (topic, payload) => {
  console.log("Received Topic: " + topic);
  console.log("Received Message: " + payload);
  payload = JSON.parse(payload);

  // Topic "Arduino/SendTempHumidityLog"가 입력되었을 때
  if (topic === "Arduino/SendTempHumidityLog") {
    connection.query(
      "INSERT INTO humidity (humidity, temp) VALUES (?,?)",
      [payload.temp, payload.humidity],
      (err) => {}
    );
  } else if (topic == "Arduino/RequestRfidVerify") {
    const rfidTag = payload.rfidTag;

    connection.query(
      "SELECT * FROM rfid WHERE tag = ?",
      [rfidTag],
      (err, rows) => {
        if (err) return;

        if (rows.length == 0) {
          connection.query(
            "INSERT INTO rfid_log (tag, message, name) VALUES (?, ?, ?)",
            [rfidTag, "Not Registered Tag", null],
            (err) => {
              if (err) console.log(err);
            }
          );

          device.publish(
            "Arduino/ResponseRfidVerify",
            JSON.stringify({
              type: "rfid",
              result: 0,
            })
          );
        } else {
          rfid = rows[0];

          connection.query(
            "INSERT INTO rfid_log (tag, message, name) VALUES (?, ?, ?)",
            [rfidTag, "Success", rfid.name],
            (err) => {
              if (err) console.log(err);
            }
          );

          device.publish(
            "Arduino/ResponseRfidVerify",
            JSON.stringify({
              type: "rfid",
              result: 1,
            })
          );
        }
      }
    );
  } else if (topic === "Arduino/SendDustLog") {
    connection.query(
      "INSERT INTO dustsensor (dust) VALUE (?)",
      [payload.dust],
      (err) => {
        console.log(err);
      }
    );
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

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

app.get("/api", (req, res) => {
  res.json({
    result: true,
  });
});

app.get("/api/status", (req, res) => {
  res.json({
    result: true,
    message: "Connect Success",
  });
});

app.get("/api/users", (req, res) => {
  connection.query("SELECT * FROM users", (err, rows) => {
    if (err) throw err;
    res.json({
      result: true,
      data: rows,
    });
  });
});

app.get("/api/humidity_log", (req, res) => {
  connection.query("SELECT * FROM humidity", (err, rows) => {
    if (err) throw err;
    res.json({
      result: true,
      data: rows,
    });
  });
});

app.get("/api/access_log", (req, res) => {
  connection.query(
    "SELECT * FROM rfid_log ORDER BY created_datetime DESC LIMIT 5",
    (err, rows) => {
      if (err) throw err;
      res.json({
        result: true,
        data: rows,
      });
    }
  );
});

app.get("/api/dustlog", (req, res) => {
  connection.query(
    "SELECT * FROM dustsensor ORDER BY create_time DESC",
    (err, rows) => {
      if (err) throw err;
      res.json({
        result: true,
        data: rows,
      });
    }
  );
});

app.get("api/window_control", (req, res) => {
  const active = req.body.active;
  device.publish(
    "Arduino/WindowControl",
    JSON.stringify({
      type: "window",
      active: active,
    })
  );
});

// app.post("/api/sensor_logs", (req, res) => {
//   console.log(req.body);
//   const temp = req.body.temp;
//   const humidity = req.body.humidity;

//   connection.query(
//     "INSERT INTO sensor_log (temp, humidity) VALUES (?,?)",
//     [temp, humidity],
//     (err) => {
//       if (err) {
//         res.statusCode = 500;
//         res.json({
//           result: false,
//           message: err.message,
//         });
//       } else {
//         res.json({
//           result: true,
//           message: "Success",
//         });
//       }
//     }
//   );
// });

app.post("/api/makeacc", (req, res) => {
  console.log(req.body.rfidTag);
  const name = req.body.name;
  const rfidTag = req.body.rfidTag;

  connection.query(
    "SELECT * FROM rfid WHERE tag = ?",
    [rfidTag],
    (err, rows) => {
      if (err) {
        res.statusCode = 500;
        res.json({
          result: false,
          message: err.message,
        });
      } else if (rows.length !== 0) {
        res.statusCode = 200;
        res.json({
          result: false,
          message: "already accounted!",
        });
      } else {
        connection.query(
          "INSERT INTO rfid (tag, name) values (?,?)",
          [rfidTag, name],
          (err) => {
            if (err) {
              res.statusCode = 500;
              res.json({
                result: false,
                message: err.message,
              });
            } else {
              res.statusCode = 200;
              res.json({
                result: true,
                message: "success",
              });
            }
          }
        );
      }
    }
  );
});

app.listen(3000, () => {
  console.log("Run Hello World on Port 3000");
});
