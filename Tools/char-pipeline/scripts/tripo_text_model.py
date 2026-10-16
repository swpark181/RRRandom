"""Text prompt -> textured 3D model via Tripo API V3 (props: no rigging).

Usage:
    python tripo_text_model.py "a stylized cartoon rifle ..." --name rifle
    python tripo_text_model.py "..." --name rifle --resume      # reuse the finished task from state.json

Shares the API helpers (key lookup, retries, task polling, downloads) with tripo_pipeline.py.
Requires env var TRIPO_API_KEY. Standard library only (runs on Unreal's bundled Python too).
Writes output/<name>/<name>_model.glb (+ _preview.webp) and remembers the task id in state.json,
so a failed download can resume without spending credits again.
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tripo_pipeline import ROOT, download, ext_of, model_urls, request, run_step  # noqa: E402


def main():
    p = argparse.ArgumentParser()
    p.add_argument("prompt")
    p.add_argument("--name", required=True, help="출력 폴더 이름")
    p.add_argument("--model-version", default="v3.1-20260211")
    p.add_argument("--face-limit", type=int, default=6000, help="소품은 작게 (Tripo 상한 20000)")
    p.add_argument("--no-smart-low-poly", action="store_true", help="게임용 저폴리 토폴로지 생성 끄기 (기본: 켬)")
    p.add_argument("--texture-quality", default="detailed", choices=["standard", "detailed", "extreme"])
    p.add_argument("--resume", action="store_true")
    args = p.parse_args()

    out = ROOT / "output" / args.name
    out.mkdir(parents=True, exist_ok=True)
    state_path = out / "state.json"
    state = json.loads(state_path.read_text()) if args.resume and state_path.exists() else {}
    state["_path"] = state_path
    state["prompt"] = args.prompt

    gen = {
        "prompt": args.prompt,
        "model_version": args.model_version,
        "face_limit": args.face_limit,
        "texture": True,
        "pbr": True,
        "texture_quality": args.texture_quality,
    }
    if not args.model_version.startswith("P1"):  # P1 모델은 smart_low_poly 필드 자체를 거부
        gen["smart_low_poly"] = not args.no_smart_low_poly

    print("텍스트 -> 3D 모델")
    d = run_step(state, "model_task", "model", lambda: request("POST", "/generation/text-to-model", gen))
    for i, url in enumerate(model_urls(d["output"])):
        download(url, out / f"{args.name}_model{'' if i == 0 else i}{ext_of(url, '.glb')}")
    if d["output"].get("rendered_image_url"):
        download(d["output"]["rendered_image_url"], out / f"{args.name}_preview.webp")
    print(f"\n완료: {out}")


if __name__ == "__main__":
    main()
