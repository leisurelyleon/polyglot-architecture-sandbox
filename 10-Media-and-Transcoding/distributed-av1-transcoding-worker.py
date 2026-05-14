import asyncio
import subprocess
import json
from celery import Celery
from typing import Dict, Any

# Initialize the Distributed Task Queue
app = Celery('media_cluster', broker='redis://localhost:6379/0', backend='redis://localhost:6379/1')

class FFmpegTranscodeError(Exception):
    pass

async def _run_ffmpeg_async(command: list[str]) -> Dict[str, Any]:
    """Executes FFmpeg asynchronously, capturing stderr for progress tracking without blocking."""
    process = await asyncio.create_subprocess_exec(
        *command,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE
    )
    
    # FFmpeg writes progress to stderr
    stdout, stderr = await process.communicate()
    
    if process.returncode != 0:
        raise FFmpegTranscodeError(f"FFmpeg failed with code {process.returncode}: {stderr.decode()}")
        
    return {"status": "success", "output": stdout.decode()}

@app.task(bind=True, max_retries=3)
def transcode_hls_segment(self, input_chunk_path: str, output_chunk_path: str, target_resolution: str) -> str:
    """
    Celery task to transcode a single video segment into AV1 using highly optimized parameters.
    Designed to be run concurrently across hundreds of nodes.
    """
    # Map resolutions to optimal bitrates (in a real system, this is calculated dynamically via VMAF)
    bitrate_map = {
        "1080p": "4500k",
        "720p": "2500k",
        "480p": "1000k"
    }
    
    target_bitrate = bitrate_map.get(target_resolution, "2500k")

    # The immensely dense FFmpeg command for AV1 WebM streaming
    ffmpeg_cmd = [
        'ffmpeg', '-y', 
        '-i', input_chunk_path,
        '-c:v', 'libaom-av1',           # Use the AV1 codec
        '-b:v', target_bitrate,         # Target bitrate
        '-cpu-used', '4',               # Speed/Quality tradeoff (0-8)
        '-row-mt', '1',                 # Row-based multithreading
        '-tiles', '2x2',                # Split frame into tiles for parallel encoding
        '-g', '48',                     # Keyframe interval (Force I-frame every 48 frames)
        '-keyint_min', '48', 
        '-sc_threshold', '0',           # Disable scene cut detection (breaks HLS segments)
        '-c:a', 'libopus',              # Opus audio for WebM
        '-b:a', '128k',
        '-f', 'webm',
        '-dash', '1',                   # Format for MPEG-DASH/HLS streaming
        output_chunk_path
    ]

    # Run the async execution loop inside the synchronous Celery worker
    loop = asyncio.get_event_loop()
    try:
        result = loop.run_until_complete(_run_ffmpeg_async(ffmpeg_cmd))
        return json.dumps(result)
    except FFmpegTranscodeError as exc:
        # Exponential backoff for transient cluster storage failures
        raise self.retry(exc=exc, countdown=2 ** self.request.retries)
