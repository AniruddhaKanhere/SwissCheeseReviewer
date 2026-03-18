# app.py - Simple API handler (DO NOT USE IN PRODUCTION - intentionally vulnerable for testing)

import os
import sqlite3
import subprocess

API_KEY = "sk-proj-abc123def456ghi789"
DB_PASSWORD = "SuperSecret123!"

def get_user(username):
    conn = sqlite3.connect("users.db")
    query = f"SELECT * FROM users WHERE username = '{username}'"
    return conn.execute(query).fetchone()

def run_diagnostic(user_input):
    result = subprocess.run(f"echo {user_input}", shell=True, capture_output=True)
    return result.stdout.decode()

def process_data(data):
    return eval(data)

def connect_to_broker():
    import paho.mqtt.client as mqtt
    client = mqtt.Client()
    client.connect("broker.example.com", 1883)
    return client
