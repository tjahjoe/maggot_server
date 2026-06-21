from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import Literal
from supabase_client import supabase

app = FastAPI(title="Maggot API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

DEVICE_ID = "device01"

# =========================================================
# SCHEMAS
# =========================================================

class SensorPayload(BaseModel):
    """Payload dari topicSensor (dikirim ESP32 via MQTT bridge)"""
    device_id: str = Field(default=DEVICE_ID)
    t1: float
    t2: float
    t3: float
    avg_temp: float
    set_point: float
    dim_delay_us: int

class StatusPayload(BaseModel):
    """Payload dari topicStatus (dikirim ESP32 via MQTT bridge)"""
    device_id: str = Field(default=DEVICE_ID)
    heater: Literal["on", "off"]
    fan: Literal["on", "off"]
    mode: Literal["auto", "manual"]

class LoginRequest(BaseModel):
    username: str
    password: str

# =========================================================
# ROOT
# =========================================================

@app.get("/")
def home():
    return {"status": "Maggot API running"}

# =========================================================
# MONITORING PAGE
# — Mobile GET history dulu saat buka halaman,
#   lalu lanjut subscribe MQTT untuk data realtime
# =========================================================

@app.get("/sensor/history")
def get_sensor_history(limit: int = 20, device_id: str = DEVICE_ID):
    """
    Dipakai mobile saat pertama buka halaman monitoring.
    Kembalikan data historis sensor untuk ditampilkan di chart.
    Setelah ini mobile lanjut subscribe MQTT topicSensor untuk realtime.
    """
    try:
        res = supabase.table("sensor_data") \
            .select("*") \
            .eq("device_id", device_id) \
            .order("recorded_at", desc=True) \
            .limit(limit) \
            .execute()

        data = list(reversed(res.data))

        formatted = {
            "t1": [], "t2": [], "t3": [],
            "avg_temp": [], "set_point": [], "dim_delay_us": []
        }

        for item in data:
            time = item["recorded_at"][11:19]
            formatted["t1"].append({"x": time, "y": item["t1"]})
            formatted["t2"].append({"x": time, "y": item["t2"]})
            formatted["t3"].append({"x": time, "y": item["t3"]})
            formatted["avg_temp"].append({"x": time, "y": item["avg_temp"]})
            formatted["set_point"].append({"x": time, "y": item["set_point"]})
            formatted["dim_delay_us"].append({"x": time, "y": item["dim_delay_us"]})

        return {"status": "success", "data": formatted}

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

# =========================================================
# CONTROL PAGE
# — Mobile GET status terakhir saat buka halaman control,
#   kirim perintah via MQTT publish dari mobile langsung,
#   ESP32 balik data ke topicStatus lalu POST ke /status/insert
# =========================================================

@app.get("/status/latest")
def get_latest_status(device_id: str = DEVICE_ID):
    """
    Dipakai mobile saat pertama buka halaman control.
    Kembalikan status heater & fan terakhir yang tersimpan.
    """
    try:
        sensor_res = supabase.table("sensor_data") \
            .select("*") \
            .eq("device_id", device_id) \
            .order("recorded_at", desc=True) \
            .limit(1) \
            .execute()

        status_res = supabase.table("device_status") \
            .select("*") \
            .eq("device_id", device_id) \
            .order("recorded_at", desc=True) \
            .limit(1) \
            .execute()

        return {
            "status": "success",
            "data": {
                "sensor": sensor_res.data[0] if sensor_res.data else None,
                "status": status_res.data[0] if status_res.data else None,
            }
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

# =========================================================
# INSERT — dipanggil oleh MQTT bridge, bukan mobile
# =========================================================

@app.post("/sensor/insert")
def insert_sensor(payload: SensorPayload):
    """
    Dipanggil MQTT bridge setiap ESP32 publish ke topicSensor.
    Simpan ke tabel sensor_data untuk history.
    """
    try:
        res = supabase.table("sensor_data").insert(payload.dict()).execute()
        return {"status": "success", "data": res.data}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/status/insert")
def insert_status(payload: StatusPayload):
    """
    Dipanggil MQTT bridge setiap ESP32 publish ke topicStatus.
    Simpan ke tabel device_status untuk history & audit.
    """
    try:
        res = supabase.table("device_status").insert(payload.dict()).execute()
        return {"status": "success", "data": res.data}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

# =========================================================
# AUTH
# =========================================================

@app.post("/login")
def login(user: LoginRequest):
    if user.username == "maggot" and user.password == "1234":
        return {
            "status": "success",
            "message": "Login berhasil",
            "data": {"username": user.username}
        }
    raise HTTPException(status_code=401, detail="Username atau password salah")
