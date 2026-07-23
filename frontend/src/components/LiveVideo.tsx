import {
  useEffect,
  useRef,
  useState,
} from "react";

import Hls from "hls.js";


const HLS_URL =
  "http://127.0.0.1:8000/static/hls/playlist.m3u8";


const PLAYLIST_RETRY_DELAY =
  300;


const MAX_PLAYLIST_RETRIES =
  30;


interface LiveVideoProps {
  streamSessionId: number;

  isStreamActive: boolean;

  onTimeUpdate?: (
    currentTime: number
  ) => void;

  onPlaybackStarted?: () => void;
}


function LiveVideo({
  streamSessionId,
  isStreamActive,
  onTimeUpdate,
  onPlaybackStarted,
}: LiveVideoProps) {
  const videoRef =
    useRef<HTMLVideoElement>(
      null
    );


  const hlsRef =
    useRef<Hls | null>(
      null
    );


  const retryTimeoutRef =
    useRef<
      ReturnType<typeof setTimeout>
      | null
    >(
      null
    );


  const onTimeUpdateRef =
    useRef(
      onTimeUpdate
    );


  const onPlaybackStartedRef =
    useRef(
      onPlaybackStarted
    );


  const [
    status,
    setStatus,
  ] = useState<
    "offline"
    | "waiting"
    | "loading"
    | "live"
  >(
    "offline"
  );


  /*
   * Keep callback refs updated.
   */
  useEffect(
    () => {
      onTimeUpdateRef.current =
        onTimeUpdate;
    },
    [
      onTimeUpdate,
    ]
  );


  useEffect(
    () => {
      onPlaybackStartedRef.current =
        onPlaybackStarted;
    },
    [
      onPlaybackStarted,
    ]
  );


  /*
   * Report video playback position
   * to App.tsx.
   */
  useEffect(
    () => {
      const video =
        videoRef.current;


      if (!video) {
        return;
      }


      const handleTimeUpdate =
        () => {
          onTimeUpdateRef
            .current
            ?.(
              video.currentTime
            );
        };


      video.addEventListener(
        "timeupdate",
        handleTimeUpdate
      );


      return () => {
        video.removeEventListener(
          "timeupdate",
          handleTimeUpdate
        );
      };
    },
    []
  );


  /*
   * Start new HLS session.
   *
   * This effect runs whenever
   * DeepStream sends stream_started.
   */
  useEffect(
    () => {
      const video =
        videoRef.current;


      if (!video) {
        return;
      }


      /*
       * Do not load old HLS files
       * while DeepStream is offline.
       */
      if (
        !isStreamActive ||
        streamSessionId === 0
      ) {
        setStatus(
          "offline"
        );

        return;
      }


      let cancelled =
        false;


      let retryCount =
        0;


      const destroyHls =
        () => {
          if (
            hlsRef.current
          ) {
            hlsRef
              .current
              .destroy();


            hlsRef.current =
              null;
          }
        };


      const clearRetry =
        () => {
          if (
            retryTimeoutRef.current
          ) {
            clearTimeout(
              retryTimeoutRef.current
            );


            retryTimeoutRef.current =
              null;
          }
        };


      const startPlayback =
        () => {
          if (
            cancelled
          ) {
            return;
          }


          destroyHls();


          setStatus(
            "loading"
          );


          video.pause();


          video.removeAttribute(
            "src"
          );


          video.load();


          /*
           * Chrome / Firefox / Edge.
           */
          if (
            Hls.isSupported()
          ) {
            const hls =
              new Hls({
                startPosition:
                  0,

                manifestLoadingTimeOut:
                  10000,
              });


            hlsRef.current =
              hls;


            const streamUrl =
              `${HLS_URL}` +
              `?session=` +
              `${streamSessionId}` +
              `&t=${Date.now()}`;


            hls.attachMedia(
              video
            );


            hls.on(
              Hls.Events.MEDIA_ATTACHED,
              () => {
                if (
                  cancelled
                ) {
                  return;
                }


                console.log(
                  "HLS media attached"
                );


                hls.loadSource(
                  streamUrl
                );
              }
            );


            hls.on(
              Hls.Events.MANIFEST_PARSED,
              () => {
                if (
                  cancelled
                ) {
                  return;
                }


                console.log(
                  "HLS manifest parsed"
                );


                try {
                  video.currentTime =
                    0;
                }
                catch {
                  // Ignore seek error.
                }


                video
                  .play()
                  .then(
                    () => {
                      if (
                        cancelled
                      ) {
                        return;
                      }


                      console.log(
                        "HLS video playing"
                      );


                      setStatus(
                        "live"
                      );


                      onPlaybackStartedRef
                        .current
                        ?.();
                    }
                  )
                  .catch(
                    (
                      error
                    ) => {
                      console.log(
                        "Autoplay failed:",
                        error
                      );


                      setStatus(
                        "loading"
                      );
                    }
                  );
              }
            );


            hls.on(
              Hls.Events.ERROR,
              (
                _event,
                data
              ) => {
                if (
                  cancelled ||
                  !data.fatal
                ) {
                  return;
                }


                console.log(
                  "Fatal HLS error:",
                  data.type,
                  data.details
                );


                switch (
                  data.type
                ) {
                  case Hls
                    .ErrorTypes
                    .NETWORK_ERROR:
                  {
                    destroyHls();


                    if (
                      retryCount <
                      MAX_PLAYLIST_RETRIES
                    ) {
                      retryCount++;


                      setStatus(
                        "waiting"
                      );


                      retryTimeoutRef.current =
                        setTimeout(
                          startPlayback,
                          PLAYLIST_RETRY_DELAY
                        );
                    }
                    else {
                      console.error(
                        "HLS playlist retry limit reached"
                      );


                      setStatus(
                        "offline"
                      );
                    }


                    break;
                  }


                  case Hls
                    .ErrorTypes
                    .MEDIA_ERROR:
                  {
                    hls
                      .recoverMediaError();


                    break;
                  }


                  default:
                  {
                    destroyHls();


                    setStatus(
                      "offline"
                    );


                    break;
                  }
                }
              }
            );


            return;
          }


          /*
           * Native HLS support.
           * Mainly Safari.
           */
          if (
            video.canPlayType(
              "application/vnd.apple.mpegurl"
            )
          ) {
            video.src =
              `${HLS_URL}` +
              `?session=` +
              `${streamSessionId}` +
              `&t=${Date.now()}`;


            video.load();


            video
              .play()
              .then(
                () => {
                  if (
                    cancelled
                  ) {
                    return;
                  }


                  setStatus(
                    "live"
                  );


                  onPlaybackStartedRef
                    .current
                    ?.();
                }
              )
              .catch(
                (
                  error
                ) => {
                  console.log(
                    "Native HLS autoplay failed:",
                    error
                  );
                }
              );
          }
        };


      console.log(
        "DeepStream session received:",
        streamSessionId
      );


      setStatus(
        "waiting"
      );


      startPlayback();


      return () => {
        cancelled =
          true;


        clearRetry();


        destroyHls();
      };
    },
    [
      streamSessionId,
      isStreamActive,
    ]
  );


  /*
   * Stop player when DeepStream
   * sends stream_stopped.
   */
  useEffect(
    () => {
      if (
        isStreamActive
      ) {
        return;
      }


      const video =
        videoRef.current;


      if (!video) {
        return;
      }


      if (
        hlsRef.current
      ) {
        hlsRef
          .current
          .destroy();


        hlsRef.current =
          null;
      }


      if (
        retryTimeoutRef.current
      ) {
        clearTimeout(
          retryTimeoutRef.current
        );


        retryTimeoutRef.current =
          null;
      }


      video.pause();


      video.removeAttribute(
        "src"
      );


      video.load();


      setStatus(
        "offline"
      );


      console.log(
        "DeepStream stopped. Video player offline."
      );
    },
    [
      isStreamActive,
    ]
  );


  const statusText =
    status === "waiting"
      ? "WAITING FOR STREAM..."
      : status === "loading"
        ? "STARTING LIVE VIDEO..."
        : "CAMERA OFFLINE";


  return (
    <div
      className="video-card"
    >
      <div
        className="video-container"
      >
        <video
          ref={
            videoRef
          }

          className="live-video"

          controls

          muted

          playsInline
        />


        <div
          className=
            "camera-badge camera-badge-left"
        >
          <span
            className={
              isStreamActive
                ? "status-dot status-dot-live"
                : "status-dot status-dot-offline"
            }
          />

          CAM 01
        </div>


        <div
          className=
            "camera-badge camera-badge-right"
        >
          <span
            className={
              isStreamActive
                ? "status-dot status-dot-live"
                : "status-dot status-dot-offline"
            }
          />

          CAM 02
        </div>


        {status !==
          "live" && (
          <div
            className=
              "video-offline-overlay"
          >
            <div
              className=
                "offline-icon"
            >
              ◉
            </div>

            <strong>
              {statusText}
            </strong>

            <span>
              {status ===
              "offline"
                ? "No active DeepStream session"
                : "Connecting to live stream"}
            </span>
          </div>
        )}
      </div>
    </div>
  );
}


export default LiveVideo;