"""Text prompt -> 2D image via Tripo API V3 (concept art).

Usage:
    python tripo_text_image.py "pastel dice island ..." --name dice_land/overview
    python tripo_text_image.py "..." --name dice_land/overview --resume   # reuse the finished task

Shares the API helpers with tripo_pipeline.py. Requires env var TRIPO_API_KEY.
Writes output/<name>.png and remembers the task id in output/<name>.state.json.
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tripo_pipeline import ROOT, download, ext_of, request, run_step  # noqa: E402


def main():
    p = argparse.ArgumentParser()
    p.add_argument("prompt")
    p.add_argument("--name", required=True, help="output/ 아래 경로 (확장자 없이)")
    p.add_argument("--model", default="seedream_v5")
    p.add_argument("--size", default="2560x1440")
    p.add_argument("--resume", action="store_true")
    args = p.parse_args()

    out = ROOT / "output" / args.name
    out.parent.mkdir(parents=True, exist_ok=True)
    state_path = out.with_suffix(".state.json")
    state = json.loads(state_path.read_text()) if args.resume and state_path.exists() else {}
    state["_path"] = state_path
    state["prompt"] = args.prompt

    body = {"prompt": args.prompt, "model": args.model, "size": args.size, "output_format": "png", "watermark": False}
    print("텍스트 -> 이미지")
    d = run_step(state, "image_task", out.name, lambda: request("POST", "/generation/text-to-image", body))
    url = d["output"]["generated_image_url"]
    download(url, out.with_suffix(ext_of(url, ".png")))
    print(f"\n완료: {out}")


if __name__ == "__main__":
    main()
