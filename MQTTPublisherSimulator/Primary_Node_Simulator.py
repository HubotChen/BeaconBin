import paho.mqtt.client as mqtt
import json
from datetime import datetime
import time
import random
import threading  # Added for multi-threading

# --- 1. Configuration ---
BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC = "eece5155/Hubert/data"
NUM_NODES = 100
SYNC_INTERVAL = 1 

MAX_WEIGHT = 4000  
MAX_DISTANCE = 300 
OBJECT_MAX_WEIGHT = 400
OBJECT_MIN_WEIGHT = 80
OBJECT_MAX_DISTANCE = 30
OBJECT_MIN_DISTANCE = 3

# --- 2. Shared State & Thread Safety ---
nodes = {f"node-{i:03d}": {"current_weight": 0, "current_distance": MAX_DISTANCE} for i in range(1, NUM_NODES + 1)}
data_lock = threading.Lock() # Prevents threads from accessing nodes at the same time

# --- 3. Publishing Thread Function ---
def publish_heartbeat():
    """Background thread that publishes all nodes every N seconds."""
    while True:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] Periodic Sync: Sending all {NUM_NODES} nodes...")
        
        with data_lock:
            # We copy the data or iterate carefully while locked
            current_snapshot = list(nodes.items())

        for node_id, data in current_snapshot:
            payload = {
                "device_id": node_id,
                "timestamp": datetime.now().isoformat(),
                "sensors": {
                    "weight": {"value": data["current_weight"], "unit": "g"},
                    "distance": {"value": data["current_distance"], "unit": "mm"},
                    "cancel": {"value": 0},
                    "confirm": {"value": 0}
                }
            }
            client.publish(TOPIC, json.dumps(payload))
            # Small delay between individual node publishes to avoid broker spam
            time.sleep(0.005) 
            
        time.sleep(SYNC_INTERVAL)

# --- 4. Setup MQTT Client ---
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="esp32-manual-test")
client.connect(BROKER, PORT)

# Start the background sync thread
sync_thread = threading.Thread(target=publish_heartbeat, daemon=True)
sync_thread.start()

# --- 5. Main Logic Loop (Runs in the main thread) ---
try:
    while True:
        mu, sigma = 50, 15
        chosen_index = max(1, min(NUM_NODES, int(random.gauss(mu, sigma))))
        node_id = f"node-{chosen_index:03d}"

        # Logic calculations
        obj_weight = random.randint(OBJECT_MIN_WEIGHT, OBJECT_MAX_WEIGHT)
        obj_height = random.randint(OBJECT_MIN_DISTANCE, OBJECT_MAX_DISTANCE)

        with data_lock: # Block the sync thread while we update values
            node = nodes[node_id]
            new_weight = node["current_weight"] + obj_weight
            new_distance = node["current_distance"] - obj_height

            if new_weight <= MAX_WEIGHT and new_distance >= 0:
                node["current_weight"] = new_weight
                node["current_distance"] = new_distance
                print(f"Object added to {node_id}. New Total: {node['current_weight']}g")
            else:
                print(f"Node {node_id} full.")

        # How often to add a new object
        time.sleep(.5) 

except KeyboardInterrupt:
    print("\nShutting down simulation...")
finally:
    client.disconnect()