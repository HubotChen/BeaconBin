import paho.mqtt.client as mqtt
import json
from datetime import datetime
import time
import random
from pynput import keyboard  # New import

# 1. Configuration
BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC = "eece5155/Hubert/data"

# Global control variables
paused = False

def on_press(key):
    global paused
    try:
        if key.char == '`':  # Press '`' to toggle pause
            paused = not paused
            state = "PAUSED" if paused else "RESUMED"
            print(f"\n--- Simulation {state} ---")
    except AttributeError:
        pass

# Start the keyboard listener in the background
listener = keyboard.Listener(on_press=on_press)
listener.start()

# 2. Setup the client
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="esp32-manual-test")
print(f"Connecting... (Press '`' to pause/resume)")
client.connect(BROKER, PORT)

try:
    while True:
        if not paused:
            device_num = random.randint(1, 5)
            data = {
                "device_id": f"node-{device_num:03d}",
                "timestamp": datetime.now().isoformat(),
                "sensors": {
                    "weight": {"value": random.randint(0, 4000)},
                    "distance": {"value": 1001},
                    "cancel": {"value": 0},
                    "confirm": {"value": 0}
                }
            }
            client.publish(TOPIC, json.dumps(data))
            print(f"Sent packet for node-{device_num:03d}", end='\r')
        
        time.sleep(1) 

except KeyboardInterrupt:
    print("\nShutting down simulation...")
finally:
    client.disconnect()
    listener.stop()