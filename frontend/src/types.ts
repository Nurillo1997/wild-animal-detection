export interface DetectionEvent {
  id: number;

  event_type: string;

  animal: string;

  tracker_id: number;

  confidence: number;

  source_id: number;

  frame_number: number;

  video_timestamp: number;

  detected_at: string;
}