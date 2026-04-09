import sqlite3
import os
from flask import Flask, render_template, jsonify, request

app = Flask(__name__)
DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'lora_data.db')

def get_db_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    try:
        conn = get_db_connection()
        conn.execute('''CREATE TABLE IF NOT EXISTS node_metadata (
            node_id INTEGER PRIMARY KEY,
            location TEXT
        )''')
        conn.commit()
        conn.close()
    except Exception as e:
        print(f"DB Init Warning: {e}")

init_db()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/data')
def api_data():
    if not os.path.exists(DB_PATH):
        return jsonify({"data": [], "nodes": []})
        
    conn = get_db_connection()
    # Get latest 50 records
    records = conn.execute('SELECT * FROM telemetry ORDER BY timestamp DESC LIMIT 50').fetchall()
    
    # Get distinct nodes and their latest seen time along with metadata location
    nodes = conn.execute('''
        SELECT t.from_node, MAX(t.timestamp) as last_seen, COUNT(t.id) as packets, m.location 
        FROM telemetry t
        LEFT JOIN node_metadata m ON t.from_node = m.node_id
        GROUP BY t.from_node
    ''').fetchall()
    conn.close()
    
    return jsonify({
        "data": [dict(ix) for ix in records],
        "nodes": [dict(n) for n in nodes]
    })

@app.route('/api/node/<int:node_id>/location', methods=['POST'])
def update_location(node_id):
    data = request.get_json()
    if not data or 'location' not in data:
        return jsonify({"status": "error", "message": "Missing location string"}), 400
        
    location = data.get('location', '')
    conn = get_db_connection()
    conn.execute('''
        INSERT INTO node_metadata (node_id, location)
        VALUES (?, ?)
        ON CONFLICT(node_id) DO UPDATE SET location=excluded.location
    ''', (node_id, location))
    conn.commit()
    conn.close()
    return jsonify({"status": "success"})

if __name__ == '__main__':
    # Running on 0.0.0.0 makes it accessible to the local network (laptop on ethernet)
    app.run(host='0.0.0.0', port=5000, debug=True)
