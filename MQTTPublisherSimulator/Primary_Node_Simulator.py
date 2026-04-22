import paho.mqtt.client as mqtt
import json
from datetime import datetime
import time

# 1. Configuration
BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC = "eece5155/Hubert/data"  # Use the same topic as your Node-RED MQTT node

# 2. Setup the client
# Note: Using CallbackAPIVersion.VERSION2 for the latest paho-mqtt library
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="esp32-manual-test")
print(f"Connecting...")
# 3. Connect and Publish
client.connect(BROKER, PORT)

print(f"Connected")
# The exact JSON structure your Node-RED function is expecting
data1 = {
    "device_id": "esp32-node-02",
    "timestamp": datetime.now().isoformat(),
    "sensors": {
        "weight": {"value": 600},
        "distance": {"value": 1001},
        "cancel": {"value": 0},
        "confirm": {"value": 0}
    }
}
data2 = {
    "device_id": "esp32-node-03",
    "timestamp": datetime.now().isoformat(),
    "sensors": {
        "weight": {"value": 50},
        "distance": {"value": 50},
        "cancel": {"value": 0},
        "confirm": {"value": 0}
    }
}

# Publish the message
# We use json.dumps() to turn the Python dictionary into a JSON string
while(True):
    client.publish(TOPIC, json.dumps(data1))
    client.publish(TOPIC, json.dumps(data2))
    print(f"Message published to {TOPIC}")
    time.sleep(1)
    
    
 
print(f"Message published to {TOPIC}")

# 4. Disconnect
client.disconnect()