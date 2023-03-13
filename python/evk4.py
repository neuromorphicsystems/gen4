import dataclasses
import pathlib
import evk4_extension
import re
import dataclasses


@dataclasses.dataclass
class Biases:
    pr: int = 0x7C
    fo: int = 0x53
    hpf: int = 0x00
    diff_on: int = 0x66
    diff: int = 0x4D
    diff_off: int = 0x49
    inv: int = 0x5B
    refr: int = 0x14
    reqpuy: int = 0x8C
    reqpux: int = 0x7C
    sendreqpdy: int = 0x94
    unknown_1: int = 0x74
    unknown_2: int = 0x51


@dataclasses.dataclass
class Parameters:
    biases: Biases


@dataclasses.dataclass
class RecordingStatus:
    name: str = ""
    duration: int = 0
    size: int = 0


recording_name_pattern = re.compile(r"^[-\w .]+$")


class Camera(evk4_extension.Camera):
    def __init__(
        self,
        recordings_path: pathlib.Path,
        log_path: pathlib.Path,
        slice_duration: int,
        slice_count: int,
        slice_initial_capacity: int,
    ):
        recordings_path.mkdir(exist_ok=True, parents=True)
        log_path.parent.mkdir(exist_ok=True, parents=True)
        assert slice_duration > 0
        assert slice_count > 0
        assert slice_initial_capacity >= 0
        super().__init__(
            recordings_path,
            log_path,
            slice_duration,
            slice_count,
            slice_initial_capacity,
        )

    def set_parameters(self, parameters: Parameters):
        super().set_parameters(dataclasses.asdict(parameters))

    def start_recording_to(self, name: str):
        name = name.strip()
        if recording_name_pattern.match(name) is None:
            raise Exception(
                "the recording name can only contain alphanumeric characters, -, and _"
            )
        super().record_to(name)

    def stop_recording(self):
        super().record_to("")

    def recording_status(self):
        data = super().recording_status()
        return RecordingStatus(name=data[0], duration=data[1], size=data[2])
