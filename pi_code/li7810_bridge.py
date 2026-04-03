import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient
import json

# --- CONFIGURATION ---
LI7810_IP = "192.168.1.50"
LI7810_TOPIC = "licor/niobrara/output/concentration"
INFLUX_DB = "sensor_data"

db_client = InfluxDBClient(host='localhost', port=8086)
db_client.switch_database(INFLUX_DB)

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to LI-7810 Broker at {LI7810_IP}!")
        client.subscribe(LI7810_TOPIC)
    else:
        print(f"Failed to connect. Return code: {rc}")

def on_message(client, userdata, msg):
    try:
        raw_payload = json.loads(msg.payload.decode())
        
        # Dig into the "Concentration" envelope
        data = raw_payload.get("Concentration", {})
        
        if not data:
            return 

        h2o = data.get("H2O", 0.0)
        co2 = data.get("CO2", 0.0)
        ch4 = data.get("CH4", 0.0)

        json_body = [
            {
                "measurement": "li7810_sensors", 
                "tags": {
                    "sensor": "li7810",
                    "location": "lab"
                },
                "fields": {
                    "h2o": float(h2o),
                    "co2": float(co2),
                    "ch4": float(ch4)
                }
            }
        ]
        
        db_client.write_points(json_body)
        print(f" -> Saved LI-7810 Data: H2O: {h2o:.1f}, CO2: {co2:.1f}, CH4: {ch4:.1f}")
        
    except Exception as e:
        print(f"Error processing LI-7810 message: {e}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

print(f"Attempting to connect to LI-7810 at {LI7810_IP}...")
client.connect(LI7810_IP, 1883, 60) 
client.loop_forever()