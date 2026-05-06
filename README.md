# Leaf Cuvette Software Operating Instructions

## First Time Connection:
1. Power on the raspberry pi. This can be verified by a red light on the board and the fan spinning up.
2. Wait ~1 minute to allow the pi to boot up
3. On your local machine connect to the "Leaf-Link" wifi network. Password: "cuvettemaster"

## Once Connected:
1. Open terminal and ssh in: "ssh pi@10.42.0.1" password: "plzletmein"
2. Navigate to bridge.py: "cd leaf_cuvette/bridge/"
3. Run bridge.py: "python3 bridge.py"
4. Open a new terminal, ssh in, open sandbox: "source leaf_env/bin/activate"
5. Navigate into bridge folder again and run li7810_bridge.py: "python3 li7810_bridge.py"
6. Open a new terminal, ssh in, open sandbox, navigate to app.py folder: "cd leaf_cuvette/app/"
7. Run app.py: "streamlit run app.py"
8. While connected to the Leaf-Link wifi enter "10.42.0.1:8501/" into your browser
9. Enjoy!

## Shut Down Procedure (A little overkill but ensures data is safe):
1. Stop every terminal by pressing CTRL+C
2. Deactivate each sandbox by typing: "deactivate"
3. In any terminal that is still ssh'd in type "sudo shutdown -h now"
4. Do not unplug Raspberry Pi until green light stops flickering

## Extra Notes:
- This software is meant to collect data from an Arduino ESP32-S3 remotely and a Li_7810 gas analyzer via ethernet. Any differences in setup will require software adjustment.
- The Arduino code is meant to be used with an AS7343 spectral sensor, SHT31 Temp. & Humidity sensor, 10k epoxy thermistor, and a FQP13N10L FQP N-Channel Mosfet. Any differences in components will also require software adjustment.
- If not using the original Raspberry Pi used in the project setup will require configuring the pi to host it's own wifi. Adustments will have to be made so that the li_7810 and esp32 can successfully connect.
