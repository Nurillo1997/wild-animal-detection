from datetime import datetime

from sqlalchemy import (
    DateTime,
    Float,
    Integer,
    String,
)

from sqlalchemy.orm import (
    Mapped,
    mapped_column,
)

from app.database import Base


class DetectionEvent(Base):
    __tablename__ = "detection_events"

    id: Mapped[int] = mapped_column(
        Integer,
        primary_key=True,
        index=True,
    )

    event_type: Mapped[str] = mapped_column(
        String,
        nullable=False,
    )

    animal: Mapped[str] = mapped_column(
        String,
        nullable=False,
        index=True,
    )

    tracker_id: Mapped[int] = mapped_column(
        Integer,
        nullable=False,
    )

    confidence: Mapped[float] = mapped_column(
        Float,
        nullable=False,
    )

    source_id: Mapped[int] = mapped_column(
        Integer,
        nullable=False,
        index=True,
    )

    frame_number: Mapped[int] = mapped_column(
        Integer,
        nullable=False,
    )

    # Video presentation timestamp
    # received from DeepStream.
    # Stored in seconds.
    video_timestamp: Mapped[float] = mapped_column(
        Float,
        nullable=False,
    )

    detected_at: Mapped[datetime] = mapped_column(
        DateTime,
        nullable=False,
    )