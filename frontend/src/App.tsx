import { useEffect, useState } from "react";
import type { DetectionEvent } from "./types";
import "./App.css";

const API_URL = "http://127.0.0.1:8000";

function App() {
  const [events, setEvents] = useState<DetectionEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    async function fetchEvents() {
      try {
        const response = await fetch(
          `${API_URL}/events/history`
        );

        if (!response.ok) {
          throw new Error(
            `HTTP error: ${response.status}`
          );
        }

        const data: DetectionEvent[] =
          await response.json();

        setEvents(data);
      } catch (err) {
        setError(
          err instanceof Error
            ? err.message
            : "Failed to load events"
        );
      } finally {
        setLoading(false);
      }
    }

    fetchEvents();
  }, []);

  useEffect(() => {
  const socket = new WebSocket(
    "ws://127.0.0.1:8000/ws"
  );

  socket.onopen = () => {
    console.log("WebSocket connected");
  };

  socket.onmessage = (event) => {
    const newEvent: DetectionEvent =
      JSON.parse(event.data);

    console.log(
      "New detection received:",
      newEvent
    );

    setEvents((previousEvents) => [
      newEvent,
      ...previousEvents,
    ]);
  };

  socket.onerror = (error) => {
    console.error(
      "WebSocket error:",
      error
    );
  };

  socket.onclose = () => {
    console.log("WebSocket disconnected");
  };

  return () => {
    socket.close();
  };
}, []);

  if (loading) {
    return <h2>Loading detection events...</h2>;
  }

  if (error) {
    return <h2>Error: {error}</h2>;
  }

  return (
    <main>
      <h1>Wild Animal Detection Dashboard</h1>

      <p>Total detections: {events.length}</p>

      <table>
        <thead>
          <tr>
            <th>ID</th>
            <th>Animal</th>
            <th>Confidence</th>
            <th>Source</th>
            <th>Tracker ID</th>
            <th>Frame</th>
            <th>Detected At</th>
          </tr>
        </thead>

        <tbody>
          {events.map((event) => (
            <tr key={event.id}>
              <td>{event.id}</td>

              <td>{event.animal}</td>

              <td>
                {(event.confidence * 100).toFixed(0)}%
              </td>

              <td>{event.source_id}</td>

              <td>{event.tracker_id}</td>

              <td>{event.frame_number}</td>

              <td>
                {new Date(
                  event.detected_at
                ).toLocaleString()}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </main>
  );
}

export default App;