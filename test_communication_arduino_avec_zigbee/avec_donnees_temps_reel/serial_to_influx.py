import json
import time
import serial
from influxdb_client import InfluxDBClient, Point, WritePrecision

# ---------------- CONFIG ----------------
SERIAL_PORT = "COM5"
SERIAL_BAUD = 9600

INFLUX_URL = "http://localhost:8086"
INFLUX_TOKEN = "o19EwtNS4q_wVuYlnxRoonH1bUGgWWh4csLQHUBj_N9omx4tAZ3ACaRxIrAECFIfmjMYyrjWbyryMk5Vx0vEEw=="
INFLUX_ORG = "Palantir"
INFLUX_BUCKET = "capteurs"

MEASUREMENT = "mesures"
DEVICE_TAG = "xbee_noeud1"
# ----------------------------------------

def to_float(x):
    try:
        return float(x)
    except Exception:
        return None

def main():
    ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=1)
    time.sleep(2)  # laisse le temps au port de se stabiliser

    client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
    write_api = client.write_api()

    print("OK: lecture série -> InfluxDB")
    print(f"Port={SERIAL_PORT} Baud={SERIAL_BAUD} Bucket={INFLUX_BUCKET}")

    while True:
        line = ser.readline().decode(errors="ignore").strip()
        if not line:
            continue

        # On attend une ligne JSON pure
        try:
            data = json.loads(line)
        except json.JSONDecodeError:
            print("IGNORED (not JSON):", line)
            continue

        # Point Influx
        p = Point(MEASUREMENT).tag("device", DEVICE_TAG)

        # Ajoute uniquement les champs numériques
        for k, v in data.items():
            if isinstance(v, (int, float)):
                p = p.field(k, v)
            elif isinstance(v, str):
                fv = to_float(v)
                if fv is not None:
                    p = p.field(k, fv)

        # Ecriture
        write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=p)

        print("WROTE:", data)

if __name__ == "__main__":
    main()
