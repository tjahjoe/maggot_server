from fastapi import FastAPI
from mangum import Mangum
from supabase_client import supabase

app = FastAPI()

@app.get("/")
def home():
    return {"status": "API running"}

@app.get("/data")
def get_data():
    res = supabase.table("maggot_logs").select("*").execute()
    return res.data

handler = Mangum(app)
