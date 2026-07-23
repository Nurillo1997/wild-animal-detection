from fastapi import (
    Depends,
    FastAPI,
    WebSocket,
    WebSocketDisconnect,
)

from sqlalchemy.orm import Session

from app.database import (
    Base,
    engine,
    get_db,
)

from app.models import DetectionEvent

from app.schemas import (
    AnimalDetectionEvent,
    DetectionEventResponse,
)

from app.websocket_manager import manager

from fastapi.middleware.cors import CORSMiddleware

from fastapi.staticfiles import StaticFiles




Base.metadata.create_all(
    bind=engine
)


app = FastAPI(
    title="Wild Animal Detection API",
    version="1.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:5173",
        "http://127.0.0.1:5173",
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.mount(
    "/static",
    StaticFiles(directory="static"),
    name="static",
)


@app.get("/")
def root():
    return {
        "message":
        "Wild Animal Detection API is running"
    }


@app.post(
    "/events",
    response_model=DetectionEventResponse,
    status_code=201,
)
async def create_event(
    event: AnimalDetectionEvent,
    db: Session = Depends(get_db),
):
    # Save event to database
    db_event = DetectionEvent(
        event_type=event.event_type,
        animal=event.animal,
        tracker_id=event.tracker_id,
        confidence=event.confidence,
        source_id=event.source_id,
        frame_number=event.frame_number,
        detected_at=event.detected_at,
    )

    db.add(db_event)
    db.commit()
    db.refresh(db_event)

    print(
        f"Event saved: "
        f"{db_event.animal} | "
        f"source={db_event.source_id} | "
        f"tracker={db_event.tracker_id}"
    )

    # Convert SQLAlchemy object
    # into Pydantic response model
    response_event = (
        DetectionEventResponse.model_validate(
            db_event
        )
    )

    # Broadcast new event to all
    # connected WebSocket clients
    await manager.broadcast(
        response_event.model_dump(
            mode="json"
        )
    )

    return db_event


@app.get(
    "/events/history",
    response_model=list[DetectionEventResponse],
)
def get_event_history(
    db: Session = Depends(get_db),
):
    events = (
        db.query(DetectionEvent)
        .order_by(
            DetectionEvent.detected_at.desc(),
            DetectionEvent.id.desc(),
        )
        .all()
    )

    return events


@app.websocket("/ws")
async def websocket_endpoint(
    websocket: WebSocket,
):
    await manager.connect(
        websocket
    )

    print(
        "WebSocket client connected"
    )

    try:
        while True:
            # Keep connection alive and
            # detect client disconnect.
            await websocket.receive_text()

    except WebSocketDisconnect:
        manager.disconnect(
            websocket
        )

        print(
            "WebSocket client disconnected"
        )