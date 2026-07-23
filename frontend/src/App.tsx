import { useEffect, useState } from "react";
import type { DetectionEvent } from "./types";
import LiveVideo from "./components/LiveVideo";
import "./App.css";

const API_URL = "http://127.0.0.1:8000";
const WS_URL = "ws://127.0.0.1:8000/ws";

function App() {
  const [events, setEvents] = useState<DetectionEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  /*
   * Load detection history from FastAPI
   * when the application starts.
   */
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

        /*
         * Show newest events first.
         */
        setEvents([...data].reverse());
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

  /*
   * Connect to FastAPI WebSocket.
   *
   * New DeepStream detection events
   * are pushed to the frontend in
   * real time.
   */
  useEffect(() => {
    const socket = new WebSocket(
      WS_URL
    );

    socket.onopen = () => {
      console.log(
        "WebSocket connected"
      );
    };

    socket.onmessage = (message) => {
      try {
        const newEvent: DetectionEvent =
          JSON.parse(
            message.data
          );

        console.log(
          "New detection received:",
          newEvent
        );

        /*
         * Add newest detection
         * to the top of the table.
         */
        setEvents(
          (previousEvents) => [
            newEvent,
            ...previousEvents,
          ]
        );
      } catch (error) {
        console.error(
          "Failed to parse WebSocket message:",
          error
        );
      }
    };

    socket.onerror = (error) => {
      console.error(
        "WebSocket error:",
        error
      );
    };

    socket.onclose = () => {
      console.log(
        "WebSocket disconnected"
      );
    };

    /*
     * Close WebSocket when
     * component unmounts.
     */
    return () => {
      socket.close();
    };
  }, []);

  /*
   * Loading state.
   */
  if (loading) {
    return (
      <main>
        <h2>
          Loading detection events...
        </h2>
      </main>
    );
  }

  /*
   * API error state.
   */
  if (error) {
    return (
      <main>
        <h2>
          Error: {error}
        </h2>
      </main>
    );
  }

  return (
    <main>
      <h1>
        Wild Animal Detection Dashboard
      </h1>

      {/* ============================= */}
      {/* Live HLS Video                */}
      {/* ============================= */}

      <LiveVideo />

      {/* ============================= */}
      {/* Detection Statistics          */}
      {/* ============================= */}

      <section>
        <h2>
          Detection Events
        </h2>

        <p>
          Total detections:{" "}
          {events.length}
        </p>
      </section>

      {/* ============================= */}
      {/* Detection History Table       */}
      {/* ============================= */}

      <table>
        <thead>
          <tr>
            <th>ID</th>

            <th>
              Animal
            </th>

            <th>
              Confidence
            </th>

            <th>
              Source
            </th>

            <th>
              Tracker ID
            </th>

            <th>
              Frame
            </th>

            <th>
              Detected At
            </th>
          </tr>
        </thead>

        <tbody>
          {events.map(
            (event, index) => (
              <tr
                key={
                  event.id ??
                  `${event.source_id}-${event.tracker_id}-${event.frame_number}-${index}`
                }
              >
                <td>
                  {event.id ?? "-"}
                </td>

                <td>
                  {event.animal}
                </td>

                <td>
                  {(
                    event.confidence *
                    100
                  ).toFixed(0)}
                  %
                </td>

                <td>
                  {event.source_id}
                </td>

                <td>
                  {event.tracker_id}
                </td>

                <td>
                  {event.frame_number}
                </td>

                <td>
                  {new Date(
                    event.detected_at
                  ).toLocaleString()}
                </td>
              </tr>
            )
          )}
        </tbody>
      </table>
    </main>
  );
}

export default App;