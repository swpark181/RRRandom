"""Make paired, frame-synchronous review MP4s from ue_fox_anim_compare.py captures."""
import argparse
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("frames", type=Path)
parser.add_argument("destination", type=Path)
parser.add_argument("--ffmpeg", default="ffmpeg")
parser.add_argument("--after", default="after")
parser.add_argument("--before", default="baseline")
parser.add_argument("--species", help="Make one roster comparison with the species name on screen.")
args = parser.parse_args()
args.destination.mkdir(parents=True, exist_ok=True)

shots = [("strafe", "조준 옆걸음", 94), ("pivot", "달리다 급반전", 58),
         ("ears", "발소리를 향하는 귀", 70), ("tail", "급정지 뒤 꼬리", 71),
         ("climb", "높은 턱 오르기", 53)]
def label(title, x, y, size):
    return f"drawtext=fontfile='C\\:/Windows/Fonts/malgunbd.ttf':text='{title}':x={x}:y={y}:fontsize={size}:fontcolor=white"

for name, title, count in shots:
    inputs = []
    for take in (args.before, args.after):
        folder = args.frames / take
        assert all((folder / f"{name}_{i:03d}.png").is_file() for i in range(count))
        inputs += ["-framerate", "30", "-i", str(folder / (name + "_%03d.png"))]
    common = ("[0:v]scale=600:420,setsar=1[a];[1:v]scale=600:420,setsar=1[b];"
              "[a][b]hstack,pad=1200:482:0:52:color=0x17212b,"
              + label("적용 전",18,10,22) + "," + label("적용 후","w-tw-18",10,22)
              + ",drawbox=x=598:y=52:w=4:h=420:color=0x17212b:t=fill,split[n][s];")
    if args.species:
        # One continuous slow pass for the roster; keep chapter intermediates with the take.
        chapter_dir = args.frames / "chapters"
        chapter_dir.mkdir(exist_ok=True)
        caption = "발소리 반응" if name == "ears" else title
        graph = common.removesuffix(",split[n][s];") + ",setpts=1.5*PTS," + label(
            args.species + "   ·   " + caption, "(w-tw)/2", 12, 20) + ",fps=30[out]"
        subprocess.run([args.ffmpeg, "-v", "error", "-y", *inputs, "-filter_complex", graph,
                        "-map", "[out]", "-c:v", "libx264", "-crf", "20", "-pix_fmt", "yuv420p",
                        str(chapter_dir / (name+".mp4"))], check=True)
        continue
    normal = "[n]" + label(title + "   ·   실시간","(w-tw)/2",13,18) + "[real];"
    slow = "[s]setpts=2*PTS," + label(title + "   ·   0.5배속","(w-tw)/2",13,18) + "[slow];"
    subprocess.run([args.ffmpeg, "-v", "error", "-y", *inputs, "-filter_complex",
                    common + normal + slow + "[real][slow]concat=n=2:v=1:a=0,fps=30[out]",
                    "-map", "[out]", "-c:v", "libx264", "-crf", "20", "-pix_fmt", "yuv420p",
                    "-movflags", "+faststart", str(args.destination / (name+".mp4"))], check=True)

playlist = args.frames / "comparison-concat.txt"
source = (args.frames / "chapters") if args.species else args.destination
movie = (args.frames.name + ".mp4") if args.species else "FoxAnimationComparison.mp4"
playlist.write_text("".join("file '" + (source / (name+".mp4")).resolve().as_posix()
                            + "'\n" for name, _, _ in shots), encoding="utf-8")
subprocess.run([args.ffmpeg, "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", str(playlist),
                "-c", "copy", "-movflags", "+faststart", str(args.destination / movie)], check=True)
