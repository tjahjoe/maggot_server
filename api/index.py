from fastapi import FastAPI
from supabase_client import supabase

app = FastAPI()

@app.get("/")
def root():
    return {"status": "API running"}

@app.get("/data")
def get_data():
    try:
        res = supabase.table("maggot_logs").select("*").execute()
        return {"data": res.data}
    except Exception as e:
        return {"error": str(e)}
