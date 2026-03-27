"""
main.py
A real-time MOT pipeline comparing Hugging Face RT-DETR with RF-DETR using ByteTrack.
"""

import cv2
import torch
import supervision as sv

# For the Hugging Face Transformer approach
from transformers import RTDETRForObjectDetection, RTDETRImageProcessor

# For the optimized RF-DETR approach
from inference import get_model


def process_video():
    # 1. Configuration
    SOURCE_VIDEO = "data/input_videos/traffic.mp4"
    TARGET_VIDEO = "data/output_videos/traffic_tracked.mp4"

    # Toggle this to False to see the RF-DETR implementation in action
    USE_HF_TRANSFORMERS = True
    device = "cuda" if torch.cuda.is_available() else "cpu"

    # 2. Model Initialization
    if USE_HF_TRANSFORMERS:
        print("Loading Hugging Face RT-DETR...")
        processor = RTDETRImageProcessor.from_pretrained("HuggingFaceM4/rt-detr-l")
        model = RTDETRForObjectDetection.from_pretrained("HuggingFaceM4/rt-detr-l").to(
            device
        )
    else:
        print("Loading RF-DETR (Optimized for Real-Time)...")
        # rf-detr through Roboflow Inference is explicitly designed for this stack
        rf_model = get_model(model_id="rfdetr-medium")

    # 3. Supervision Tracking & Annotation Setup
    tracker = sv.ByteTrack()
    box_annotator = sv.BoxAnnotator()
    label_annotator = sv.LabelAnnotator()
    trace_annotator = sv.TraceAnnotator()

    # 4. Video Processing Loop
    video_info = sv.VideoInfo.from_video_path(SOURCE_VIDEO)
    frames_generator = sv.get_video_frames_generator(SOURCE_VIDEO)

    # VideoSink handles the threaded writing of the output video
    with sv.VideoSink(target_path=TARGET_VIDEO, video_info=video_info) as sink:
        for frame in frames_generator:
            # --- DETECTION PHASE ---
            if USE_HF_TRANSFORMERS:
                # The Hugging Face Pipeline
                inputs = processor(images=frame, return_tensors="pt").to(device)
                with torch.no_grad():
                    outputs = model(**inputs)

                # Post-process HF output
                target_sizes = torch.tensor([frame.shape[:2]]).to(device)
                results = processor.post_process_object_detection(
                    outputs, target_sizes=target_sizes, threshold=0.3
                )[0]

                # The "Translation Layer": Manually mapping HF outputs to sv.Detections
                detections = sv.Detections(
                    xyxy=results["boxes"].cpu().numpy(),
                    confidence=results["scores"].cpu().numpy(),
                    class_id=results["labels"].cpu().numpy(),
                )
                id2label = model.config.id2label

            else:
                # The RF-DETR Pipeline
                # RF-DETR returns results that map directly to Supervision using a built-in method
                result = rf_model.infer(frame, confidence=0.3)[0]
                detections = sv.Detections.from_inference(result)

                # Fetching class names for labels
                class_names = getattr(rf_model, "class_names", {})
                id2label = (
                    {i: name for i, name in enumerate(class_names)}
                    if isinstance(class_names, list)
                    else class_names
                )

            # --- TRACKING PHASE ---
            # ByteTrack handles the Kalman Filtering and Hungarian matching under the hood
            detections = tracker.update_with_detections(detections)

            # --- ANNOTATION PHASE ---
            labels = [
                f"#{tracker_id} {id2label.get(class_id, 'object')} {conf:.2f}"
                for class_id, tracker_id, conf in zip(
                    detections.class_id, detections.tracker_id, detections.confidence
                )
            ]

            # Stack annotations cleanly over the frame
            annotated_frame = frame.copy()
            annotated_frame = trace_annotator.annotate(
                scene=annotated_frame, detections=detections
            )
            annotated_frame = box_annotator.annotate(
                scene=annotated_frame, detections=detections
            )
            annotated_frame = label_annotator.annotate(
                scene=annotated_frame, detections=detections, labels=labels
            )

            # --- RENDER / OUTPUT ---
            sink.write_frame(frame=annotated_frame)
            cv2.imshow("Real-Time MOT", annotated_frame)

            # Press 'q' to gracefully exit the video stream
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

    cv2.destroyAllWindows()


if __name__ == "__main__":
    process_video()
