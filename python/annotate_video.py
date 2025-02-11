#!/usr/bin/env python3

import argparse
from pathlib import Path
from enum import IntEnum
import textwrap
from typing import TypeAlias, Optional

import cv2
import numpy as np

from decord import VideoReader, cpu


class AnnotationMode(IntEnum):
    SEEK = 1
    # Drawing modes
    DRAW_RECT = 2
    DRAW_POLY = 3
    DRAW_ELLIPSE = 4
    # Shape editing modes
    EDIT_RECT = 5
    EDIT_ELLIPSE = 6
    EDIT_POLY = 7

class PoeRoi(IntEnum):
    """
    Regions of interest for PoE2 UI.
    """
    HEALTH_GLOBE = 1
    ENERGY_SHIELD = 2
    MANA_GLOBE = 3
    EXPERIENCE_BAR = 4
    MINIMAP = 5
    BUFFS_REGION = 6


# TODO: Legend for each anotation mode
LEGENDS = {
    # TODO: show rectangle id (num) when i is pressed and select using integer
    AnnotationMode.SEEK: textwrap.dedent(
        """\
        MODE: frame seeking
        h: show help
        j: next frame
        k: prev frame
        r: draw rect
        c: draw ellipse
        e: edit roi
        d: delete roi
        """
    ),
    # TODO: convenient roi editing keybindings
    # - arrows to move
    # - shift-arrow to resize 
    AnnotationMode.DRAW_RECT: textwrap.dedent(
        """\
        MODE: rectangle annotation
        (Draw four corners, counter-clockwise)
        h: show help
        """
    ),
    AnnotationMode.EDIT_RECT: textwrap.dedent(
        """\
        MODE: rectangle editing (Move corners)
        h: show help
        UP/DOWN/L/R: move rect
        SHIFT+U/D: move top border
        CTRL+U/D: move bottom border
        SHIFT+L/R: move left border
        CTRL+L/R: move right border
        """
    ),
    AnnotationMode.DRAW_ELLIPSE: textwrap.dedent(
        """\
        MODE: ellipse annotation
        (Draw center, r1, r2)
        h: show help
        """
    ),
    AnnotationMode.EDIT_ELLIPSE: textwrap.dedent(
        """\
        MODE: ellipse editing
        (edit center, r1, r2)
        h: show help
        UP/DOWN/L/R: move
        SHIFT+U/D: resize r1
        SHIFT+L/R: resize r2
        """
    ),
    AnnotationMode.DRAW_POLY: textwrap.dedent(
        """\
        MODE: polygon annotation
        (Draw vertices, counter-clockwise)
        h: show help
        """
    )
}

PolygonCoords: TypeAlias = list[tuple[float, float]]

class VideoAnnotator:
    """
    Annotate ROIs in video frames interactively.
    """

    def __init__(self):
        self.polygons_per_frame: dict[PoeRoi] = {}  # Dictionary: frame_index -> list of polygons (each is a list of (x, y) points)
        self.current_polygon: PolygonCoords = []     # Points of the polygon currently being drawn
        self.current_frame_index = 0
        self.current_roi: Optional[PoeRoi] = None

        self.WINDOW_NAME = "Annotation Tool"

        self.mode = AnnotationMode.SEEK
    

    def draw_polygons(self, image, polygons, current_polygon=None):
        """
        Draw existing polygons and the current (in-progress) polygon on the image.
        """
        overlay = image.copy()

        # Draw finished polygons in green
        for poly in polygons:
            pts = np.array(poly, dtype=np.int32).reshape((-1, 1, 2))
            cv2.polylines(overlay, [pts], isClosed=True, color=(0, 255, 0), thickness=2)

        # Draw current in-progress polygon in blue
        if current_polygon and len(current_polygon) > 1:
            pts = np.array(current_polygon, dtype=np.int32).reshape((-1, 1, 2))
            cv2.polylines(overlay, [pts], isClosed=False, color=(255, 0, 0), thickness=2)

        return overlay

    def mouse_callback(self, event, x, y, flags, param):
        """
        Mouse callback for drawing polygons.
        """
        # TODO: callbacks depending on the mode we are in

        if event == cv2.EVENT_LBUTTONDOWN:
            # Left-click: add a point to the current polygon
            self.current_polygon.append((x, y))

        elif event == cv2.EVENT_RBUTTONDOWN:
            # Right-click: finalize (close) the current polygon
            if len(self.current_polygon) > 2:
                # Add the polygon to the list for the current frame
                if self.current_frame_index not in self.polygons_per_frame:
                    self.polygons_per_frame[self.current_frame_index] = []
                self.polygons_per_frame[self.current_frame_index].append(self.current_polygon.copy())

            # Reset the current polygon
            self.current_polygon = []


    def run(self, video_path: Path):
        """
        Video annotation main loop (blocking).
        """

        # 1. Open the video using `decord` (CPU context)
        with open(video_path, 'rb') as f:
            vr = VideoReader(f, ctx=cpu(0))
        total_frames = len(vr)
        if total_frames == 0:
            print("No frames found in the video. Exiting.")
            return

        # 2. Create an OpenCV window and set the mouse callback
        cv2.namedWindow(self.WINDOW_NAME, cv2.WINDOW_NORMAL)
        cv2.setMouseCallback(self.WINDOW_NAME, self.mouse_callback)

        print("Use left/right arrow keys to navigate frames. Right-click to finalize a polygon.")
        print("Press 'q' to quit.")

        # 3. Main loop: show each frame on demand
        while True:
            
            # Detect if the user closed the window
            if cv2.getWindowProperty(self.WINDOW_NAME, cv2.WND_PROP_VISIBLE) < 1:
                break

            # Fetch the current frame via decord indexing
            frame_nd = vr[self.current_frame_index]          # returns an NDArray in RGB
            frame_bgr = cv2.cvtColor(frame_nd.asnumpy(), cv2.COLOR_RGB2BGR)

            # Retrieve existing polygons for this frame
            existing_polygons = self.polygons_per_frame.get(self.current_frame_index, [])

            # Draw polygons onto the image
            display_img = self.draw_polygons(frame_bgr, existing_polygons, self.current_polygon)

            # Display the frame
            cv2.imshow(self.WINDOW_NAME, display_img)

            # Handle keyboard input
            key = cv2.waitKey(30)
            key_alpha = key & 0xFF  # alphanumeric keys
            
            # TODO: add state machine to switch between drawing modes
            if key_alpha == ord('q'):
                break
            
            elif key_alpha == ord('k') or key == 81:
                if self.current_frame_index > 0:
                    self.current_frame_index -= 1
                    # Reset current polygon when switching frames
                    self.current_polygon.clear()
                    print(f"Seek to frame {self.current_frame_index}")
            
            elif key_alpha == ord('j') or key == 83:
                if self.current_frame_index < total_frames - 1:
                    self.current_frame_index += 1
                    # Reset current polygon when switching frames
                    self.current_polygon.clear()
                    print(f"Seek to frame {self.current_frame_index}")
            
            elif key_alpha != 255:
                print(f"Unrecognized key: {key}")

        # 4. Clean up
        cv2.destroyAllWindows()

        # Print the recorded polygons. Replace this with logic to export/save if needed.
        print("Polygons per frame:")
        for f_idx, polygons in self.polygons_per_frame.items():
            print(f"Frame {f_idx}:")
            for i, poly in enumerate(polygons):
                print(f"  Polygon {i}: {poly}")

def main(video_path: Path):
    """
    Start video annotation tool.

    TODO: improve the tool
    - not polygons per frame, just one set
    - set polygon labels (enum for required annotations)
    - drawing modes: rectangle, polygon, circle, elipse?
        - show legend per drawing mode (show/hide option)
    - export in sensible format
    - load existing polygons from QuPath ?
    - automatic detection using SAM2
    """

    if not video_path.is_file():
        raise ValueError(f"Not a file: {video_path}")
    
    annotator = VideoAnnotator()
    annotator.run(video_path)

    



if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="Video annotation tool using decord (on-demand frame access).")
    parser.add_argument("video_path", help="Path to the video file.", type=Path)
    args = parser.parse_args()

    main(args.video_path)
