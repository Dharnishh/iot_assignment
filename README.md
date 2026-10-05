# IoT Assessment - Task 1 (Option A: ESP-IDF)

Hi, this is my submission for Task 1, Option A.
I wrote it in plain C using ESP-IDF. I am still a beginner in embedded
programming, so I kept the code simple and added comments on every part
so it is easy to follow.

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

## Files

    iot_assignment/
    |-- CMakeLists.txt
    |-- README.md
    |-- .gitignore
    `-- main/
        |-- CMakeLists.txt
        |-- idf_component.yml    (adds the MQTT library)
        `-- main.c               (all the code for this task)

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
![Proof](image.png)
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

Screenshots / logs: [add the HiveMQ screenshot and a short log here]

## What I used from FreeRTOS

- One task (`heartbeat_task`) that sends the message every 5 seconds with `vTaskDelay`.
- Two simple flags (`wifi_connected`, `mqtt_connected`) so the task sends only when both are true.

## Limitations (honest list)

- Reconnect uses a fixed short wait. A better way is to wait longer after each failure.
- If the saved password is wrong, the device keeps trying. I fix it by running `erase-flash`.
  A better way is to clear the saved details automatically after several wrong-password failures.
- The heartbeat uses `vTaskDelay`, so the time can drift a little. `vTaskDelayUntil` would be more exact.
- MQTT is not encrypted (plain `mqtt://`) and the Wi-Fi password is stored as plain text in flash.
  A real product should use TLS and flash encryption.
- If the network is down, heartbeats are skipped, not stored. A real device should store them and send later.
- Empty passwords (open networks) are not supported because my input function asks again for an empty line.
- I tested only on ESP32 (WROOM), not on ESP32-C6.

## What I learned

This was my first time using ESP-IDF, NVS, SNTP and MQTT together.
I learned how events work (Wi-Fi and MQTT tell my code when something changes),
how to debug a connection using the Wi-Fi reason codes, and why the ESP32 needs
a 2.4 GHz network.