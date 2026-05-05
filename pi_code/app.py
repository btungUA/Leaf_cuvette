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
    # Fetch a deep history so the graph and the database share the EXACT same "Time Zero"
    df = get_aggregated_data(limit=10000) 

    if not df.empty:
        # 1. Find the true start time of the experiment
        min_time = df['time'].min()
        
        # 2. Calculate the correct elapsed time for every point
        df['Elapsed Minutes'] = (df['time'] - min_time).dt.total_seconds() / 60.0
        df = df.sort_values('time', ascending=True)
        
        # 3. Slice to show only the last 8 minutes (approx 100 points at 5s intervals) on the live graph
        df = df.tail(100)

    col_co2, col_ch4, col_h2o = st.columns(3)
    if not df.empty and 'co2' in df.columns:
        with col_co2:
            st.markdown("###  CO2 (ppm)")
            fig = px.line(df, x='Elapsed Minutes', y='co2', template="plotly_dark", height=300)
            fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
            st.plotly_chart(fig, use_container_width=True)

        with col_ch4:
            st.markdown("###  CH4 (ppb) ")
            fig = px.line(df, x='Elapsed Minutes', y='ch4', template="plotly_dark", height=300)
            fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
            st.plotly_chart(fig, use_container_width=True)

        with col_h2o:
            st.markdown("###  H2O (ppm) ")
            fig = px.line(df, x='Elapsed Minutes', y='h2o', template="plotly_dark", height=300)
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
    
    st.divider()
    st.subheader("⏱️ Cuvette Control Scheduler")
    st.write("Update the Solenoid/MOSFET timing for Cuvette 1.")
    
    sc1, sc2, sc3 = st.columns([1, 1, 2])
    with sc1:
        # Changed to text input for MM:SS
        open_val = st.text_input("Open Duration (MM:SS)", value="13:00")
    with sc2:
        closed_val = st.text_input("Closed Duration (MM:SS)", value="02:00")
    with sc3:
        st.write("") 
        st.write("") 
        if st.button("Update Schedule"):
            try:
                # Parse the MM:SS strings into numeric minutes
                o_m, o_s = map(int, open_val.split(":"))
                c_m, c_s = map(int, closed_val.split(":"))
                
                open_min = o_m + (o_s / 60.0)
                closed_min = c_m + (c_s / 60.0)

                payload = json.dumps({"open": open_min, "closed": closed_min})
                publish.single("sensors/leaf_1/control", payload, hostname="localhost", retain=True)
                st.success("Schedule sent successfully! ESP32 will update on its next cycle.")
            except ValueError:
                st.error("Please use MM:SS format (e.g., 13:00)")
            except Exception as e:
                st.error(f"Failed to send schedule: {e}")

# --- PAGE 2: DATA VIEW ---
# --- PAGE 2: DATA VIEW ---
def render_data_view():
    st.title("📂 Database Records")
    st.button("⬅️ Return to Home", on_click=go_to_home)
    
    st.markdown("### ⏳ Select Download Range by Elapsed Time")
    
    use_custom = st.checkbox("Apply Elapsed Time Filter (Uncheck to view all records)")
    
    col1, col2 = st.columns(2)
    with col1:
        start_val = st.text_input("Start Elapsed Time (MM:SS)", value="00:00")
    with col2:
        end_val = st.text_input("End Elapsed Time (MM:SS)", value="60:00")
        
    st.divider()
    
    # Fetch a massive block of data to guarantee a solid "Time Zero"
    df = get_aggregated_data(limit=10000) 
    
    if not df.empty:
        try:
            min_time = df['time'].min()
            df['total_sec'] = (df['time'] - min_time).dt.total_seconds()
            
            if use_custom:
                s_m, s_s = map(int, start_val.split(":"))
                e_m, e_s = map(int, end_val.split(":"))
                start_sec = s_m * 60 + s_s
                end_sec = e_m * 60 + e_s
                
                # Apply the filter only if the checkbox is checked
                df = df[(df['total_sec'] >= start_sec) & (df['total_sec'] <= end_sec)]
                
                if df.empty:
                    st.warning("No data found in that specific time range.")
                    return
                st.success(f"Filtered records between {start_val} and {end_val}.")
            else:
                st.success("Showing all available records.")
            
            # Format back to clean MM:SS strings for the table view
            df['Elapsed Time'] = df['total_sec'].apply(lambda x: f"{int(x // 60):02d}:{int(x % 60):02d}")
            df = df.drop(columns=['time', 'total_sec'])
            
            # Map 1 and 0 to Open and Closed
            if 'mosfet_state' in df.columns:
                df['mosfet_state'] = df['mosfet_state'].apply(
                    lambda x: 'Open' if pd.notna(x) and int(x) == 1 else ('Closed' if pd.notna(x) else x)
                )
            
            ideal_order = [
                'Elapsed Time', 'cuvette_id', 'co2', 'ch4', 'h2o', 'temp_leaf', 'temp_air', 
                'humidity', 'par_value', 'mosfet_state', 'mosfet_open_min', 'mosfet_closed_min',
                'diag', 'ring_down_time', 'checksum', 'remark'
            ]
            
            current_cols = [col for col in ideal_order if col in df.columns]
            df = df[current_cols]
            df_display = df.rename(columns={'par_value': 'par'})
            
            st.dataframe(df_display, use_container_width=True, hide_index=True)
            
            c1, c2 = st.columns(2)
            c1.download_button("Download CSV", data=df_display.to_csv(index=False).encode('utf-8'), file_name='leaf_data.csv', mime='text/csv')
            c2.download_button("Download JSON", data=df_display.to_json(orient='records'), file_name='leaf_data.json', mime='application/json')
            
        except ValueError:
            st.error("Please use valid MM:SS format (e.g., 20:00)")
    else:
        st.info("No data found in database. Check connections.")

# --- APP ROUTING ---
if st.session_state.page == 'dashboard':
    render_dashboard()
elif st.session_state.page == 'data_view':
    render_data_view()