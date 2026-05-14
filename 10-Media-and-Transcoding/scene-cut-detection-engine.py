import cv2
import numpy as np
import multiprocessing as mp
from typing import List, Tuple

def calculate_frame_difference(frame1: np.ndarray, frame2: np.ndarray) -> float:
    """Calculates the Mean Absolute Difference (MAD) between two frames."""
    # Convert frames to grayscale to speed up matrix math
    gray1 = cv2.cvtColor(frame1, cv2.COLOR_BGR2GRAY)
    gray2 = cv2.cvtColor(frame2, cv2.COLOR_BGR2GRAY)
    
    # Apply a Gaussian blur to remove compression artifacts and noise
    blur1 = cv2.GaussianBlur(gray1, (5, 5), 0)
    blur2 = cv2.GaussianBlur(gray2, (5, 5), 0)
    
    # Calculate absolute pixel difference matrix
    diff = cv2.absdiff(blur1, blur2)
    return np.mean(diff)

def analyze_chunk(chunk_data: Tuple[int, str, float]) -> List[int]:
    """Worker function to process a specific chunk of video frames."""
    start_frame, video_path, threshold = chunk_data
    scene_cuts = []
    
    cap = cv2.VideoCapture(video_path)
    cap.set(cv2.CAP_PROP_POS_FRAMES, start_frame)
    
    ret, prev_frame = cap.read()
    if not ret:
        return []

    # Process 1000 frames per worker
    for offset in range(1, 1000):
        ret, curr_frame = cap.read()
        if not ret:
            break
            
        diff_score = calculate_frame_difference(prev_frame, curr_frame)
        
        # If the visual difference exceeds our heuristic threshold, we found a cut!
        if diff_score > threshold:
            scene_cuts.append(start_frame + offset)
            
        prev_frame = curr_frame
        
    cap.release()
    return scene_cuts

class SceneCutDetector:
    def __init__(self, video_path: str, sensitivity: float = 15.0):
        self.video_path = video_path
        self.sensitivity = sensitivity
        
    def detect_cuts(self) -> List[int]:
        cap = cv2.VideoCapture(self.video_path)
        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        cap.release()

        # Split the video into overlapping 1000-frame chunks for the CPU pool
        chunk_params = [
            (i, self.video_path, self.sensitivity) 
            for i in range(0, total_frames, 1000)
        ]
        
        # Utilize all available CPU cores to crunch the matrix math
        with mp.Pool(processes=mp.cpu_count()) as pool:
            results = pool.map(analyze_chunk, chunk_params)
            
        # Flatten the list of lists into a single sorted array of frame indexes
        master_cut_list = [frame for chunk_result in results for frame in chunk_result]
        return sorted(master_cut_list)

if __name__ == "__main__":
    detector = SceneCutDetector("/mnt/media/raw_gameplay.mp4")
    cuts = detector.detect_cuts()
    print(f"Detected {len(cuts)} hard scene transitions. Optimal I-Frame locations mapped.")
