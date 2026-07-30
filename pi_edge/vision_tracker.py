import cv2
import dlib
from scipy.spatial import distance as dist
import threading
import numpy as np

# Thread-safe global variables
vision_lock = threading.Lock()
shared_is_drowsy = False
shared_ear = 0.0

EAR_THRESHOLD = 0.25
CONSECUTIVE_FRAMES = 20

# Yawning constants
MAR_THRESHOLD = 0.6
YAWN_CONSECUTIVE_FRAMES = 30

def eye_aspect_ratio(eye):
    A = dist.euclidean(eye[1], eye[5])
    B = dist.euclidean(eye[2], eye[4])
    C = dist.euclidean(eye[0], eye[3])
    return (A + B) / (2.0 * C)

def mouth_aspect_ratio(mouth):
    # vertical distances
    A = dist.euclidean(mouth[13], mouth[19]) # 51, 59
    B = dist.euclidean(mouth[14], mouth[18]) # 52, 58
    C = dist.euclidean(mouth[15], mouth[17]) # 53, 57
    # horizontal distance
    D = dist.euclidean(mouth[12], mouth[16]) # 48, 54
    if D == 0: return 0
    return (A + B + C) / (3.0 * D)

def get_head_tilt(shape):
    # Basic 2D head tilt estimation using nose tip (30) and chin (8) vs eyes
    nose = (shape.part(30).x, shape.part(30).y)
    chin = (shape.part(8).x, shape.part(8).y)
    # If the x coordinate of the nose is significantly skewed relative to chin, the head is tilted/turned.
    tilt_deviation = abs(nose[0] - chin[0])
    return tilt_deviation > 40  # arbitrary threshold for heavy tilt

def get_vision_data():
    """Accessor for the main hub thread to read the latest CV state safely."""
    with vision_lock:
        return shared_ear, shared_is_drowsy

def start_monitoring():
    global shared_is_drowsy, shared_ear
    drowsy_counter = 0
    yawn_counter = 0
    
    print("[INFO] Loading facial landmark predictor...")
    detector = dlib.get_frontal_face_detector()
    try:
        predictor = dlib.shape_predictor("models/shape_predictor_68_face_landmarks.dat")
    except Exception as e:
        print("[WARNING] Dlib model not found in models/ folder. Simulation mode active.")
        import time
        while True:
            with vision_lock:
                shared_ear = 0.30
                shared_is_drowsy = False
            time.sleep(0.1)

    print("[INFO] Starting video stream...")
    vs = cv2.VideoCapture(0)

    while True:
        ret, frame = vs.read()
        if not ret:
            break

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        faces = detector(gray, 0)

        current_ear = 0.0
        current_drowsy = False

        for face in faces:
            shape = predictor(gray, face)
            
            # 1. Eye Aspect Ratio (EAR)
            leftEye = [(shape.part(i).x, shape.part(i).y) for i in range(36, 42)]
            rightEye = [(shape.part(i).x, shape.part(i).y) for i in range(42, 48)]
            leftEAR = eye_aspect_ratio(leftEye)
            rightEAR = eye_aspect_ratio(rightEye)
            current_ear = (leftEAR + rightEAR) / 2.0

            # 2. Mouth Aspect Ratio (Yawning)
            mouth = [(shape.part(i).x, shape.part(i).y) for i in range(48, 68)]
            mar = mouth_aspect_ratio(mouth)

            # 3. Head Tilt
            is_tilted = get_head_tilt(shape)

            # --- Logic Triggers ---
            if current_ear < EAR_THRESHOLD:
                drowsy_counter += 1
            else:
                drowsy_counter = 0

            if mar > MAR_THRESHOLD:
                yawn_counter += 1
            else:
                yawn_counter = 0

            # If eyes closed OR yawning continuously OR head tilted excessively
            if drowsy_counter >= CONSECUTIVE_FRAMES or yawn_counter >= YAWN_CONSECUTIVE_FRAMES or is_tilted:
                current_drowsy = True
                cv2.putText(frame, "DROWSINESS/FATIGUE ALERT!", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

            cv2.putText(frame, f"EAR: {current_ear:.2f} MAR: {mar:.2f}", (10, 60),
                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
            if is_tilted:
                cv2.putText(frame, "HEAD TILT DETECTED", (10, 90),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 165, 255), 2)

        # Update thread-safe variables
        with vision_lock:
            shared_ear = current_ear
            shared_is_drowsy = current_drowsy

        cv2.imshow("Advanced Driver Monitoring", frame)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cv2.destroyAllWindows()
    vs.release()
