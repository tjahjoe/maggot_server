from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware # 1. Import Middleware
from pydantic import BaseModel
from typing import Optional
from supabase_client import supabase

app = FastAPI()

# 2. Konfigurasi CORS
# Kamu bisa mengganti ["*"] dengan ["http://localhost:8081"] untuk lebih spesifik
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Mengizinkan semua domain (termasuk localhost kamu)
    allow_credentials=True,
    allow_methods=["*"],  # Mengizinkan semua metode (GET, POST, dll)
    allow_headers=["*"],  # Mengizinkan semua headers
)

# Schema data menggunakan Pydantic
class MaggotLog(BaseModel):
    t1: float
    t2: float
    t3: float
    p1: float
    p2: float
    p3: float
    fan1: bool
    fan2: bool
    fan3: bool

@app.get("/")
def home():
    return {"status": "API maggot_logs running"}

# 1. URL UNTUK MENGAMBIL DATA (GET)
@app.get("/data")
def get_data():
    try:
        # Mengambil data terbaru berdasarkan created_at
        res = supabase.table("maggot_logs").select("*").order("created_at", desc=True).execute()
        return {"status": "success", "data": res.data}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

# 2. URL UNTUK MEMASUKKAN DATA (POST)
@app.post("/insert")
def insert_data(log: MaggotLog):
    try:
        # .dict() sudah deprecated di Pydantic v2, gunakan .model_dump() jika pakai v2
        # tapi .dict() masih berfungsi di banyak versi.
        data_to_insert = log.dict()
        
        res = supabase.table("maggot_logs").insert(data_to_insert).execute()
        
        return {"status": "data inserted", "data": res.data}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))