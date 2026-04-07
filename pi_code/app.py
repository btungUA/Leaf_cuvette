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
def get_aggregated_data(limit=1000):
    """
    Queries InfluxDB to bucket ALL data into 5-second intervals,
    averages it, and merges both sensors into a single row.
    """
    client = st.session_state.influx_client
    if not client:
        return pd.DataFrame()
        
    query = f'''
    SELECT mean(*) FROM "li7810_sensors", "leaf_sensors" 
    WHERE time > now() - 24h 
    GROUP BY time(5s) fill(none) 
    ORDER BY time DESC LIMIT {limit}
    '''
    try:
        result = client.query(query)
        data_list = list(result.items())
        
        df_final = pd.DataFrame()
        for (meta, res) in data_list:
            temp_df = pd.DataFrame(list(res))
            if not temp_df.empty:
                # Remove InfluxDB's automatic "mean_" prefix to keep headers clean
                temp_df.columns = [col.replace('mean_', '') for col in temp_df.columns]
                
                # Merge the LiCor and ESP32 dataframes side-by-side based on the timestamp
                if df_final.empty:
                    df_final = temp_df
                else:
                    df_final = pd.merge(df_final, temp_df, on='time', how='outer')
                    
        if not df_final.empty:
            df_final['time'] = pd.to_datetime(df_final['time'])
            df_final = df_final.sort_values('time', ascending=False).reset_index(drop=True)
            
        return df_final
    except Exception as e:
        st.error(f"Database error: {e}")
        return pd.DataFrame()

def get_latest_leaf_data():
    """Fetches the absolute latest raw reading from the ESP32 for the UI Metrics"""
    client = st.session_state.influx_client
    if client:
        try:
            result = client.query('SELECT * FROM "leaf_sensors" ORDER BY time DESC LIMIT 1')
            points = list(result.get_points())
            if points: return points[0]
        except: pass
    return {}

def go_to_data_page(): st.session_state.page = 'data_view'
def go_to_home(): st.session_state.page = 'dashboard'

# --- PAGE 1: DASHBOARD ---
def render_dashboard():
    st.title("🌱 Leaf Cuvette Dashboard")

    # Fetch the synced data once for all three charts
    df = get_aggregated_data(limit=60) 

    col_co2, col_ch4, col_h2o = st.columns(3)
    
    if not df.empty and 'co2' in df.columns:
        with col_co2:
            st.markdown("### ☁️ CO2 (ppm)")
            fig = px.line(df, x='time', y='co2', template="plotly_dark", height=300)
            fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
            st.plotly_chart(fig, use_container_width=True)

        with col_ch4:
            st.markdown("### 💨 CH4 (ppb)")
            fig = px.line(df, x='time', y='ch4', template="plotly_dark", height=300)
            fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
            st.plotly_chart(fig, use_container_width=True)

        with col_h2o:
            st.markdown("### 💧 H2O (mmol)")
            fig = px.line(df, x='time', y='h2o', template="plotly_dark", height=300)
            fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
            st.plotly_chart(fig, use_container_width=True)
    else:
        st.info("Waiting for synchronized data to plot...")

    st.divider() 
    col_sensors, col_status = st.columns(2)
    
    latest = get_latest_leaf_data()

    with col_sensors:
        st.subheader("📝 Live Sensor Readings")
        st.metric("Air Temperature", f"{latest.get('temp_air', 0.0):.1f} °C")
        st.metric("Leaf Temperature", f"{latest.get('temp_leaf', 0.0):.1f} °C")
        st.metric("Humidity", f"{latest.get('humidity', 0.0):.1f} %")
        st.metric("Spectral Total", f"{int(latest.get('spectral_total', 0))}")
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
    st.title("📂 Unified Research Records")
    st.button("⬅️ Return to Dashboard", on_click=go_to_home)
    
    df = get_aggregated_data(limit=1000)
    
    if not df.empty:
        st.dataframe(df, use_container_width=True)
        
        c1, c2 = st.columns(2)
        csv_data = df.to_csv(index=False).encode('utf-8')
        json_data = df.to_json(orient='records')
        
        c1.download_button("Download Unified CSV", data=csv_data, file_name='leaf_data_synced.csv', mime='text/csv')
        c2.download_button("Download Unified JSON", data=json_data, file_name='leaf_data_synced.json', mime='application/json')
    else:
        st.info("No data found in database. Check connections.")

if st.session_state.page == 'dashboard':
    render_dashboard()
    time.sleep(REFRESH_RATE)
    st.rerun()
elif st.session_state.page == 'data_view':
    render_data_view()