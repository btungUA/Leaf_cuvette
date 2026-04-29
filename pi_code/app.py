import streamlit as st
from influxdb import InfluxDBClient
import pandas as pd
import datetime
import plotly.express as px
import paho.mqtt.publish as publish
import json

# --- CONFIGURATION ---
INFLUX_DB = "sensor_data"

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
    client = st.session_state.influx_client
    if not client: return pd.DataFrame()
        
    if start_time and end_time:
        start_utc = start_time + datetime.timedelta(hours=7)
        end_utc = end_time + datetime.timedelta(hours=7)
        start_str = start_utc.strftime('%Y-%m-%dT%H:%M:%SZ')
        end_str = end_utc.strftime('%Y-%m-%dT%H:%M:%SZ')
        time_clause = f"time >= '{start_str}' AND time <= '{end_str}'"
        limit_clause = "" 
    else:
        time_clause = "time > now() - 24h"
        limit_clause = f"LIMIT {limit}"

    q_licor = f'''
    SELECT mean("co2") as "co2", mean("ch4") as "ch4", mean("h2o") as "h2o", 
           mean("ring_down_time") as "ring_down_time", max("diag") as "diag", 
           last("checksum") as "checksum", last("remark") as "remark"
    FROM "li7810_sensors" WHERE {time_clause} GROUP BY time(5s) fill(none) ORDER BY time DESC {limit_clause}
    '''
    
    # ADDED: Fetch cuvette_id and scheduler settings
    q_esp = f'''
    SELECT mean("temp_leaf") as "temp_leaf", mean("temp_air") as "temp_air", 
           mean("humidity") as "humidity", mean("par_value") as "par_value", 
           last("mosfet_state") as "mosfet_state", last("cuvette_id") as "cuvette_id",
           last("mosfet_open_min") as "mosfet_open_min", last("mosfet_closed_min") as "mosfet_closed_min"
    FROM "leaf_sensors" WHERE {time_clause} GROUP BY time(5s) fill(none) ORDER BY time DESC {limit_clause}
    '''

    try:
        df_licor = pd.DataFrame()
        res_licor = list(client.query(q_licor).get_points())
        if res_licor: df_licor = pd.DataFrame(res_licor)

        df_esp = pd.DataFrame()
        res_esp = list(client.query(q_esp).get_points())
        if res_esp: df_esp = pd.DataFrame(res_esp)

        if df_licor.empty and df_esp.empty:
            return pd.DataFrame()
        elif df_licor.empty:
            df_final = df_esp
        elif df_esp.empty:
            df_final = df_licor
        else:
            df_final = pd.merge(df_licor, df_esp, on='time', how='outer')

        if not df_final.empty:
            df_final['time'] = pd.to_datetime(df_final['time'], utc=True)
            df_final['time'] = df_final['time'].dt.tz_convert('America/Phoenix').dt.tz_localize(None)
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
@st.fragment(run_every=2)
def render_live_stream():
    df = get_aggregated_data(limit=60) 

    col_co2, col_ch4, col_h2o = st.columns(3)
    if not df.empty and 'co2' in df.columns:
        with col_co2:
            st.markdown("###  CO2 (ppm)")
            fig = px.line(df, x='time', y='co2', template="plotly_dark", height=300)
            fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
            st.plotly_chart(fig, use_container_width=True)

        with col_ch4:
            st.markdown("###  CH4 (ppb) ")
            fig = px.line(df, x='time', y='ch4', template="plotly_dark", height=300)
            fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
            st.plotly_chart(fig, use_container_width=True)

        with col_h2o:
            st.markdown("###  H2O (ppm) ")
            fig = px.line(df, x='time', y='h2o', template="plotly_dark", height=300)
            fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
            st.plotly_chart(fig, use_container_width=True)
    else:
        st.info("Waiting for synchronized data to plot...")

    st.divider() 
    col_sensors, col_status = st.columns(2)
    latest = get_latest_leaf_data()

    with col_sensors:
        st.subheader(f"Environmental Sensor Readings (Cuvette {int(latest.get('cuvette_id', 1))})")
        st.metric("Air Temperature", f"{latest.get('temp_air', 0.0):.1f} °C")
        st.metric("Leaf Temperature", f"{latest.get('temp_leaf', 0.0):.1f} °C")
        st.metric("Humidity", f"{latest.get('humidity', 0.0):.1f} %")
        st.metric("PAR", f"{latest.get('par_value', 0.0):.2f} \u00B5mol/m\u00B2/s")
    
    with col_status:
        st.subheader("Cuvette Cycle Status")
        mosfet_state = latest.get("mosfet_state", 0)
        if mosfet_state == 1:
            st.success("✅ **FLUSHING:** Solenoids OPEN")
        else:
            st.warning("🛑 **SEALED:** Solenoids CLOSED")

def render_dashboard():
    c_title, c_btn = st.columns([4, 1])
    c_title.title("🌱 Chlorophellas Dashboard")
    c_btn.button("View Full Database", on_click=go_to_data_page)
    
    render_live_stream()
    
    # --- ADDED: THE MOSFET SCHEDULER UI ---
    st.divider()
    st.subheader("⏱️ Cuvette Control Scheduler")
    st.write("Update the Solenoid/MOSFET timing for Cuvette 1.")
    
    sc1, sc2, sc3 = st.columns([1, 1, 2])
    with sc1:
        open_min = st.number_input("Open Duration (Minutes)", min_value=1, value=13)
    with sc2:
        closed_min = st.number_input("Closed Duration (Minutes)", min_value=1, value=2)
    with sc3:
        st.write("") 
        st.write("") 
        if st.button("Update Schedule"):
            # Send the retained MQTT message to the ESP32
            payload = json.dumps({"open": open_min, "closed": closed_min})
            try:
                publish.single("sensors/leaf_1/control", payload, hostname="localhost", retain=True)
                st.success("Schedule sent successfully! ESP32 will update on its next cycle.")
            except Exception as e:
                st.error(f"Failed to send schedule: {e}")

# --- PAGE 2: DATA VIEW ---
def render_data_view():
    st.title("📂 Database Records")
    st.button("⬅️ Return to Home", on_click=go_to_home)
    
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
    
    if use_custom:
        df = get_aggregated_data(start_time=start_dt, end_time=end_dt)
        if not df.empty:
            st.success(f"Loaded records between {start_dt.strftime('%H:%M')} and {end_dt.strftime('%H:%M')}")
    else:
        df = get_aggregated_data(limit=1000)
    
    if not df.empty:
        # ADDED: Calculate Relative Time (Minutes elapsed since the oldest record in this view)
        min_time = df['time'].min()
        df.insert(1, 'Time Elapsed (Min)', ((df['time'] - min_time).dt.total_seconds() / 60).round(2))
        
        df = df.set_index('time')
        
        # ADDED: New variables placed cleanly into the final output
        ideal_order = [
            'Time Elapsed (Min)', 'cuvette_id', 'co2', 'ch4', 'h2o', 'temp_leaf', 'temp_air', 
            'humidity', 'par_value', 'mosfet_state', 'mosfet_open_min', 'mosfet_closed_min',
            'diag', 'ring_down_time', 'checksum', 'remark'
        ]
        
        current_cols = [col for col in ideal_order if col in df.columns]
        df = df[current_cols]
        
        df_display = df.rename(columns={'par_value': 'par'})
        
        st.dataframe(df_display, use_container_width=True)
        
        c1, c2 = st.columns(2)
        csv_data = df_display.reset_index().to_csv(index=False).encode('utf-8')
        json_data = df_display.reset_index().to_json(orient='records')
        
        c1.download_button("Download CSV", data=csv_data, file_name='leaf_data.csv', mime='text/csv')
        c2.download_button("Download JSON", data=json_data, file_name='leaf_data.json', mime='application/json')
    else:
        st.info("No data found in database. Check connections.")

# --- APP ROUTING ---
if st.session_state.page == 'dashboard':
    render_dashboard()
elif st.session_state.page == 'data_view':
    render_data_view()