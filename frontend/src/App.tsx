import {
  useCallback,
  useEffect,
  useRef,
  useState,
} from "react";

import type {
  DetectionEvent,
} from "./types";

import LiveVideo from
  "./components/LiveVideo";

import "./App.css";


const API_URL =
  "http://127.0.0.1:8000";


const WS_URL =
  "ws://127.0.0.1:8000/ws";


const SYNC_TOLERANCE =
  0.15;


const ITEMS_PER_PAGE =
  5;


function App() {
  const [
    events,
    setEvents,
  ] = useState<
    DetectionEvent[]
  >(
    []
  );


  const pendingEventsRef =
    useRef<
      DetectionEvent[]
    >(
      []
    );


  const videoCurrentTimeRef =
    useRef(
      0
    );


  const [
    isStreamActive,
    setIsStreamActive,
  ] = useState(
    false
  );


  const [
    streamSessionId,
    setStreamSessionId,
  ] = useState(
    0
  );


  const [
    loading,
    setLoading,
  ] = useState(
    true
  );


  const [
    error,
    setError,
  ] = useState<
    string | null
  >(
    null
  );


  const [
    currentPage,
    setCurrentPage,
  ] = useState(
    1
  );


  const [
    latestDetection,
    setLatestDetection,
  ] = useState<
    DetectionEvent | null
  >(
    null
  );


  const [
    currentTime,
    setCurrentTime,
  ] = useState(
    new Date()
  );


  /*
   * Dashboard clock.
   */
  useEffect(
    () => {
      const interval =
        setInterval(
          () => {
            setCurrentTime(
              new Date()
            );
          },
          1000
        );


      return () =>
        clearInterval(
          interval
        );
    },
    []
  );


  /*
   * Load historical detections.
   */
  useEffect(
    () => {
      async function
      fetchEvents() {
        try {
          const response =
            await fetch(
              `${API_URL}/events/history`
            );


          if (
            !response.ok
          ) {
            throw new Error(
              `HTTP error: ${response.status}`
            );
          }


          const data:
            DetectionEvent[] =
            await response.json();


          setEvents(
            data
          );
        }
        catch (
          err
        ) {
          setError(
            err instanceof Error
              ? err.message
              : "Failed to load events"
          );
        }
        finally {
          setLoading(
            false
          );
        }
      }


      fetchEvents();
    },
    []
  );


  /*
   * WebSocket.
   */
  useEffect(
    () => {
      let socket:
        WebSocket | null =
        null;


      let reconnectTimer:
        ReturnType<
          typeof setTimeout
        >
        | null =
        null;


      let manuallyClosed =
        false;


      const connect =
        () => {
          socket =
            new WebSocket(
              WS_URL
            );


          socket.onopen =
            () => {
              console.log(
                "WebSocket connected"
              );
            };


          socket.onmessage =
            (
              message
            ) => {
              try {
                const data =
                  JSON.parse(
                    message.data
                  );


                /*
                 * DeepStream started.
                 */
                if (
                  data.event_type ===
                  "stream_started"
                ) {
                  console.log(
                    "DeepStream stream_started received"
                  );


                  pendingEventsRef.current =
                    [];


                  videoCurrentTimeRef.current =
                    0;


                  setLatestDetection(
                    null
                  );


                  setIsStreamActive(
                    true
                  );


                  setStreamSessionId(
                    (
                      previous
                    ) =>
                      previous +
                      1
                  );


                  return;
                }


                /*
                 * DeepStream stopped.
                 */
                if (
                  data.event_type ===
                  "stream_stopped"
                ) {
                  console.log(
                    "DeepStream stream_stopped received"
                  );


                  setIsStreamActive(
                    false
                  );


                  pendingEventsRef.current =
                    [];


                  return;
                }


                /*
                 * Animal detection.
                 */
                if (
                  data.event_type ===
                  "animal_detected"
                ) {
                  const newEvent =
                    data as
                      DetectionEvent;


                  console.log(
                    "Detection received and queued:",
                    newEvent.animal,
                    `timestamp=${newEvent.video_timestamp}s`
                  );


                  /*
                   * Add immediately to
                   * Recent Detections.
                   */
                  setEvents(
                    (
                      previousEvents
                    ) => [
                      newEvent,
                      ...previousEvents,
                    ]
                  );


                  /*
                   * Queue warning until
                   * video reaches detection
                   * timestamp.
                   */
                  pendingEventsRef
                    .current
                    .push(
                      newEvent
                    );


                  pendingEventsRef
                    .current
                    .sort(
                      (
                        a,
                        b
                      ) =>
                        a.video_timestamp -
                        b.video_timestamp
                    );


                  setCurrentPage(
                    1
                  );


                  return;
                }
              }
              catch (
                err
              ) {
                console.error(
                  "Invalid WebSocket message:",
                  err
                );
              }
            };


          socket.onerror =
            (
              websocketError
            ) => {
              console.error(
                "WebSocket error:",
                websocketError
              );
            };


          socket.onclose =
            () => {
              console.log(
                "WebSocket disconnected"
              );


              if (
                manuallyClosed
              ) {
                return;
              }


              reconnectTimer =
                setTimeout(
                  connect,
                  1000
                );
            };
        };


      connect();


      return () => {
        manuallyClosed =
          true;


        if (
          reconnectTimer
        ) {
          clearTimeout(
            reconnectTimer
          );
        }


        if (
          socket &&
          (
            socket.readyState ===
              WebSocket.OPEN ||
            socket.readyState ===
              WebSocket.CONNECTING
          )
        ) {
          socket.close();
        }
      };
    },
    []
  );


  /*
   * Synchronize detections with
   * video playback timestamp.
   */
  const handleVideoTimeUpdate =
    useCallback(
      (
        videoTime:
          number
      ) => {
        videoCurrentTimeRef.current =
          videoTime;


        const pending =
          pendingEventsRef.current;


        if (
          pending.length ===
          0
        ) {
          return;
        }


        const readyEvents =
          pending.filter(
            (
              event
            ) =>
              event.video_timestamp <=
              videoTime +
                SYNC_TOLERANCE
          );


        if (
          readyEvents.length ===
          0
        ) {
          return;
        }


        pendingEventsRef.current =
          pending.filter(
            (
              event
            ) =>
              event.video_timestamp >
              videoTime +
                SYNC_TOLERANCE
          );


        console.log(
          "Displaying synchronized events:",
          readyEvents
        );


        const newestReadyEvent =
          readyEvents[
            readyEvents.length -
            1
          ];


        setLatestDetection(
          newestReadyEvent
        );
      },
      []
    );


  const handlePlaybackStarted =
    useCallback(
      () => {
        console.log(
          "New video playback session started"
        );


        videoCurrentTimeRef.current =
          0;
      },
      []
    );


  /*
   * Pagination.
   */
  const totalPages =
    Math.max(
      1,
      Math.ceil(
        events.length /
          ITEMS_PER_PAGE
      )
    );


  const startIndex =
    (
      currentPage -
      1
    ) *
    ITEMS_PER_PAGE;


  const visibleEvents =
    events.slice(
      startIndex,
      startIndex +
        ITEMS_PER_PAGE
    );


  const showingStart =
    events.length === 0
      ? 0
      : startIndex + 1;


  const showingEnd =
    Math.min(
      startIndex +
        ITEMS_PER_PAGE,
      events.length
    );


  /*
   * Page buttons around current page.
   */
  const getPageNumbers =
    () => {
      const pages:
        number[] =
        [];


      const start =
        Math.max(
          1,
          currentPage -
            1
        );


      const end =
        Math.min(
          totalPages,
          start +
            2
        );


      for (
        let page =
          start;
        page <=
        end;
        page++
      ) {
        pages.push(
          page
        );
      }


      return pages;
    };


  if (
    loading
  ) {
    return (
      <div
        className=
          "app-loading"
      >
        Loading detection system...
      </div>
    );
  }


  if (
    error
  ) {
    return (
      <div
        className=
          "app-loading"
      >
        Error: {error}
      </div>
    );
  }


  return (
    <div
      className="app-shell"
    >
      <div
        className="dashboard"
      >

        {/* =========================
            HEADER
        ========================= */}

        <header
          className=
            "dashboard-header"
        >
          <div
            className=
              "brand"
          >
            <div
              className=
                "brand-icon"
            >
              🐾
            </div>


            <div>
              <h1>
                Wild Animal
                Detection Dashboard
              </h1>

              <p>
                Real-time wildlife
                monitoring system
              </p>
            </div>
          </div>


          <div
            className=
              "header-actions"
          >
            <div
              className=
                "system-status"
            >
              <span
                className={
                  isStreamActive
                    ? "status-dot status-dot-live"
                    : "status-dot status-dot-offline"
                }
              />


              <span>
                Cameras
              </span>


              <strong
                className={
                  isStreamActive
                    ? "status-live-text"
                    : "status-offline-text"
                }
              >
                {isStreamActive
                  ? "LIVE"
                  : "OFFLINE"}
              </strong>
            </div>


            <div
              className=
                "dashboard-clock"
            >
              <div
                className=
                  "clock-icon"
              >
                ◷
              </div>


              <div>
                <strong>
                  {currentTime
                    .toLocaleTimeString(
                      [],
                      {
                        hour:
                          "2-digit",

                        minute:
                          "2-digit",

                        second:
                          "2-digit",
                      }
                    )}
                </strong>


                <span>
                  {currentTime
                    .toLocaleDateString(
                      [],
                      {
                        month:
                          "short",

                        day:
                          "numeric",

                        year:
                          "numeric",
                      }
                    )}
                </span>
              </div>
            </div>
          </div>
        </header>


        {/* =========================
            LIVE VIDEO
        ========================= */}

        <section
          className=
            "video-section"
        >
          <LiveVideo
            streamSessionId={
              streamSessionId
            }

            isStreamActive={
              isStreamActive
            }

            onTimeUpdate={
              handleVideoTimeUpdate
            }

            onPlaybackStarted={
              handlePlaybackStarted
            }
          />
        </section>


        {/* =========================
            DETECTION ALERT
        ========================= */}

        {latestDetection ? (
          <section
            className=
              "detection-alert"
          >
            <div
              className=
                "alert-title"
            >
              <div
                className=
                  "alert-icon"
              >
                !
              </div>

              <strong>
                ANIMAL DETECTED
              </strong>
            </div>


            <div
              className=
                "alert-animal"
            >
              🐾{" "}

              {latestDetection
                .animal
                .toUpperCase()}
            </div>


            <div
              className=
                "alert-confidence"
            >
              Confidence:

              <strong>
                {" "}
                {(
                  latestDetection
                    .confidence *
                  100
                ).toFixed(
                  0
                )}
                %
              </strong>
            </div>


            <div
              className=
                "alert-camera"
            >
              🎥{" "}

              CAM{" "}

              {String(
                latestDetection
                  .source_id +
                1
              ).padStart(
                2,
                "0"
              )}
            </div>
          </section>
        ) : (
          <section
            className=
              "detection-alert detection-alert-idle"
          >
            <div
              className=
                "alert-title"
            >
              <div
                className=
                  "alert-icon alert-icon-idle"
              >
                ✓
              </div>

              <strong>
                MONITORING
              </strong>
            </div>


            <div
              className=
                "alert-idle-message"
            >
              Waiting for animal
              detection events
            </div>
          </section>
        )}


        {/* =========================
            RECENT DETECTIONS
        ========================= */}

        <section
          className=
            "detections-card"
        >
          <div
            className=
              "detections-header"
          >
            <div
              className=
                "detections-title-group"
            >
              <div
                className=
                  "list-icon"
              >
                ☷
              </div>


              <div>
                <h2>
                  Recent Detections
                </h2>

                <p>
                  Total detections:

                  <strong>
                    {" "}
                    {events.length}
                  </strong>
                </p>
              </div>
            </div>


            <button
              className=
                "filter-button"
            >
              ◇

              <span>
                Filter
              </span>

              <span>
               ⌄
              </span>
            </button>
          </div>


          <div
            className=
              "table-wrapper"
          >
            <table
              className=
                "detections-table"
            >
              <thead>
                <tr>
                  <th>
                    ID
                  </th>

                  <th>
                    Animal
                  </th>

                  <th>
                    Confidence
                  </th>

                  <th>
                    Camera
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
                {visibleEvents.map(
                  (
                    event
                  ) => {
                    const confidence =
                      event.confidence *
                      100;


                    return (
                      <tr
                        key={
                          event.id
                        }
                      >
                        <td>
                          {event.id}
                        </td>


                        <td>
                          <div
                            className=
                              "animal-cell"
                          >
                            <span
                              className=
                                "animal-icon"
                            >
                              🐾
                            </span>


                            <span>
                              {event.animal}
                            </span>
                          </div>
                        </td>


                        <td>
                          <strong
                            className={
                              confidence >=
                              85
                                ? "confidence-high"
                                : confidence >=
                                    70
                                  ? "confidence-medium"
                                  : "confidence-low"
                            }
                          >
                            {confidence
                              .toFixed(
                                0
                              )}
                            %
                          </strong>
                        </td>


                        <td>
                          <span
                            className={
                              event.source_id ===
                              0
                                ? "camera-table-badge camera-one"
                                : "camera-table-badge camera-two"
                            }
                          >
                            CAM{" "}

                            {String(
                              event.source_id +
                              1
                            ).padStart(
                              2,
                              "0"
                            )}
                          </span>
                        </td>


                        <td>
                          {event
                            .tracker_id}
                        </td>


                        <td>
                          {event
                            .frame_number}
                        </td>


                        <td>
                          {new Date(
                            event.detected_at
                          ).toLocaleString()}
                        </td>
                      </tr>
                    );
                  }
                )}
              </tbody>
            </table>
          </div>


          {/* =====================
              PAGINATION
          ===================== */}

          <div
            className=
              "pagination-container"
          >
            <div
              className=
                "pagination-info"
            >
              Showing{" "}

              {showingStart}

              {" "}to{" "}

              {showingEnd}

              {" "}of{" "}

              {events.length}

              {" "}results
            </div>


            <div
              className=
                "pagination"
            >
              <button
                onClick={
                  () =>
                    setCurrentPage(
                      (
                        page
                      ) =>
                        Math.max(
                          1,
                          page -
                            1
                        )
                    )
                }

                disabled={
                  currentPage ===
                  1
                }
              >
                Previous
              </button>


              {getPageNumbers()
                .map(
                  (
                    page
                  ) => (
                    <button
                      key={
                        page
                      }

                      className={
                        page ===
                        currentPage
                          ? "page-active"
                          : ""
                      }

                      onClick={
                        () =>
                          setCurrentPage(
                            page
                          )
                      }
                    >
                      {page}
                    </button>
                  )
                )}


              {totalPages >
                4 && (
                <span
                  className=
                    "pagination-dots"
                >
                  ...
                </span>
              )}


              {totalPages >
                3 &&
                currentPage <
                  totalPages -
                    1 && (
                <button
                  onClick={
                    () =>
                      setCurrentPage(
                        totalPages
                      )
                  }
                >
                  {totalPages}
                </button>
              )}


              <button
                onClick={
                  () =>
                    setCurrentPage(
                      (
                        page
                      ) =>
                        Math.min(
                          totalPages,
                          page +
                            1
                        )
                    )
                }

                disabled={
                  currentPage ===
                  totalPages
                }
              >
                Next
              </button>
            </div>
          </div>
        </section>
      </div>
    </div>
  );
}


export default App;