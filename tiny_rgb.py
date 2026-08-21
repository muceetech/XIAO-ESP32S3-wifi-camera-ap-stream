import cv2
import time

# Replace with the IP shown by ESP32-S3
CAMERA_URL = "http://192.168.4.1/stream"

def open_camera():
    print("Connecting to ESP32-S3...")

    cap = cv2.VideoCapture(
        CAMERA_URL,
        cv2.CAP_FFMPEG
    )

    if not cap.isOpened():
        print("Camera connection failed")
        cap.release()
        return None

    print("Camera connected")
    return cap


cap = open_camera()

if cap is None:
    raise SystemExit


while True:

    ret, frame = cap.read()

    if not ret:

        print("Frame failed - reconnecting...")

        cap.release()

        time.sleep(0.5)

        cap = open_camera()

        if cap is None:
            time.sleep(1)
            continue

        continue


    print(
        "Frame:",
        frame.shape
    )

    cv2.imshow(
        "ESP32-S3 Camera",
        frame
    )

    key = cv2.waitKey(1) & 0xFF

    if key == 27 or key == ord("q"):
        break


cap.release()
cv2.destroyAllWindows()
