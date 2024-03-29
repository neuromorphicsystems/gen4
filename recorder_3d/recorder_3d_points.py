import numpy as np
import sys
import pathlib
import datetime
import point_cloud
import evk4_recorder_3d
import themes
import time
import vispy.app
import vispy.color
import vispy.scene
import vispy.visuals
import vispy.visuals.transforms
import PyQt5.QtWidgets
import PyQt5.uic

vispy.app.use_app("pyqt5")

dirname = pathlib.Path(__file__).resolve().parent
form_class = PyQt5.uic.loadUiType(dirname / "recorder_3d.ui")[0]


class MyWindow(PyQt5.QtWidgets.QMainWindow, form_class):
    def __init__(self, camera, fps, parent=None):
        PyQt5.QtWidgets.QMainWindow.__init__(self, parent)
        self.setupUi(self)
        self.setGeometry(0, 0, 1400, 800)
        self.setWindowTitle("EBC 3D")
        self.camera = camera
        self.diff = 77
        self.diff_on = 122
        self.diff_off = 53
        self.setBiases()
        self.horizontalSliderOn.setValue(self.diff_on - self.diff)
        self.horizontalSliderOff.setValue(int(self.diff - self.diff_off))
        self.horizontalSliderOn.valueChanged[int].connect(self.setOnThreshold)
        self.horizontalSliderOff.valueChanged[int].connect(self.setOffThreshold)
        self.mirror = True
        self.checkBoxMirror.setChecked(self.mirror)
        self.checkBoxMirror.stateChanged.connect(self.toggle_mirror)
        self.paused = False
        self.checkBoxPause.stateChanged.connect(self.toggle_pause)
        self.threeD = False
        self.checkBox3D.stateChanged.connect(self.toggle_3d)
        self.showOn = True
        self.showFrames = False
        self.checkBoxOn.setChecked(self.showFrames)
        self.checkBoxOn.setText("Frames")
        self.checkBoxOn.stateChanged.connect(self.toggle_frames)
        self.showPerspective = False
        self.checkBoxOff.setChecked(self.showPerspective)
        self.checkBoxOff.setText("Perspective")
        self.checkBoxOff.stateChanged.connect(self.toggle_perspective)
        self.recording = False
        self.checkBoxRecord.stateChanged.connect(self.toggle_recording)
        self.comboBoxTheme.currentIndexChanged.connect(self.set_theme)
        self.stopped = False

        self.camera = camera
        self.time_window = 0.1  # s
        self.fps = fps
        self.w = 1280
        self.h = 720
        self.on_events = np.zeros(2**20, dtype=[("position", np.float32, 3)])
        self.off_events = np.zeros(2**20, dtype=[("position", np.float32, 3)])
        self.counts = np.array(
            [int(fps * self.time_window), 0, 0, 0, 0], dtype=np.uint64
        )
        self.space_scale = 1 / self.w  # GL coordinates / px
        self.time_scale = 1e-8  # GL coordinates / µs
        self.updateMatrix()

        self.next_update: float = 0.0
        self.frame_duration = 1.0 / self.fps
        self.half_frame_duration = self.frame_duration / 2.0

        self.canvas = vispy.scene.SceneCanvas(
            title="PSEE 413",
            size=(1280, 720),
            # keys="interactive",
            keys={
                "q": self.exit,
                "m": self.toggle_m,
                "3": self.toggle_three,
                "f": self.toggle_f,
                "p": self.toggle_p,
                "r": self.toggle_r,
                "Space": self.toggle_space,
            },
            resizable=True,
            show=True,
            bgcolor=themes.default.background,
            vsync=True,
        )
        self.view = self.canvas.central_widget.add_view()
        self.view.camera = vispy.scene.cameras.turntable.TurntableCamera(
            fov=0.0,  # use 0.0 for orthographic projection, and other values (typically 45.0) for pin-hole projection
            elevation=0.0,
            azimuth=0.0,
            scale_factor=0.7,
        )
        self.view.camera.center = [0.5, self.time_window * 10 / 2, 0.5 * 9 / 16]
        self.axis = vispy.scene.visuals.Line(
            pos=np.array(
                [
                    [0, 0, 0],
                    [1, 0, 0],
                    [1, self.time_window * 10, 0],
                    [0, self.time_window * 10, 0],
                    [0, 0, 0],
                    [0, 0, self.h / self.w],
                    [0, self.time_window * 10, self.h / self.w],
                    [0, self.time_window * 10, 0],
                ]
            ),
            color=vispy.color.ColorArray(themes.default.axis).rgba,
            connect="strip",
        )
        self.axis.antialias = False
        self.view.add(self.axis)
        slices = int(maximum_time_window / self.time_window + 1)
        self.ticks_pos = np.zeros((slices, 3))
        self.ticks_pos[:, 1] = np.arange(slices) * self.time_window
        self.markers = vispy.scene.visuals.Markers(
            pos=self.ticks_pos,
            symbol="cross",
            edge_color=None,
            face_color=vispy.color.ColorArray(themes.default.axis).rgba,
        )
        self.view.add(self.markers)
        self.on = vispy.scene.visuals.create_visual_node(point_cloud.PointCloudVisual)(
            vispy.color.ColorArray(themes.default.on).rgba,
            base_point_size=1.05 * self.space_scale,
        )
        self.view.add(self.on)
        self.off = vispy.scene.visuals.create_visual_node(point_cloud.PointCloudVisual)(
            vispy.color.ColorArray(themes.default.off).rgba,
            base_point_size=1.05 * self.space_scale,
        )
        self.view.add(self.off)
        self.updateMatrix()
        self.on.transform = vispy.visuals.transforms.linear.MatrixTransform(
            matrix=self.matrix
        )
        self.off.transform = vispy.visuals.transforms.linear.MatrixTransform(
            matrix=self.matrix
        )
        self.on.transform.translate(np.array([self.mirror, 0, 0]))
        self.off.transform.translate(np.array([self.mirror, 0, 0]))
        self.verticalLayout.addWidget(self.canvas.native)
        self.comboBoxTheme.insertItems(0, themes.names)
        self.timer = vispy.app.Timer(
            interval=str(self.frame_duration), connect=self.update, start=True
        )

    def update(self, _):
        if not self.stopped:
            now = time.monotonic()
            if now - self.next_update > self.half_frame_duration:
                self.next_update = now + self.frame_duration
            else:
                self.next_update += self.frame_duration
                if not self.paused:
                    self.camera.update_points(
                        self.on_events, self.off_events, self.counts
                    )
                    self.redraw()
        else:
            # clean up and exit
            self.timer.stop()
            self.close()
            vispy.app.quit()

    def updateMatrix(self):
        self.matrix = np.array(
            [
                [-self.space_scale * (2 * self.mirror - 1), 0.0, 0.0, 0.0],
                [0.0, 0.0, self.space_scale, 0.0],
                [0.0, -self.time_scale, 0.0, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ]
        )

    def redraw(self):
        end_t = float(self.counts[4])
        self.on.end_t = end_t
        self.off.end_t = end_t
        self.on.set_data(self.on_events[0 : self.counts[1]])
        self.off.set_data(self.off_events[0 : self.counts[2]])
        self.canvas.update()

    def setOnThreshold(self):
        self.diff_on = int(self.diff + self.horizontalSliderOn.value())
        self.setBiases()

    def setOffThreshold(self):
        self.diff_off = int(self.diff - self.horizontalSliderOff.value())
        self.setBiases()

    def setBiases(self):
        self.camera.set_parameters(
            evk4_recorder_3d.Parameters(
                biases=evk4_recorder_3d.Biases(
                    diff_on=self.diff_on,  # default 102
                    diff=self.diff,
                    diff_off=self.diff_off,  # default 73
                )
            )
        )

    def toggle_space(self):
        self.checkBoxPause.setChecked(not self.paused)

    def toggle_pause(self):
        self.paused = not self.paused

    def toggle_m(self):
        self.checkBoxMirror.setChecked(not self.mirror)

    def toggle_mirror(self):
        self.mirror = not self.mirror
        self.updateMatrix()
        self.on.transform = vispy.visuals.transforms.linear.MatrixTransform(
            matrix=self.matrix
        )
        self.off.transform = vispy.visuals.transforms.linear.MatrixTransform(
            matrix=self.matrix
        )
        self.on.transform.translate(np.array([self.mirror, 0, 0]))
        self.off.transform.translate(np.array([self.mirror, 0, 0]))

    def toggle_three(self):
        self.checkBox3D.setChecked(not self.threeD)

    def toggle_3d(self):
        self.threeD = not self.threeD
        if self.threeD:
            self.time_scale = 1e-5
        else:
            self.time_scale = 1e-8
        self.updateMatrix()
        self.on.transform = vispy.visuals.transforms.linear.MatrixTransform(
            matrix=self.matrix
        )
        self.off.transform = vispy.visuals.transforms.linear.MatrixTransform(
            matrix=self.matrix
        )
        self.on.transform.translate(np.array([self.mirror, 0, 0]))
        self.off.transform.translate(np.array([self.mirror, 0, 0]))

    def toggle_f(self):
        self.checkBoxOn.setChecked(not self.showFrames)

    def toggle_frames(self):
        self.showFrames = not self.showFrames
        self.on.floor_timestamps = self.showFrames
        self.off.floor_timestamps = self.showFrames
        if not self.threeD:
            self.toggle_three()
        self.canvas.update()

    def toggle_p(self):
        self.checkBoxOff.setChecked(not self.showPerspective)

    def toggle_perspective(self):
        self.showPerspective = not self.showPerspective
        assert isinstance(
            self.view.camera, vispy.scene.cameras.turntable.TurntableCamera
        )
        self.view.camera.fov = 10.0 - self.view.camera.fov
        self.view.camera.scale_factor = 0.7
        self.view.camera.azimuth = 0.0
        self.view.camera.elevation = 0.0
        self.view.camera.center = [0.5, 0.5, 0.28125]

    def toggle_r(self):
        self.checkBoxRecord.setChecked(not self.recording)

    def toggle_recording(self):
        if self.recording:
            self.camera.stop_recording()
            self.recording = False
            print("Recording off")
        else:
            name = f'{datetime.datetime.now(tz=datetime.timezone.utc).isoformat().replace("+00:00", "Z").replace(":", "-")}.es'
            self.camera.start_recording_to(name)
            self.recording = True
            print("Recording to " + name)

    def set_theme(self):
        theme = themes.values[self.comboBoxTheme.currentIndex()]
        self.canvas.bgcolor = theme.background
        self.on.set_color(vispy.color.ColorArray(theme.on).rgba)
        self.off.set_color(vispy.color.ColorArray(theme.off).rgba)
        self.axis.set_data(
            pos=self.axis.pos,
            color=vispy.color.ColorArray(theme.axis).rgba,
            connect=self.axis.connect,
        )
        self.markers.set_data(
            pos=self.ticks_pos,
            edge_color=None,  # type: ignore
            face_color=theme.axis,
        )

    def exit(self):
        self.stopped = True


if __name__ == "__main__":
    gui = PyQt5.QtWidgets.QApplication(sys.argv)
    dirname = pathlib.Path(__file__).resolve().parent

    maximum_time_window = 1  # s
    fps = 30  # slices per second

    # Camera
    camera = evk4_recorder_3d.Camera(
        recordings_path=dirname / "recordings",
        log_path=dirname / "recordings" / "log.jsonl",
        slice_duration=1000000 // fps,  # µs
        slice_count=maximum_time_window * fps,
        slice_initial_capacity=2**24,  # events
    )
    w = MyWindow(camera, fps)
    w.show()
    vispy.app.run()
