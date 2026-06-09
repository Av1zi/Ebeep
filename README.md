# Ebeep

> A pair of handmade wireless devices that let two people send notes to each other across any distance, no phone required!*

---

## What is Ebeep?

Ebeep is a DIY electronics project: two small standalone devices, one for you and one for someone you love. Each device has an e-ink display, three mechanical buttons, and a buzzer. You type a message on yours, press send — and a few seconds later, their device buzzes, lights up, and shows your words. No app, no phone, no subscription. Just a little box that sits on a desk and occasionally says *I love you*.

The devices communicate over WiFi via MQTT, meaning they work across any distance — same room, different city, different country.

---

## Features

-  **Send & receive messages** — type freehand using a 3-button scrollable alphabet interface
-  **Buzzer notification** — plays a melody when a new message arrives
-  **Battery powered** — LiPo battery with charging over USB-C, battery % always visible on screen
-  **Works across countries** — communicates over WiFi via MQTT (HiveMQ Cloud free tier)
-  **E-ink display** — for always on display and for ultra low power draw
-  **Expandable** — a Games slot is reserved in the UI for future additions

---

## Hardware

| Component | Part |
|---|---|
| Microcontroller | ESP32 |
| Display | Waveshare e-ink (2.9") |
| Battery | 3.7V LiPo, 3000mAh |
| Buttons | 3× low profile mechanical keyboard switches |
| Buzzer | Passive piezo buzzer |

---

## How It Works

### Communication
Both devices connect to WiFi and talk to a private **MQTT broker**. Each device subscribes to its own topic and publishes to the other's. Messages are JSON payloads containing the text and sender ID.

### WiFi Setup (captive portal)
On first boot, the device creates a hotspot called `Ebeep-Setup`. Connect to it from any phone and a setup page appears automatically — pick your WiFi network, enter the password, done. Credentials are saved to flash and the device never asks again.

### Typing Interface
Messages are typed using a 3-button scrollable alphabet:
- **Left / Right** — scroll through characters (`_ A B C D E F G H I ...`)
- **Confirm** — select the current character
- **Two spaces in a row** — sends the message

---

## Project Status

- [x] Mockup version for the UI (finishing touches)
- [ ] Wiring design complete
- [ ] MQTT publish/subscribe
- [ ] WiFiManager captive portal integration
- [X] Typing UI with scrollable alphabet
- [x] Full screen state machine
- [ ] Buzzer melodies
- [ ] Enclosure / physical build

---

## Why "Ebeep"?

**E** for e-ink. **Beep** for the little buzz it makes when someone is thinking of you (also for beeper).

---

## License

GPL-3.0 license — do whatever you want with it, just make sure its open source as well.
