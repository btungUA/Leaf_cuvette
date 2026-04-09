import streamlit as st
from influxdb import InfluxDBClient
import pandas as pd
import time
import datetime # Required for the new scheduler
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
def get_aggregated_data(start_time=None, end_time=None, limit=1000):
    """
    Queries InfluxDB to bucket ALL data into 5-second intervals.
    Filters by specific timeframe if requested, and protects diagnostic integers.
    """
    client = st.session_state.influx_client
    if not client:
        return pd.DataFrame()
        
    # Set the timeframe boundaries
    if start_time and end_time:
        start_str = start_time.strftime('%Y-%m-%dT%H:%M:%SZ')
        end_str = end_time.strftime('%Y-%m-%dT%H:%M:%SZ')
        time_clause = f"time >= '{start_str}' AND time <= '{end_str}'"
        limit_clause = "" # Pull all data within range
    else:
        time_clause = "time > now() - 24h"
        limit_clause = f"LIMIT {limit}"

    # Explicitly typed query to protect max("diag") and last("remark")
    query = f'''
    SELECT 
        mean("co2") as "co2", 
        mean("ch4") as "ch4", 
        mean("h2o") as "h2o", 
        mean("temp_leaf") as "temp_leaf", 
        mean("temp_air") as "temp_air", 
        mean("humidity") as "humidity", 
        mean("spectral_total") as "spectral_total", 
        mean("ring_down_time") as "ring_down_time",
        max("diag") as "diag", 
        last("checksum") as "checksum",
        last("mosfet_state") as "mosfet_state",
        last("remark") as "remark"
    FROM "li7810_sensors", "leaf_sensors" 
    WHERE {time_clause} 
    GROUP BY time(5s) fill(none) 
    ORDER BY time DESC {limit_clause}
    '''

    try:
        result = client.query(query)
        data_list = list(result.items())
        
        df_final = pd.DataFrame()
        for (meta, res) in data_list:
            temp_df = pd.DataFrame(list(res))
            if not temp_df.empty:
                # No longer need to replace "mean_" because the SQL query renames the columns!
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
            st.markdown("### 💧 H2O (ppm)")
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
    st.title("📂 Database Records")
    st.button("⬅️ Return to Home", on_click=go_to_home)
    
    # --- TIMEFRAME UI ---
    st.markdown("### 📅 Select Download Range")
    col1, col2, col3, col4 = st.columns(4)
    
    now = datetime.datetime.now()
    one_hour_ago = now - datetime.timedelta(hours=1)
    
    with col1: start_date = st.date_input("Start Date", one_hour_ago.date())
    with col2: start_time = st.time_input("Start Time", one_hour_ago.time())
    with col3: end_date = st.date_input("End Date", now.date())
    with col4: end_time = st.time_input("End Time", now.time())
        
    start_dt = datetime.datetime.combine(start_date, start_time)
    end_dt = datetime.datetime.combine(end_date, end_time)
    
    use_custom = st.checkbox("Apply Date/Time Filter (Uncheck to view latest 1000 records)")
    st.divider()
    
    # Fetch based on toggle
    if use_custom:
        df = get_aggregated_data(start_time=start_dt, end_time=end_dt)
        if not df.empty:
            st.success(f"Loaded records between {start_dt.strftime('%H:%M')} and {end_dt.strftime('%H:%M')}")
    else:
        df = get_aggregated_data(limit=1000)
    
    if not df.empty:
        # Promote real time column to index
        df = df.set_index('time')
        
        # Included checksum and remark in the ideal order
        ideal_order = ['co2', 'ch4', 'h2o', 'temp_leaf', 'temp_air', 'humidity', 'spectral_total', 'diag', 'ring_down_time', 'checksum', 'mosfet_state', 'remark']
        
        current_cols = [col for col in ideal_order if col in df.columns]
        df = df[current_cols]
        
        st.dataframe(df, use_container_width=True)
        
        c1, c2 = st.columns(2)
        # Reset index for downloads to preserve the timestamp column
        csv_data = df.reset_index().to_csv(index=False).encode('utf-8')
        json_data = df.reset_index().to_json(orient='records')
        
        c1.download_button("Download CSV", data=csv_data, file_name='leaf_data.csv', mime='text/csv')
        c2.download_button("Download JSON", data=json_data, file_name='leaf_data.json', mime='application/json')
    else:
        st.info("No data found in database. Check connections.")

if st.session_state.page == 'dashboard':
    render_dashboard()
    time.sleep(REFRESH_RATE)
    st.rerun()
elif st.session_state.page == 'data_view':
    render_data_view()