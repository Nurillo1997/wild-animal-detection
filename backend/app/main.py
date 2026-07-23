from fastapi import FastAPI

from app.schemas import AnimalDetectionEvent


app = FastAPI(
    title="Wild Animal Detection API",
    version="1.0.0",
)


@app.get("/")
def root():
    return {
        "message": "Wild Animal Detection API is running"
    }


@app.post("/events", status_code=201)
def create_event(event: AnimalDetectionEvent):
    print("\n=== EVENT RECEIVED FROM DEEPSTREAM ===")
    print(event.model_dump())
    print("======================================\n")

    return {
        "status": "success",
        "message": "Animal detection event received",
        "event": event,
    }