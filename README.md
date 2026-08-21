# XIAO ESP32S3 WiFi Camera AP Stream

This project implements a **standalone WiFi Access Point (AP) camera server** using the **Seeed Studio XIAO ESP32S3 Sense**.

The ESP32-S3 creates its own WiFi network and streams live camera frames over HTTP. No external WiFi router or internet connection is required.

## Features

* XIAO ESP32S3 Sense onboard OV2640 camera
* ESP32-S3 operates as a WiFi Access Point
* Live MJPEG camera streaming over HTTP
* Access the camera from a PC, laptop, phone, or Raspberry Pi
* No external WiFi router required
* Suitable for embedded computer-vision and depth-estimation projects

## Hardware

* XIAO ESP32S3 Sense
* USB cable for programming and power

## WiFi Configuration

The ESP32-S3 creates its own WiFi network.

Default configuration:

```text
WiFi Mode: Access Point (AP)
AP IP Address: 192.168.4.1
```

Connect your computer or other device to the WiFi network created by the ESP32-S3.

Then open:

```text
http://192.168.4.1
```

in a web browser.

## Camera Streaming

The camera provides a live MJPEG stream through the ESP32-S3 web server.

The stream can be accessed from:

* Web browser
* Python/OpenCV
* Raspberry Pi
* Computer vision applications

Example Python/OpenCV usage:

```python
import cv2

url = "http://192.168.4.1/stream"

cap = cv2.VideoCapture(url)

while True:
    ret, frame = cap.read()

    if not ret:
        continue

    cv2.imshow("XIAO ESP32S3 Camera", frame)

    if cv2.waitKey(1) & 0xFF == 27:
        break

cap.release()
cv2.destroyAllWindows()
```

## Project Structure

```text
XIAO-ESP32S3-WiFi-Camera/
│
├── firmware/
│   └── xiao_esp32s3_camera_ap.ino
│
├── python/
│   └── camera_stream.py
│
├── README.md
└── .gitignore
```

## Arduino IDE Setup

Install the ESP32 board package in Arduino IDE.

Select:

```text
Board:
XIAO ESP32S3 Sense
```

Connect the board through USB and upload the firmware.

After booting, the ESP32-S3 starts the WiFi Access Point and camera server.

## Basic Operation

```text
        XIAO ESP32S3 Sense
              │
        OV2640 Camera
              │
              ▼
       ESP32-S3 Camera
              │
       WiFi Access Point
              │
       192.168.4.1
              │
       ┌──────┴──────┐
       │             │
       ▼             ▼
    Laptop       Raspberry Pi
       │             │
       └──────┬──────┘
              ▼
        MJPEG Stream
```

## Use in Computer Vision

This camera server is intended to be used as a lightweight wireless camera source for embedded vision experiments.

A possible system architecture is:

```text
XIAO ESP32S3
     │
     │ WiFi MJPEG
     ▼
Raspberry Pi / PC
     │
     ├── RGB Image
     ├── Monocular Depth Model
     ├── ToF Depth Sensor
     └── Depth Fusion
              │
              ▼
        Fused Depth Map
```

This makes the project useful as the camera component of an **RGB + ToF depth-fusion system**.

## Known Behavior

The ESP32-S3 WiFi AP/stream can occasionally experience a temporary connection drop during continuous streaming. Reconnecting the client to the AP restores communication.

The project is currently focused on establishing a reliable wireless camera stream for further computer-vision and depth-fusion development.

## Future Improvements

* Automatic WiFi client reconnection
* Stream stability improvements
* Adjustable JPEG quality
* Adjustable frame rate
* Resolution selection
* Camera configuration through web interface
* Integration with ToF depth sensors
* Integration with Python depth-estimation models
* RGB + depth visualization

## License

This project is intended for research, experimentation, and educational use.
