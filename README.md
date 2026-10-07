# IoT Assessment - Task 1 (ESP-IDF) and Task 2 (STM32)

Hi, this is my submission for the IoT assessment.
I did **Task 1 Option A (ESP-IDF)** and **Task 2 (STM32 HAL)**.
I wrote everything in plain C. I am still a beginner in embedded
programming, so I kept the code simple and added comments on every part.

**Status:** Task 1 is complete and tested on hardware. Task 2 is written but only
partly tested (see the Task 2 section for the honest details).

## Files

    iot_assignment/
    |-- README.md
    |-- .gitignore
    |-- CMakeLists.txt, main/            (Task 1: ESP-IDF)
    |   |-- CMakeLists.txt
    |   |-- idf_component.yml            (adds the MQTT library)
    |   `-- main.c
    `-- iot_assignment_stm32/            (Task 2: STM32)
        |-- iot_assignment_stm32.ioc     (CubeMX settings)
        `-- Core/Src/main.c              (all the code for this task)

---

# Task 1 - Wi-Fi + MQTT heartbeat (Option A: ESP-IDF)

## What the program does

1. On start it looks in flash memory (NVS) for a saved Wi-Fi name and password.
2. If nothing is saved, it asks me to type them in the serial monitor and saves them.
   **No Wi-Fi details are written inside the code.**
3. It connects to the Wi-Fi network. If the connection is lost, it tries again.
4. After it gets an IP address, it starts SNTP and gets the real time from the internet.
5. A FreeRTOS task sends a heartbeat message to a public MQTT broker every 5 seconds.

The heartbeat is a small JSON message, for example:

    {"device_id":"ESP32","uptime":12,"status":"online","timestamp":1791196271}

## Board and tools

- Board: ESP32 (WROOM). I do not have an ESP32-C6 board, so I tested on this one.
- ESP-IDF version: v6.1
- Editor: VS Code with the ESP-IDF extension
- Network used for testing: phone hotspot (2.4 GHz)
- MQTT broker: HiveMQ public broker, `broker.hivemq.com` (port 1883)
- Topic: `iot_assignment/dinesh/heartbeat`

## How to build and flash

In ESP-IDF v6.x the MQTT library is not inside ESP-IDF anymore, so it is added as a
dependency (already listed in `main/idf_component.yml`). If it is missing, run once:

    idf.py add-dependency "espressif/mqtt"

Then:

    idf.py set-target esp32
    idf.py build
    idf.py -p COM7 flash monitor

Change `COM7` to the port of your board. To leave the monitor press `Ctrl+]`.

## How to give the Wi-Fi details

1. The first time, the monitor shows `Enter Wi-Fi SSID:`.
2. Type the network name and press Enter.
3. Type the password and press Enter.
4. The details are saved. After a reboot it does not ask again.

To enter different details, erase the flash and start again:

    idf.py -p COM7 erase-flash

Note: the ESP32 works only with 2.4 GHz Wi-Fi.

## How I check that it works
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/57d89561-7846-49a9-a08c-243702647605" />

I opened the HiveMQ web client (https://www.hivemq.com/demos/websocket-client/),
connected to `broker.hivemq.com`, and subscribed to
`iot_assignment/dinesh/heartbeat`. A message arrives every 5 seconds.

On a computer with Mosquitto installed this also works:

    mosquitto_sub -h broker.hivemq.com -t iot_assignment/dinesh/heartbeat

## What I tested

- [x] First boot asks for Wi-Fi details and saves them
- [x] Wi-Fi connects and gets an IP address
- [x] SNTP time is correct (the timestamp matches the real date)
- [x] Heartbeat messages arrive in the HiveMQ web client every 5 seconds
- [x] The device kept running and sending for a long time (uptime reached 1139 s, about 19 minutes, in one run)
- [x] Wi-Fi loss is detected (log shows `reason code: 201`) and the device reconnects by itself, and heartbeats are skipped while the network is down
- [x] After a reboot (EN button) it does not ask for Wi-Fi details again and connects automatically

## What I used from FreeRTOS

- One task (`heartbeat_task`) that sends the message every 5 seconds with `vTaskDelay`.
- Two simple flags (`wifi_connected`, `mqtt_connected`) so the task sends only when both are true.

## Limitations (Task 1)

- Reconnect uses a fixed short wait. A better way is to wait longer after each failure.
- If the saved password is wrong, the device keeps trying. I fix it by running `erase-flash`.
- The heartbeat uses `vTaskDelay`, so the time can drift a little. `vTaskDelayUntil` would be more exact.
- MQTT is not encrypted (plain `mqtt://`) and the Wi-Fi password is stored as plain text in flash.
  A real product should use TLS and flash encryption.
- If the network is down, heartbeats are skipped, not stored.
- Empty passwords (open networks) are not supported.
- I tested only on ESP32 (WROOM), not on ESP32-C6.

---

# Task 2 - STM32 ADC sampling, moving average, UART

## What the program does

1. A timer (TIM3) interrupt fires every **100 ms (10 Hz)** and only sets a flag.
   This keeps the interrupt short and the sampling time exact.
2. The main loop sees the flag, reads the ADC, and clears the flag.
3. A **moving average of 10 samples** is calculated with a circular buffer and a running total:
   remove the oldest reading from the total, store the new one, add it to the total,
   then move to the next place (after place 9 go back to 0).
   For the first 10 samples it divides only by the readings collected so far.
4. The raw value and the average are sent over UART at **115200 baud** as readable text:

        raw=2048 avg=2031

5. The return value of ADC start, ADC conversion and UART transmit is checked.
   On an error it prints an error message or turns the LED on.

## Board and tools

- Board: NUCLEO-C031C6 (STM32C031C6)
- Tools: STM32CubeMX settings (`.ioc`), STM32 HAL, VS Code with the STM32 extension
- Main code is inside the `USER CODE` blocks of `Core/Src/main.c`, so CubeMX can regenerate without deleting it.
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/b52e8dcf-3916-48b8-8b2d-2409f718a78e" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/771fe476-365e-4603-804f-6177db62a17e" />

## Honest status

- I do **not** have the board, so it was **not run on hardware**.
- I had problems with the build tools on my PC (CMake and the ARM compiler were not found),
  so I could **not complete a full build** in the time I had.
- The code logic is complete and commented. Please review the approach in `main.c`.

## What is still to check

- [ ] Build with 0 errors
- [ ] TIM3 prescaler and period give exactly 100 ms
- [ ] Output `raw=... avg=...` appears every 100 ms in a serial terminal (or simulator)

## Limitations (Task 2)

- The ADC is read by polling in the main loop. DMA would use less CPU.
- If the main loop is busy for more than 100 ms, one sample can be missed (the flag is only a single flag).
- The average uses whole numbers, so the decimal part is dropped.
- Not tested on real hardware.

---

## What I learned

This was my first time using ESP-IDF, NVS, SNTP and MQTT together, and my first time with
STM32 HAL and CubeMX. I learned how events work (Wi-Fi and MQTT tell my code when something
changes), how to debug a connection using Wi-Fi reason codes, and how to keep an interrupt
short by using a flag. I also learned that setting up the build tools is part of the job.
