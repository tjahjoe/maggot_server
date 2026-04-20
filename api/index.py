from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware 
from pydantic import BaseModel
from typing import Optional
from supabase_client import supabase

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"], 
    allow_headers=["*"], 
)

class MaggotLog(BaseModel):
    t1: float
    t2: float
    t3: float
    h1: float
    h2: float
    h3: float
    f1: float
    f2: float
    f3: float

class FanStatus(BaseModel):
    id: Optional[int] = 1  
    fan1: bool
    fan2: bool
    fan3: bool

class HeaterStatus(BaseModel):
    id: Optional[int] = 1  
    heater1: bool
    heater2: bool
    heater3: bool

class LoginRequest(BaseModel):
    username: str
    password: str

@app.get("/")
def home():
    return {"status": "API maggot_logs running"}

@app.get("/data")
def get_data():
    try:
        res = supabase.table("maggot_logs").select("*").order("created_at", desc=True).execute()
        return {"status": "success", "data": res.data}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    
@app.get("/data/latest")
def get_latest_data():
    try:
        res = supabase.table("maggot_logs").select("*").order("created_at", desc=True).limit(1).execute()
        
        if not res.data:
            return {"status": "success", "message": "No data found", "data": None}
        
        return {"status": "success", "data": res.data[0]}
        
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/fan")
def get_fan_status():
    try:
        res = supabase.table("fan_status").select("*").order("created_at", desc=True).limit(1).execute()
        
        if not res.data:
            return {"status": "success", "message": "No data found", "data": None}
        
        return {"status": "success", "data": res.data[0]}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    
@app.get("/heater")
def get_heater_status():
    try:
        res = supabase.table("heater_status").select("*").order("created_at", desc=True).limit(1).execute()
        
        if not res.data:
            return {"status": "success", "message": "No data found", "data": None}
        
        return {"status": "success", "data": res.data[0]}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/insert")
def insert_data(log: MaggotLog):
    try:
        data_to_insert = log.dict()
        
        res = supabase.table("maggot_logs").insert(data_to_insert).execute()
        
        return {"status": "data inserted", "data": res.data}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    
@app.post("/upsert/fan")
def upsert_fan_status(fan: FanStatus):
    try:
        data_to_upsert = fan.dict()

        res = supabase.table("fan_status").upsert(data_to_upsert).execute()
        
        return {"status": "success", "message": "Fan status updated/inserted", "data": res.data}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    
@app.post("/upsert/heater")
def upsert_heater_status(heater: HeaterStatus):
    try:
        data_to_upsert = heater.dict()

        res = supabase.table("heater_status").upsert(data_to_upsert).execute()
        
        return {"status": "success", "message": "Heater status updated/inserted", "data": res.data}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    

class LoginRequest(BaseModel):
    username: str
    password: str

@app.post("/login")
def login(user: LoginRequest):
    if user.username == "maggot" and user.password == "1234":
        return {
            "status": "success",
            "message": "Login berhasil",
            "data": {"username": user.username}
        }
    else:
        raise HTTPException(
            status_code=401, 
            detail="Username atau password salah"
        )