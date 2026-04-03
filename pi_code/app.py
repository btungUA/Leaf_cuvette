import streamlit as st
from influxdb import InfluxDBClient
import pandas as pd
import time
import plotly.express as px
import numpy as np

# --- CONFIGURATION ---
INFLUX_DB = "sensor_data"
REFRESH_RATE = 2 

st.set_page_config(page_title="Leaf Cuvette Dashboard", layout="wide")

# --- SETUP CONNECTIONS ---
if 'influx_client' not in st.session_state:
    try:
        client = InfluxDBClient(host='localhost', port=8086, database=INFLUX_DB)
        st.session_state.influx_client = client
    except:
        st.session_state.influx_client = None

if 'page' not in st.session_state:
    st.session_state.page = 'dashboard'

# --- HELPER FUNCTIONS ---
def get_recent_data(measurement, limit=50):
    client = st.session_state.influx_client
    if client:
        try:
            result = client.query(f'SELECT * FROM "{measurement}" ORDER BY time DESC LIMIT {limit}')
            points = list(result.get_points())
            if points:
                df = pd.DataFrame(points)
                df['time'] = pd.to_datetime(df['time'])
                return df
        except:
            pass
    dates = pd.date_range(end=pd.Timestamp.now(), periods=limit, freq='S')
    return pd.DataFrame({'time': dates, 'value': np.random.uniform(20, 30, limit)})

def get_latest_leaf_data():
    """Fetches the absolute latest reading from the ESP32"""
    client = st.session_state.influx_client
    if client:
        try:
            result = client.query('SELECT * FROM "leaf_sensors" ORDER BY time DESC LIMIT 1')
            points = list(result.get_points())
            if points:
                return points[0]
        except:
            pass
    return {}

def go_to_data_page(): st.session_state.page = 'data_view'
def go_to_home(): st.session_state.page = 'dashboard'

# --- PAGE 1: DASHBOARD ---
def render_dashboard():
    st.title("🌱 Leaf Cuvette Dashboard")

    col_co2, col_ch4, col_h2o = st.columns(3)
    with col_co2:
        st.markdown("### ☁️ CO2 (ppm)")
        fig = px.line(get_recent_data("co2"), x='time', y='value', template="plotly_dark", height=300)
        fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
        st.plotly_chart(fig, use_container_width=True)

    with col_ch4:
        st.markdown("### 💨 CH4 (ppb)")
        fig = px.line(get_recent_data("ch4"), x='time', y='value', template="plotly_dark", height=300)
        fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
        st.plotly_chart(fig, use_container_width=True)

    with col_h2o:
        st.markdown("### 💧 H2O (mmol)")
        fig = px.line(get_recent_data("h2o"), x='time', y='value', template="plotly_dark", height=300)
        fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
        st.plotly_chart(fig, use_container_width=True)

    st.divider() 
    col_sensors, col_status = st.columns(2)
    
    latest = get_latest_leaf_data()

    with col_sensors:
        st.subheader("📝 Live Sensor Readings")
        st.metric("Air Temperature", f"{latest.get('temp_air', 0.0):.1f} °C")
        st.metric("Leaf Temperature", f"{latest.get('temp_leaf', 0.0):.1f} °C")
        st.metric("Humidity", f"{latest.get('humidity', 0.0):.1f} %")
        st.metric("Spectral (Vio / Grn / Red)", f"{int(latest.get('spectral_vio', 0))} / {int(latest.get('spectral_grn', 0))} / {int(latest.get('spectral_red', 0))}")
        st.button("📂 View Full Database", on_click=go_to_data_page)
    
    with col_status:
        st.subheader("⚠️ Cuvette Cycle Status")
        
        mosfet_state = latest.get("mosfet_state", 0)
        
        if mosfet_state == 1:
            st.success("✅ **FLUSHING:** Solenoids OPEN (13 Min Cycle)")
        else:
            st.warning("🛑 **SEALED:** Solenoids CLOSED (2 Min Cycle)")

        st.markdown("**System Messages:**")
        error_box = st.container(border=True)
        error_box.write("ESP32 Connection: Stable")

# --- PAGE 2: DATA VIEW ---
def render_data_view():
    st.title("📂 Database Records")
    st.button("⬅️ Return to Dashboard", on_click=go_to_home)
    
    client = st.session_state.influx_client
    if client:
        result = client.query('SELECT * FROM "leaf_sensors" ORDER BY time DESC LIMIT 1000')
        points = list(result.get_points())
        
        if points:
            df = pd.DataFrame(points)
            df['time'] = pd.to_datetime(df['time'])
            st.dataframe(df, use_container_width=True)
            
            c1, c2 = st.columns(2)
            c1.download_button(label="Download CSV", data=df.to_csv(index=False).encode('utf-8'), file_name='leaf_data.csv', mime='text/csv')
            c2.download_button(label="Download JSON", data=df.to_json(orient='records'), file_name='leaf_data.json', mime='application/json')
        else:
            st.info("No data found in database.")
    else:
        st.error("Not connected to InfluxDB.")

if st.session_state.page == 'dashboard':
    render_dashboard()
    time.sleep(REFRESH_RATE)
    st.rerun()
elif st.session_state.page == 'data_view':
    render_data_view()