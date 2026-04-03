import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient
import json

# --- CONFIGURATION ---
INFLUX_DB = 'sensor_data'
MQTT_TOPIC = 'sensors/#' 

# 1. Connect to Database
db_client = InfluxDBClient(host='localhost', port=8086)
db_client.switch_database(INFLUX_DB)

def on_connect(client, userdata, flags, rc):
    print(f"Connected to MQTT Broker with result code {rc}")
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    print(f"Received message: {msg.payload.decode()}")
    try:
        payload = json.loads(msg.payload.decode())
        
        # Dynamically extract all numeric data from the JSON (temp, humidity, mosfet, etc.)
        fields = {}
        for key, value in payload.items():
            if isinstance(value, (int, float)) and key != "sensor":
                fields[key] = float(value)
        
        json_body = [
            {
                "measurement": "leaf_sensors", 
                "tags": {
                    "sensor": payload.get("sensor", "unknown"),
                    "location": "lab"
                },
                "fields": fields
            }
        ]
        
        db_client.write_points(json_body)
        print(" -> Saved to Database!")
        
    except Exception as e:
        print(f"Error processing message: {e}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("localhost", 1883, 60)
print("Bridge is running. Waiting for ESP32 data...")
client.loop_forever()