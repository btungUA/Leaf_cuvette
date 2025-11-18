#!/usr/bin/env python3
"""
read_licor.py
Simple MQTT subscriber for LI-7810 to print/log incoming messages.

Place this in leaf_cuvette/backend/mqtt_test/read_licor.py
"""

import paho.mqtt.client as mqtt
import datetime
import json
import os
import argparse

# -------- CONFIGURATION --------
LICOR_IP = "192.168.10.1"   # ← your LI-7810 IP (already set)
LICOR_PORT = 1883
# Default topic used in LI manual. We'll subscribe to both the concentration topic
# and a wildcard to explore all messages.
TOPICS = [
    ("licor/niobrara/output/concentration", 1),
    ("licor/niobrara/output/#", 1)  # wildcard - optional, useful for discovery
]

LOG_FILE = "licor_stream.txt"   # created in this mqtt_test/ directory


# -------- CALLBACKS --------
def on_connect(client, userdata, flags, rc):
    now = datetime.datetime.now().isoformat()
    if rc == 0:
        print(f"[{now}] Connected to MQTT broker at {LICOR_IP}:{LICOR_PORT} (rc={rc})")
        for t, qos in TOPICS:
            client.subscribe(t, qos=qos)
            print(f"[{now}] Subscribed to: {t} (qos={qos})")
    else:
        print(f"[{now}] Failed to connect, return code {rc}")


def on_message(client, userdata, msg):
    ts = datetime.datetime.now().isoformat()
    try:
        payload = msg.payload.decode("utf-8")
    except Exception:
        payload = repr(msg.payload)

    # Try to pretty-print JSON if it is JSON
    pretty = payload
    try:
        parsed = json.loads(payload)
        pretty = json.dumps(parsed, indent=2)
    except Exception:
        pass

    line = f"[{ts}] {msg.topic} | {pretty}"
    print(line)
    # Append to file
    logfile_path = os.path.join(os.path.dirname(__file__), LOG_FILE)
    try:
        with open(logfile_path, "a") as f:
            f.write(line + "\n")
    except Exception as e:
        print(f"Failed to write to log file: {e}")


# -------- MAIN --------
def main(broker_ip, broker_port):
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    print(f"Connecting to LI-7810 MQTT broker at {broker_ip}:{broker_port} ...")
    try:
        client.connect(broker_ip, broker_port, keepalive=60)
    except Exception as e:
        print(f"Connection error: {e}")
        return

    # Blocking loop that handles reconnects automatically
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nInterrupted, exiting.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="LI-7810 MQTT test subscriber")
    parser.add_argument("--host", default=LICOR_IP, help="MQTT broker host (default set in file)")
    parser.add_argument("--port", default=LICOR_PORT, type=int, help="MQTT broker port")
    args = parser.parse_args()
    main(args.host, args.port)
