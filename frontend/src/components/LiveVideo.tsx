import { useEffect, useRef } from "react";
import Hls from "hls.js";

const HLS_URL =
  "http://127.0.0.1:8000/static/hls/playlist.m3u8";

function LiveVideo() {
  const videoRef = useRef<HTMLVideoElement>(null);

  useEffect(() => {
    const video = videoRef.current;

    if (!video) {
      return;
    }

    let hls: Hls | null = null;

    if (Hls.isSupported()) {
      hls = new Hls();

      hls.loadSource(HLS_URL);
      hls.attachMedia(video);

      hls.on(Hls.Events.MANIFEST_PARSED, () => {
        video.play().catch(() => {
          console.log(
            "Autoplay blocked by browser"
          );
        });
      });
    } else if (
      video.canPlayType(
        "application/vnd.apple.mpegurl"
      )
    ) {
      video.src = HLS_URL;
    }

    return () => {
      if (hls) {
        hls.destroy();
      }
    };
  }, []);

  return (
    <section>
      <h2>Live Animal Monitoring</h2>

      <video
        ref={videoRef}
        controls
        autoPlay
        muted
        playsInline
        style={{
          width: "100%",
          maxWidth: "1280px",
        }}
      />
    </section>
  );
}

export default LiveVideo;