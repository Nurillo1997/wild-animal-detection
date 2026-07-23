from fastapi import (
    Depends,
    FastAPI,
    WebSocket,
    WebSocketDisconnect,
)

from fastapi.middleware.cors import (
    CORSMiddleware,
)

from fastapi.staticfiles import (
    StaticFiles,
)

from sqlalchemy.orm import Session

from app.database import (
    Base,
    engine,
    get_db,
)

from app.models import (
    DetectionEvent,
)

from app.schemas import (
    AnimalDetectionEvent,
    DetectionEventResponse,
    StreamLifecycleEvent,
)

from app.websocket_manager import (
    manager,
)


# ============================================================
# Database
# ============================================================

Base.metadata.create_all(
    bind=engine
)


# ============================================================
# FastAPI Application
# ============================================================

app = FastAPI(
    title="Wild Animal Detection API",
    version="1.0.0",
)


# ============================================================
# CORS
# ============================================================

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


# ============================================================
# Static HLS Files
# ============================================================

app.mount(
    "/static",
    StaticFiles(
        directory="static"
    ),
    name="static",
)


# ============================================================
# Root
# ============================================================

@app.get("/")
def root():
    return {
        "message":
        "Wild Animal Detection API is running"
    }


# ============================================================
# Stream Lifecycle
# ============================================================

@app.post("/stream/start")
async def stream_start():

    print(
        "DeepStream stream started"
    )

    message = {
        "event_type":
        "stream_started"
    }

    await manager.broadcast(
        message
    )

    return {
        "status": "ok",
        "event_type":
        "stream_started",
    }


@app.post("/stream/stop")
async def stream_stop():

    print(
        "DeepStream stream stopped"
    )

    message = {
        "event_type":
        "stream_stopped"
    }

    await manager.broadcast(
        message
    )

    return {
        "status": "ok",
        "event_type":
        "stream_stopped",
    }


# ============================================================
# Detection Events
# ============================================================

@app.post(
    "/events",
    response_model=
        DetectionEventResponse,
    status_code=201,
)
async def create_event(
    event:
        AnimalDetectionEvent,
    db:
        Session =
        Depends(
            get_db
        ),
):

    # --------------------------------------------------------
    # Save detection event
    # --------------------------------------------------------

    db_event = DetectionEvent(
        event_type=
            event.event_type,

        animal=
            event.animal,

        tracker_id=
            event.tracker_id,

        confidence=
            event.confidence,

        source_id=
            event.source_id,

        frame_number=
            event.frame_number,

        video_timestamp=
            event.video_timestamp,

        detected_at=
            event.detected_at,
    )


    db.add(
        db_event
    )

    db.commit()

    db.refresh(
        db_event
    )


    print(
        f"Event saved: "
        f"{db_event.animal} | "
        f"source="
        f"{db_event.source_id} | "
        f"tracker="
        f"{db_event.tracker_id} | "
        f"video_timestamp="
        f"{db_event.video_timestamp:.3f}s"
    )


    # --------------------------------------------------------
    # Convert SQLAlchemy model
    # into Pydantic response
    # --------------------------------------------------------

    response_event = (
        DetectionEventResponse
        .model_validate(
            db_event
        )
    )


    # --------------------------------------------------------
    # Broadcast detection event
    # --------------------------------------------------------

    await manager.broadcast(

        response_event.model_dump(
            mode="json"
        )

    )


    return db_event


# ============================================================
# Detection History
# ============================================================

@app.get(
    "/events/history",
    response_model=
        list[
            DetectionEventResponse
        ],
)
def get_event_history(
    db:
        Session =
        Depends(
            get_db
        ),
):

    events = (

        db.query(
            DetectionEvent
        )

        .order_by(

            DetectionEvent
            .detected_at
            .desc(),

            DetectionEvent
            .id
            .desc(),

        )

        .all()

    )


    return events


# ============================================================
# WebSocket
# ============================================================

@app.websocket("/ws")
async def websocket_endpoint(
    websocket:
        WebSocket,
):

    await manager.connect(
        websocket
    )


    print(
        "WebSocket client connected"
    )


    try:

        while True:

            await websocket.receive_text()


    except WebSocketDisconnect:

        manager.disconnect(
            websocket
        )


        print(
            "WebSocket client disconnected"
        )