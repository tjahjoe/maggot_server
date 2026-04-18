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
    p1: float
    p2: float
    p3: float
    fan1: bool
    fan2: bool
    fan3: bool

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

@app.post("/insert")
def insert_data(log: MaggotLog):
    try:
        data_to_insert = log.dict()
        
        res = supabase.table("maggot_logs").insert(data_to_insert).execute()
        
        return {"status": "data inserted", "data": res.data}
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