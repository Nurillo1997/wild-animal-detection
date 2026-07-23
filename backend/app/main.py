from fastapi import Depends, FastAPI
from sqlalchemy.orm import Session

from app.database import Base, engine, get_db
from app.models import DetectionEvent
from app.schemas import (
    AnimalDetectionEvent,
    DetectionEventResponse,
)


Base.metadata.create_all(bind=engine)


app = FastAPI(
    title="Wild Animal Detection API",
    version="1.0.0",
)


@app.get("/")
def root():
    return {
        "message": "Wild Animal Detection API is running"
    }


@app.post(
    "/events",
    response_model=DetectionEventResponse,
    status_code=201,
)
def create_event(
    event: AnimalDetectionEvent,
    db: Session = Depends(get_db),
):
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
        .order_by(DetectionEvent.detected_at.desc())
        .all()
    )

    return events