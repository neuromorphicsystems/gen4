import argparse
import json
import pathlib
import subprocess

dirname = pathlib.Path(__file__).resolve().parent

parser = argparse.ArgumentParser(
    description="app displays and records events from an EVK4 camera",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)
parser.add_argument("--serial", default=None, help="camera serial, for example P50420")
parser.add_argument(
    "--recordings", default=str(dirname / "recordings"), help="recordings directory"
)
parser.add_argument(
    "--biases", default=str(dirname / "biases.json"), help="default biases"
)
args = parser.parse_args()

pathlib.Path(args.recordings).mkdir(parents=True, exist_ok=True)
command = [
    str(dirname / "app" / "build" / "bin" / "release" / "evk4_recorder"),
    f"--recordings={args.recordings}",
]
if args.serial is not None:
    serial = args.serial.replace("P", "").zfill(8)
    command.append(f"--serial={serial}")
with open(args.biases) as biases_file:
    biases = json.load(biases_file)
for name in (
    "pr",
    "fo",
    "hpf",
    "diff_on",
    "diff",
    "diff_off",
    "inv",
    "refr",
    "reqpuy",
    "reqpux",
    "sendreqpdy",
    "unknown_1",
    "unknown_2",
):
    command.append(f"--{name.replace('_', '-')}={biases[name]}")
subprocess.run(command)
