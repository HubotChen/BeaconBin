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
nodes = {f"node-{i:03d}": {"current_weight": 0, "current_distance": MAX_DISTANCE, "confirm": 0, "cancel": 0} for i in range(1, NUM_NODES + 1)}
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
                    "cancel": {"value": data["cancel"]},
                    "confirm": {"value": data["confirm"]}
                }
            }
            client.publish(TOPIC, json.dumps(payload))

            data["confirm"] = 0
            data["cancel"] = 0
            # Small delay between individual node publishes to avoid broker spam
            time.sleep(0.005) 
            
        time.sleep(SYNC_INTERVAL)


def simulate_item_interaction(node_id, weight_change=0, distance_change=0):
    """
    Simulates a hand reaching into the bin (noise), settling, 
    and then sending a confirmation pulse.
    """
    with data_lock:
        original_weight = nodes[node_id]["current_weight"]
        original_distance = nodes[node_id]["current_distance"]
    
    new_weight = min(MAX_WEIGHT, (max(0, original_weight + weight_change)))
    new_distance = min(MAX_DISTANCE, max(0, original_distance - distance_change))

    # introducing noise
    with data_lock:
        nodes[node_id]["current_weight"] = min(MAX_WEIGHT, original_weight + int(random.uniform(50, 2000)))
        nodes[node_id]["current_distance"] = max(0, original_distance - int(random.uniform(5, 150)))
    
    # Wait for the noise to settle (less than 2 seconds)
    time.sleep(0.5)
    
    with data_lock:
        nodes[node_id]["current_weight"] = new_weight
        nodes[node_id]["current_distance"] = new_distance
        nodes[node_id]["confirm"] = 1

    print("added an object")

    # if new_weight <= MAX_WEIGHT and new_distance >= 0:
    #     # introducing noise
    #     with data_lock:
    #         nodes[node_id]["current_weight"] = min(MAX_WEIGHT, original_weight + int(random.uniform(50, 2000)))
    #         nodes[node_id]["current_distance"] = max(0, original_distance - int(random.uniform(5, 150)))
        
    #     # Wait for the noise to settle (less than 2 seconds)
    #     time.sleep(0.5)
        
    #     with data_lock:
    #         nodes[node_id]["current_weight"] = new_weight
    #         nodes[node_id]["current_distance"] = new_distance
    #         nodes[node_id]["confirm"] = 1

    #     print("added an object")

    # else:
    #     print(f"Node {node_id} full.")


# --- 4. Setup MQTT Client ---
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="esp32-manual-test")
client.connect(BROKER, PORT)

# Start the background sync thread
sync_thread = threading.Thread(target=publish_heartbeat, daemon=True)
sync_thread.start()

# --- 5. Main Logic Loop (Runs in the main thread) ---
try:
    while True:
        # 1. Select the node using Gaussian distribution
        mu, sigma = 50, 15
        chosen_index = max(1, min(NUM_NODES, int(random.gauss(mu, sigma))))
        node_id = f"node-{chosen_index:03d}"

        # 2. Define Event Probabilities (Total 100%)
        # 40% Add Item, 30% Remove Item, 30% No Change (Cancel)
        event_roll = random.random()

        if event_roll < 0.60:
            # SCENARIO: Adding an item
            obj_weight = random.randint(OBJECT_MIN_WEIGHT, OBJECT_MAX_WEIGHT)
            obj_height = random.randint(OBJECT_MIN_DISTANCE, OBJECT_MAX_DISTANCE)
            print(f"[EVENT] Add Item to {node_id}")
            simulate_item_interaction(node_id, obj_weight, obj_height)

        elif event_roll < 0.80:
            # SCENARIO: Removing an item
            # Use negative values to decrease weight and increase distance
            obj_weight = -random.randint(OBJECT_MIN_WEIGHT, OBJECT_MAX_WEIGHT)
            obj_height = -random.randint(OBJECT_MIN_DISTANCE, OBJECT_MAX_DISTANCE)
            print(f"[EVENT] Remove Item from {node_id}")
            simulate_item_interaction(node_id, obj_weight, obj_height)

        else:
            # SCENARIO: User interacted but added/removed nothing (Cancel)
            print(f"[EVENT] Interaction Only (Cancel) on {node_id}")
            simulate_item_interaction(node_id, 0, 0)

        # 3. Frequency of interaction events
        time.sleep(0.5)

except KeyboardInterrupt:
    print("\nShutting down simulation...")
finally:
    client.disconnect()