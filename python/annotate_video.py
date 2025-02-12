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
    ROI_SELECTION = 2
    # Drawing modes
    DRAW_RECT = 3
    DRAW_POLY = 4
    DRAW_ELLIPSE = 5
    # Shape editing modes
    EDIT_RECT = 6
    EDIT_ELLIPSE = 7
    EDIT_POLY = 8
    # Deletion
    DELETE_ROI = 9

SHAPE_EDIT_MODES = (
    AnnotationMode.EDIT_RECT,
    AnnotationMode.EDIT_ELLIPSE,
    AnnotationMode.EDIT_POLY,
)

class HudComponent(IntEnum):
    """
    HUD elements of the PoE2 user interface that we want to annotate.

    Note that there is only one annotation of each type for the entire video.
    """
    HEALTH_GLOBE = 1
    ENERGY_SHIELD = 2
    MANA_GLOBE = 3
    EXPERIENCE_BAR = 4
    MINIMAP = 5
    BUFFS_REGION = 6

# Extended keycodes on Windows
ARROW_KEY_LEFT = 2424832
ARROW_KEY_RIGHT = 2555904
ARROW_KEY_UP = 2490368
ARROW_KEY_DOWN = 2621440
KEYBOARD_ARROW_KEYS = ARROW_KEY_LEFT, ARROW_KEY_RIGHT, ARROW_KEY_DOWN, ARROW_KEY_UP

LEGENDS = {
    # Seek mode
    # - use can seek in the video/skip between frames
    # - enter different ROI drawing/editing modes
    # - when (e) or (d) is pressed to edit or draw an ROI:
    #   first display their integer enum numbers in the center of each ROI,
    #   and let user select with number keypress. Depending on the ROI that's
    #   selected for editing, enter the corresponding ROI editing mode
    AnnotationMode.SEEK: textwrap.dedent(
        """\
        MODE: frame seeking (frame {frame_number})
        h: show help
        j: next frame
        k: prev frame
        r: draw rect
        c: draw ellipse
        e: edit roi
        d: delete roi
        """
    ),
    # Select the PoE2 ROI that you want to draw or edit
    AnnotationMode.ROI_SELECTION: textwrap.dedent(
        """\
        MODE: ROI selection
        h: show help
        esc: return to seek
        """
    ) + "\n".join(
        f"{roi.value}: {roi.name}" for roi in list(HudComponent)
    ),
    # Draw upper left vertex, bottom boundary, right boundary
    AnnotationMode.DRAW_RECT: textwrap.dedent(
        """\
        MODE: rectangle annotation
        (Three points, counter-clockwise)
        h: show help
        """
    ),
    # TODO: convenient roi editing keybindings
    # - arrows to move the ROI
    # - shift-arrow to resize
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
        (Draw center, radii rv and rh)
        h: show help
        """
    ),
    AnnotationMode.EDIT_ELLIPSE: textwrap.dedent(
        """\
        MODE: ellipse editing
        (edit center, rv, rh)
        h: show help
        UP/DOWN/L/R: move
        SHIFT+U/D: resize rv
        SHIFT+L/R: resize rh
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
        self.hud_rois: dict[HudComponent, dict] = {}  # Dictionary: PoeRoi -> {type: str, coords: tuple}
        self.current_points: PolygonCoords = []  # Points for the current shape being drawn
        self.current_mouse_pos = (0, 0)  # Current mouse position for preview
        self.current_frame_index = 0
        self.current_hud_item: Optional[HudComponent] = None  # Currently selected ROI for editing
        self.mode = AnnotationMode.SEEK            # current annotation mode
        self.target_mode: Optional[AnnotationMode] = None  # mode we want to end up in
        self.WINDOW_NAME = "Annotation Tool"
        # Track modifier keys
        self.shift_pressed = False
        self.ctrl_pressed = False
        self.show_legend = True


    def draw_legend(self, image):
        """
        Draw the current annotation mode's legend on the image.
        """
        legend_text = LEGENDS.get(self.mode, "")
        if self.mode == AnnotationMode.SEEK:
            legend_text = legend_text.format(frame_number=self.current_frame_index)
        lines = legend_text.split('\n')

        y = 30  # Starting y position
        for line in lines:
            cv2.putText(image, line, (10, y), cv2.FONT_HERSHEY_SIMPLEX,
                        0.6, (73, 235, 52), 2)
            y += 25
        return image

    # ========================================================================
    # Mouse callbacks for each FSM state / annotation mode
    # ========================================================================

    def mouse_callback(self, event, x, y, flags, param):
        """
        Main mouse callback that delegates to mode-specific handlers.
        """
        self.current_mouse_pos = (x, y)

        if self.mode == AnnotationMode.SEEK:
            if event in (cv2.EVENT_LBUTTONDOWN, cv2.EVENT_RBUTTONDOWN):
                print("Enter ROI drawing mode to start annotating.")
        elif self.mode == AnnotationMode.DRAW_RECT:
            self.mode_drawrect_callback(event, x, y, flags, param)
        elif self.mode == AnnotationMode.DRAW_ELLIPSE:
            self.mode_drawellipse_callback(event, x, y, flags, param)
        elif self.mode == AnnotationMode.DRAW_POLY:
            self.mode_drawpoly_callback(event, x, y, flags, param)
        # elif self.mode == AnnotationMode.EDIT_RECT:
        #     self.edit_rect_callback(event, x, y, flags, param)
        # elif self.mode == AnnotationMode.EDIT_ELLIPSE:
        #     self.edit_ellipse_callback(event, x, y, flags, param)
        

    def mode_drawrect_callback(self, event, x, y, flags, param):
        """
        Mouse callback for rectangle drawing mode.
        """
        if event == cv2.EVENT_LBUTTONDOWN:
            if not self.current_points:
                self.current_points = [(x, y)]
            else:
                pt1 = self.current_points[0]
                pt2 = (x, y)
                if self.current_hud_item:
                    self.hud_rois[self.current_hud_item] = {
                        'type': 'rect',
                        'coords': (pt1, pt2)
                    }
                self.current_points = []
                self.mode = AnnotationMode.SEEK

    def mode_drawellipse_callback(self, event, x, y, flags, param):
        """
        Mouse callback for ellipse drawing mode.
        """
        if event == cv2.EVENT_LBUTTONDOWN:
            if not self.current_points:
                self.current_points = [(x, y)]  # Center point
            else:
                center = self.current_points[0]
                axes = (abs(x - center[0]), abs(y - center[1]))
                if self.current_hud_item:
                    self.hud_rois[self.current_hud_item] = {
                        'type': 'ellipse',
                        'coords': (center, axes, 0)  # 0 is rotation angle
                    }
                self.current_points = []
                self.mode = AnnotationMode.SEEK


    def mode_drawpoly_callback(self, event, x, y, flags, param):
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


    def draw_shapes(self, image):
        """
        Draw all shapes on the image.
        """
        overlay = image.copy()

        # Draw existing shapes
        for roi_type, shape in self.hud_rois.items():

            if shape['type'] == 'rect':
                pt1, pt2 = shape['coords']
                cv2.rectangle(overlay, pt1, pt2, (0, 255, 0), 2)

            elif shape['type'] == 'ellipse':
                center, axes, angle = shape['coords']
                cv2.ellipse(overlay, center, axes, angle, 0, 360, (0, 255, 0), 2)

            elif shape['type'] == 'polygon':
                pts = np.array(shape['coords'], dtype=np.int32).reshape((-1, 1, 2))
                cv2.polylines(overlay, [pts], isClosed=True, color=(0, 255, 0), thickness=2)

        # Draw shape in progress
        if self.current_hud_item:

            if self.mode == AnnotationMode.DRAW_RECT and len(self.current_points) >= 1:
                pt1 = self.current_points[0]
                pt2 = (self.current_mouse_pos[0], self.current_mouse_pos[1])
                cv2.rectangle(overlay, pt1, pt2, (255, 0, 0), 2)

            elif self.mode == AnnotationMode.DRAW_ELLIPSE and len(self.current_points) >= 1:
                center = self.current_points[0]
                axes = (abs(self.current_mouse_pos[0] - center[0]),
                    abs(self.current_mouse_pos[1] - center[1]))
                cv2.ellipse(overlay, center, axes, 0, 0, 360, (255, 0, 0), 2)

            elif self.mode == AnnotationMode.DRAW_POLY and len(self.current_points) >= 1:
                pts = np.array(self.current_points, dtype=np.int32).reshape((-1, 1, 2))
                cv2.polylines(overlay, [pts], isClosed=False, color=(255, 0, 0), thickness=2)

        return overlay


    def draw_roi_numbers(self, image):
        """
        Draw the ROI id/number in the center of each ROI shape.
        """
        for roi_type, shape in self.hud_rois.items():
            # Get integer value of ROI enum for display
            roi_number = int(roi_type)

            if shape['type'] == 'rect':
                pt1, pt2 = shape['coords']
                # Calculate center of rectangle
                center_x = (pt1[0] + pt2[0]) // 2
                center_y = (pt1[1] + pt2[1]) // 2

            elif shape['type'] == 'ellipse':
                center, _, _ = shape['coords']
                center_x, center_y = center

            elif shape['type'] == 'polygon':
                # For polygon, calculate centroid
                pts = np.array(shape['coords'])
                center_x = int(np.mean(pts[:, 0]))
                center_y = int(np.mean(pts[:, 1]))

            # Draw the ROI number
            text = str(roi_number)
            font = cv2.FONT_HERSHEY_SIMPLEX
            font_scale = 1.0
            thickness = 2

            # Get text size to center it properly
            (text_width, text_height), _ = cv2.getTextSize(text, font, font_scale, thickness)
            text_x = center_x - text_width // 2
            text_y = center_y + text_height // 2

            # Draw text with contrasting outline for visibility
            cv2.putText(image, text, (text_x, text_y), font, font_scale, (0, 0, 0), thickness + 2)
            cv2.putText(image, text, (text_x, text_y), font, font_scale, (255, 255, 255), thickness)

        return image

    def run(self, video_path: Path):
        """
        Video annotation main loop (blocking).
        """

        # 1. Open the video using `decord` (CPU context)
        with open(video_path, 'rb') as f:
            vr = VideoReader(f, ctx=cpu(0))

        self.total_frames = len(vr)
        if self.total_frames == 0:
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

            # Draw shapes and legend
            display_img = self.draw_shapes(frame_bgr)
            if self.show_legend:
                display_img = self.draw_legend(display_img)

            cv2.imshow(self.WINDOW_NAME, display_img)

            key = cv2.waitKeyEx(100)  # rate limit the refresh time to 100ms
            key_alpha = key & 0xFF

            # Handle mode-independent keys first
            if key_alpha == ord('q'):
                break
            if key_alpha == ord('h'):
                # Toggle help overlay
                self.show_legend = not self.show_legend
                continue

            # Track modifier keys
            if key == 0x10:  # VK_SHIFT
                self.shift_pressed = True
            elif key == 0x11:  # VK_CONTROL
                self.ctrl_pressed = True
            elif key == -0x10:  # VK_SHIFT released
                self.shift_pressed = False
            elif key == -0x11:  # VK_CONTROL released
                self.ctrl_pressed = False

            # Handle keys based on current mode
            if self.mode == AnnotationMode.SEEK:
                self.mode_seek_transitions(key, display_img)

            elif self.mode == AnnotationMode.ROI_SELECTION:
                self.mode_roi_selection_transitions(key, display_img)

            elif self.mode in [AnnotationMode.DRAW_RECT, AnnotationMode.DRAW_ELLIPSE]:
                if key_alpha == 27:  # ESC
                    self.mode = AnnotationMode.SEEK
                    self.current_points = []
                    self.current_hud_item = None

            elif self.mode == AnnotationMode.EDIT_RECT:
                self.mode_edit_rect_transitions(key, display_img)

            elif self.mode == AnnotationMode.EDIT_ELLIPSE:
                self.mode_edit_ellipse_transitions(key, display_img)

            elif key_alpha != 255:
                print(f"Unrecognized key: {key}")

        # 4. Clean up
        cv2.destroyAllWindows()

        # Print the recorded polygons. Replace this with logic to export/save if needed.
        # TODO: export shapes

    # ========================================================================
    # Transition logic for Finite State Machine (FSM)
    # ========================================================================

    def mode_seek_transitions(self, key: int, display_img: np.ndarray):
        """
        FSM transitions for SEEK mode.
        """
        key_alpha = key & 0xFF

        if key_alpha == ord('r'):
            # Enter rectangle drawing mode - first select ROI
            # TODO: we want to update the legend at this point
            print("Select ROI type for rectangle")
            self.mode = AnnotationMode.ROI_SELECTION
            self.target_mode = AnnotationMode.DRAW_RECT

        elif key_alpha == ord('c'):
            # Enter ellipse drawing mode - first select ROI
            print("Select ROI type for rectangle")
            self.mode = AnnotationMode.ROI_SELECTION
            self.target_mode = AnnotationMode.DRAW_ELLIPSE

        elif key_alpha == ord('e'):
            # Enter edit mode - select ROI to edit
            print("Selecting ROI number to edit ...")
            self.mode = AnnotationMode.ROI_SELECTION
            self.target_mode = AnnotationMode.EDIT_POLY

        elif key_alpha == ord('d'):
            # Delete mode - select ROI to delete
            print("Selecting ROI number to delete ...")
            self.mode = AnnotationMode.ROI_SELECTION
            self.target_mode = AnnotationMode.DELETE_ROI

        elif key_alpha == ord('k') or key == ARROW_KEY_LEFT:  # Previous frame
            if self.current_frame_index > 0:
                self.current_frame_index -= 1
                print(f"Seek to frame {self.current_frame_index}")

        elif key_alpha == ord('j') or key == ARROW_KEY_RIGHT:  # Next frame
            if self.current_frame_index < self.total_frames - 1:
                self.current_frame_index += 1
                print(f"Seek to frame {self.current_frame_index}")

        elif key_alpha != 255:
            print(f"Unrecognized key: {key} for mode {self.mode}")


    def mode_roi_selection_transitions(self, key: int, display_img: np.ndarray):
        """
        FSM transitions for ROI_SELECTION mode.
        """
        print(f"Entering mode: {AnnotationMode.ROI_SELECTION.name}")

        img = self.draw_roi_numbers(display_img)
        cv2.imshow(self.WINDOW_NAME, img)

        roi_key = cv2.waitKey(0) & 0xFF     # Blocking: wait for key press
        try:
            self.current_hud_item = HudComponent(int(chr(roi_key)))

            # Select the next mode based on intended target mode
            if (self.target_mode in SHAPE_EDIT_MODES) and (self.current_hud_item in self.hud_rois):
                shape = self.hud_rois[self.current_hud_item]
                if shape['type'] == 'rect':
                    self.target_mode = AnnotationMode.EDIT_RECT
                elif shape['type'] == 'ellipse':
                    self.target_mode = AnnotationMode.EDIT_ELLIPSE

            elif self.target_mode == AnnotationMode.DELETE_ROI:
                del self.hud_rois[self.current_hud_item]
                print(f"Deleted ROI for HUD item '{self.current_hud_item.name}'")
                self.target_mode = AnnotationMode.SEEK

            self.mode = self.target_mode
            self.current_points = []

        except ValueError:
            print(f"Unknown PoE2 ROI : {chr(roi_key)}")
            self.mode = AnnotationMode.SEEK
            pass


    def mode_edit_rect_transitions(self, key: int, display_img: np.ndarray):
        """
        FSM transitions for EDIT_RECT mode.
        """
        print(f"Entering mode: {AnnotationMode.EDIT_RECT.name}")

        key_alpha = key & 0xFF

        if key_alpha == 27:  # ESC
            self.mode = AnnotationMode.SEEK
            self.current_hud_item = None

        elif key in KEYBOARD_ARROW_KEYS:

            # Get vertices and translate according to keypresses
            shape = self.hud_rois[self.current_hud_item]
            pt1, pt2 = shape['coords']

            # Get current modifier states
            if self.shift_pressed:
                # Shift+arrows: adjust top/left
                if key == ARROW_KEY_UP:  # Up
                    pt1 = (pt1[0], pt1[1] - 1)
                elif key == ARROW_KEY_DOWN:  # Down
                    pt1 = (pt1[0], pt1[1] + 1)
                elif key == ARROW_KEY_LEFT:  # Left
                    pt1 = (pt1[0] - 1, pt1[1])
                elif key == ARROW_KEY_RIGHT:  # Right
                    pt1 = (pt1[0] + 1, pt1[1])
            elif self.ctrl_pressed:
                # Ctrl+arrows: adjust bottom/right
                if key == ARROW_KEY_UP:  # Up
                    pt2 = (pt2[0], pt2[1] - 1)
                elif key == ARROW_KEY_DOWN:  # Down
                    pt2 = (pt2[0], pt2[1] + 1)
                elif key == ARROW_KEY_LEFT:  # Left
                    pt2 = (pt2[0] - 1, pt2[1])
                elif key == ARROW_KEY_RIGHT:  # Right
                    pt2 = (pt2[0] + 1, pt2[1])
            else:
                # No modifier: move entire rectangle
                dx = 1 if key == ARROW_KEY_RIGHT else (-1 if key == ARROW_KEY_LEFT else 0)
                dy = 1 if key == ARROW_KEY_DOWN else (-1 if key == ARROW_KEY_UP else 0)
                pt1 = (pt1[0] + dx, pt1[1] + dy)
                pt2 = (pt2[0] + dx, pt2[1] + dy)

            # Update two points that define rectangle
            self.hud_rois[self.current_hud_item]['coords'] = (pt1, pt2)

        elif key_alpha != 255:
            print(f"Unrecognized key: {key} for mode {self.mode.name}")

    def mode_edit_ellipse_transitions(self, key: int, display_img: np.ndarray):
        """
        FSM transitions for EDIT_ELLIPSE mode.
        """
        print(f"Entering mode: {AnnotationMode.EDIT_ELLIPSE.name}")

        key_alpha = key & 0xFF

        if key_alpha == 27:  # ESC
            self.mode = AnnotationMode.SEEK
            self.current_hud_item = None

        elif key in KEYBOARD_ARROW_KEYS:  # Arrow keys

            shape = self.hud_rois[self.current_hud_item]
            center, axes, angle = shape['coords']

            # Get current modifier state
            if self.shift_pressed:
                # Shift+arrows: adjust radii
                if key in [ARROW_KEY_DOWN, ARROW_KEY_UP]:  # Up/Down
                    axes = (axes[0], axes[1] + (1 if key == ARROW_KEY_UP else -1))
                else:  # Left/Right
                    axes = (axes[0] + (1 if key == ARROW_KEY_RIGHT else -1), axes[1])
            else:
                # No modifier: move center
                dx = 1 if key == ARROW_KEY_RIGHT else (-1 if key == ARROW_KEY_LEFT else 0)
                dy = 1 if key == ARROW_KEY_DOWN else (-1 if key == ARROW_KEY_UP else 0)
                center = (center[0] + dx, center[1] + dy)

            self.hud_rois[self.current_hud_item]['coords'] = (center, axes, angle)

        elif key_alpha != 255:
            print(f"Unrecognized key: {key} for mode {self.mode.name}")

def main(video_path: Path):
    """
    Start video annotation tool.
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
