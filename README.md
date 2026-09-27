# Smart-prepaid-energy-meter
A smart prepaid energy metering system built using the PZEM-004T sensor module, featuring real-time monitoring, automatic load management, theft detection, and a dynamic tariff system designed to modernize energy billing and prevent power theft.
# Prepaid Smart Energy Meter using PZEM-004T

A smart prepaid energy metering system built using the PZEM-004T sensor module, featuring real-time monitoring, automatic load management, theft detection, and a dynamic tariff system — designed to modernize energy billing and prevent power theft.

---

##  Features

- Real-Time Energy Monitoring — Continuously measures voltage, current, power, and energy consumption using the PZEM-004T sensor
- *Prepaid System* — Users preload credit; consumption is tracked and deducted in real time
- *Threshold-Based Load Shedding* — When balance drops below a set threshold, a 5-second buzzer alert sounds and non-essential loads are automatically cut off, while essential loads remain active
- *Theft Detection* — Detects abnormal current flow or tampering; triggers a 10-second buzzer alert and shuts down the entire system
- *Essential vs Non-Essential Load Classification* — Loads are categorized so critical appliances always stay powered during low balance situations
- *Dynamic Tariff System* — Operator can adjust electricity tariff rates in real time through the system
- *Monitoring Dashboard* — Visual interface displaying live consumption data, balance, and system status

---

##  Hardware Used

- PZEM-004T Energy Sensor Module
- Microcontroller (Arduino / NodeMCU)
- Buzzer
- Relay modules (for load switching)
- Display module(TFT 1.6 inch)
- Power supply unit

---

##  Software & Tools

- Arduino IDE
- VS code

---

##  How It Works

1. The PZEM-004T sensor continuously monitors energy parameters in real time
2. Consumed units are deducted from the user's prepaid balance
3. When balance falls below the threshold:
   - Buzzer sounds for *5 seconds*
   - Non-essential loads are *automatically disconnected*
   - Essential loads continue to function normally
4. If theft or tampering is detected:
   - Buzzer sounds for *10 seconds*
   - The *entire system shuts down*
5. The operator can update tariff rates dynamically without resetting the system
6. All data is displayed live on the monitoring dashboard

---

##  System Architecture

The system works in the following flow:

1. *PZEM-004T Sensor* reads live voltage, current, power and energy data
2. *Microcontroller* processes the data and tracks prepaid balance
3. *Balance Tracking Engine* continuously deducts consumed units from credit
4. *Threshold Check* — if balance goes low:
   - Buzzer sounds for 5 seconds
   - Non-essential loads are cut off via relay
   - Essential loads stay ON
5. *Theft Detection Module* monitors abnormal current:
   - Buzzer sounds for 10 seconds
   - Entire system shuts down
6. *Dynamic Tariff System* allows operator to update rates anytime
7. *Monitoring Dashboard displays live data:
   - Voltage and current readings
   - Power consumed
   - Balance remaining
   - System status
---

##  Author

*ARYAN RAM VINAYAK YADAV*:
Electrical Engineering, VJTI Mumbai

---

##  Problem Statement

- Eliminates manual meter reading
- Prevents electricity theft automatically
- Ensures critical appliances stay on even during low balance
- Gives operators flexibility to change tariff rates dynamically
- Automates load shedding intelligently
