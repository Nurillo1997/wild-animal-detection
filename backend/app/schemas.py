from datetime import datetime

from pydantic import (
    BaseModel,
    ConfigDict,
    Field,
)


class AnimalDetectionEvent(
    BaseModel
):
    event_type: str

    animal: str

    tracker_id: int

    confidence: float = Field(
        ge=0.0,
        le=1.0,
    )

    source_id: int

    frame_number: int

    video_timestamp: float

    detected_at: datetime


class DetectionEventResponse(
    AnimalDetectionEvent
):
    id: int

    model_config = ConfigDict(
        from_attributes=True
    )


class StreamLifecycleEvent(
    BaseModel
):
    event_type: str