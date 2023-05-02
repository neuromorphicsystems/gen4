import datetime
import pathlib
import evk4
import time

dirname = pathlib.Path(__file__).resolve().parent

camera = evk4.Camera(
    recordings_path=dirname / "recordings",
    log_path=dirname / "recordings" / "log.jsonl",
)
camera.set_parameters(
    evk4.Parameters(
        biases=evk4.Biases(
            diff_on=150,  # default 102
            diff_off=150,  # default 73
        )
    )
)

camera.start_recording_to(
    f'{datetime.datetime.now(tz=datetime.timezone.utc).isoformat().replace("+00:00", "Z").replace(":", "-")}.es'
)

while True:
    backlog = camera.backlog()
    events = camera.all_packets()
    print(f"backlog = {backlog}, events = {len(events)}, status = {camera.recording_status()}")
    time.sleep(1.0)
