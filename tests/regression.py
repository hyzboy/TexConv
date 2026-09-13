# TexConv 回归基线工具
#
# 用法:
#   生成测试输入图:  python regression.py gen-inputs
#   跑基线:          python regression.py run  --exe <TexConv.exe> --out <目录>
#   与基线对比:      python regression.py compare --exe <TexConv.exe> --against <manifest.json>
#
# 判定标准:产物 .Tex2D 的字节数 + sha256 逐字节一致。
# 退出码与 stdout 不参与判定(旧版退出码恒 0、日志格式随 hgl 变化),仅记录供人工检查。

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
INPUTS_DIR = os.path.join(HERE, "baseline", "inputs")
MAGICK = r"E:\ImageMagick-7.1.2-Q16-HDRI\magick.exe"
RES_IMAGE = r"E:\ULRE\res\image"

# ---------------------------------------------------------------- 测试输入生成

def gen_inputs():
    os.makedirs(INPUTS_DIR, exist_ok=True)

    def m(*args):
        subprocess.run([MAGICK, *args], check=True)

    # 1 通道灰度
    m("-size", "64x64", "gradient:black-white", "-colorspace", "Gray",
      "-depth", "8", os.path.join(INPUTS_DIR, "g8.png"))
    # 2 通道 灰度+alpha
    m("-size", "64x64", "gradient:black-white", "-colorspace", "Gray",
      "-alpha", "set", "-channel", "A", "-evaluate", "set", "50%", "+channel",
      os.path.join(INPUTS_DIR, "ga8.png"))
    # 3 通道
    m("-size", "64x64", "gradient:red-blue", "-depth", "8",
      os.path.join(INPUTS_DIR, "rgb8.png"))
    # 4 通道(alpha 渐变)
    m("-size", "64x64", "gradient:red-blue",
      "(", "-size", "64x64", "gradient:white-black", ")",
      "-alpha", "off", "-compose", "CopyOpacity", "-composite",
      os.path.join(INPUTS_DIR, "rgba8.png"))
    # 4 通道 TGA
    m(os.path.join(INPUTS_DIR, "rgba8.png"), os.path.join(INPUTS_DIR, "rgba8.tga"))
    # 3 通道 16bit
    m("-size", "64x64", "gradient:red-blue", "-depth", "16",
      os.path.join(INPUTS_DIR, "rgb16.png"))
    # 3 通道 float EXR / HDR
    m("-size", "32x32", "radial-gradient:white-black", "-depth", "32",
      "-define", "quantum:format=floating-point",
      os.path.join(INPUTS_DIR, "rgbf32.exr"))
    m(os.path.join(INPUTS_DIR, "rgbf32.exr"), os.path.join(INPUTS_DIR, "rgb.hdr"))
    # 损坏文件(负例)
    with open(os.path.join(INPUTS_DIR, "corrupt.png"), "wb") as f:
        f.write(b"this is not an image at all" * 16)

    print("inputs generated at", INPUTS_DIR)

# ---------------------------------------------------------------- 用例定义

I = lambda name: os.path.join(INPUTS_DIR, name)
R = lambda name: os.path.join(RES_IMAGE, name)

# 每项: (用例名, [输入路径, 参数...], 期望产物文件名列表)
# 运行时重排为 [参数..., 输入路径] —— main.cpp 约定输入路径必须是最后一个参数。
# 期望产物为空表示该用例应失败、不产出有效纹理。
CASES = [
    # 默认槽位(1ch→BC4, 2ch→BC5, 3ch/4ch→BC7)
    ("g8_default",      [I("g8.png")],          ["g8.Tex2D"]),
    ("ga8_default",     [I("ga8.png")],         ["ga8.Tex2D"]),
    ("rgb8_default",    [I("rgb8.png")],        ["rgb8.Tex2D"]),
    ("rgba8png_default",[I("rgba8.png")],       ["rgba8.Tex2D"]),
    ("rgba8tga_default",[I("rgba8.tga")],       ["rgba8.Tex2D"]),
    ("rgb16_default",   [I("rgb16.png")],       ["rgb16.Tex2D"]),
    ("exr_default",     [I("rgbf32.exr")],      ["rgbf32.Tex2D"]),
    ("hdr_default",     [I("rgb.hdr")],         ["rgb.Tex2D"]),
    ("grid2x2_default", [R("Grid2x2.png")],     ["Grid2x2.Tex2D"]),
    ("lena32_default",  [R("Lena32.tga")],      ["Lena32.Tex2D"]),

    # R 槽(1通道源)
    ("g8_r8",    [I("g8.png"), "/R:R8"],    ["g8.Tex2D"]),
    ("g8_r16f",  [I("g8.png"), "/R:R16F"],  ["g8.Tex2D"]),
    ("g8_r32f",  [I("g8.png"), "/R:R32F"],  ["g8.Tex2D"]),

    # RG 槽(2通道源)
    ("ga8_rg8",    [I("ga8.png"), "/RG:RG8"],    ["ga8.Tex2D"]),
    ("ga8_rg16f",  [I("ga8.png"), "/RG:RG16F"],  ["ga8.Tex2D"]),

    # RGB 槽(3通道源):压缩 + 未压缩
    ("rgb8_bc1rgba", [I("rgb8.png"), "/RGB:BC1RGBA"], ["rgb8.Tex2D"]),
    ("rgb8_bc4",     [I("rgb8.png"), "/RGB:BC4"],     ["rgb8.Tex2D"]),
    ("rgb8_rgb32f",  [I("rgb8.png"), "/RGB:RGB32F"],  ["rgb8.Tex2D"]),
    ("rgb8_rgb565",  [I("rgb8.png"), "/RGB:RGB565"],  ["rgb8.Tex2D"]),
    ("rgb8_b10gr11", [I("rgb8.png"), "/RGB:B10GR11UF"],["rgb8.Tex2D"]),
    ("rgb8_bc7",     [I("rgb8.png"), "/RGB:BC7"],     ["rgb8.Tex2D"]),

    # RGBA 槽(4通道源)
    ("rgba8_rgba8",    [I("rgba8.png"), "/RGBA:RGBA8"],    ["rgba8.Tex2D"]),
    ("rgba8_rgba16f",  [I("rgba8.png"), "/RGBA:RGBA16F"],  ["rgba8.Tex2D"]),
    ("rgba8_rgba4",    [I("rgba8.png"), "/RGBA:RGBA4"],    ["rgba8.Tex2D"]),
    ("rgba8_bgra4",    [I("rgba8.png"), "/RGBA:BGRA4"],    ["rgba8.Tex2D"]),
    ("rgba8_a1rgb5",   [I("rgba8.png"), "/RGBA:A1RGB5"],   ["rgba8.Tex2D"]),
    ("rgba8_a2bgr10",  [I("rgba8.png"), "/RGBA:A2BGR10"],  ["rgba8.Tex2D"]),
    ("rgba8_bc3",      [I("rgba8.png"), "/RGBA:BC3"],      ["rgba8.Tex2D"]),

    # mipmap
    ("rgb8_mip",          [I("rgb8.png"), "/mip"],               ["rgb8.Tex2D"]),
    ("rgba8_rgba8_mip",   [I("rgba8.png"), "/RGBA:RGBA8", "/mip"],["rgba8.Tex2D"]),
    ("rgb8_rgb32f_mip",   [I("rgb8.png"), "/RGB:RGB32F", "/mip"], ["rgb8.Tex2D"]),
    ("g8_r8_mip",         [I("g8.png"), "/R:R8", "/mip"],         ["g8.Tex2D"]),

    # 其它开关
    ("rgb8_out",          [I("rgb8.png"), "/out:custom_name"],   ["custom_name.Tex2D"]),
    ("rgb8_colorkey",     [I("rgb8.png"), "/ColorKey:FF00FF"],   ["rgb8.Tex2D"]),
    ("rgb8_gray",         [I("rgb8.png"), "/gray"],              ["rgb8.Tex2D"]),
    ("rgba8_discard",     [I("rgba8.png"), "/discard_alpha"],    ["rgba8.Tex2D"]),
    ("rgb8_normal",       [I("rgb8.png"), "/normal"],            ["rgb8.Tex2D"]),
    ("rgb8_amd",          [I("rgb8.png"), "/AMD"],               ["rgb8.Tex2D"]),
    ("ga8_gray",          [I("ga8.png"), "/gray", "/discard_alpha"], ["ga8.Tex2D"]),

    # Intel ISPC 编码器(根构建 kernel*.obj 存在,ISPC 可用):应产出与 AMD 不同的 BC7 数据
    ("intel_ispc",        [I("rgb8.png"), "/Intel"],             ["rgb8.Tex2D"]),

    # 负例:损坏文件 → 加载失败;无有效产物
    ("neg_corrupt",       [I("corrupt.png")],                    []),

    # 负例:3通道源配 RGBA8 目标 → RGB 创建器拒绝;旧实现会留下 32 字节头文件
    ("neg_rgb8_rgba8tgt", [I("rgb8.png"), "/RGB:RGBA8"],         ["rgb8.Tex2D"]),
]

# 目录模式用例在 run_case 中单独处理

def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()

def run_exe(exe, args, cwd):
    env = dict(os.environ)
    env["PATH"] = os.path.dirname(exe) + os.pathsep + env.get("PATH", "")
    p = subprocess.run([exe, *args], cwd=cwd, env=env,
                       capture_output=True, text=True, errors="replace", timeout=300)
    return p.returncode, (p.stdout or "") + (p.stderr or "")

def expected_out_paths(args, outdir):
    """无 /out: 时产物写在输入文件同目录;有 /out: 时写在进程 CWD。
    args[0] 为输入,其余为选项。"""
    options = args[1:]
    if any(a.startswith("/out:") or a == "/out" for a in options):
        base = "custom_name"
        for a in options:
            if a.startswith("/out:"):
                base = a[5:]
        return [os.path.join(outdir, base + ".Tex2D")]
    input_path = args[0]
    stem = os.path.splitext(os.path.basename(input_path))[0]
    return [os.path.join(os.path.dirname(os.path.abspath(input_path)), stem + ".Tex2D")]

def snapshot_and_cleanup(paths, keep=False):
    """对期望产物哈希;除非 keep,否则删除文件(保持 inputs/res 源目录干净)"""
    result = {}
    for fp in paths:
        if os.path.isfile(fp):
            result[os.path.basename(fp)] = {"size": os.path.getsize(fp),
                                            "sha256": sha256_of(fp)}
            if not keep:
                os.remove(fp)
    return result

def prepare_dir_case(workroot):
    """目录模式:把 3 张图拷进 scratch(含子目录),并预置一个 .Tex2D 文件验证跳过逻辑"""
    src = os.path.join(workroot, "srctree")
    if os.path.isdir(src):
        shutil.rmtree(src)
    os.makedirs(os.path.join(src, "sub"))
    shutil.copy(I("rgb8.png"), src)
    shutil.copy(I("rgba8.png"), src)
    shutil.copy(I("g8.png"), os.path.join(src, "sub"))
    with open(os.path.join(src, "preset.Tex2D"), "wb") as f:
        f.write(b"PRESET")
    return src

def run_case(exe, name, args, expect_names, workroot):
    outdir = os.path.join(workroot, name)
    os.makedirs(outdir, exist_ok=True)

    if name == "dir_mode_s":
        src = prepare_dir_case(workroot)
        code, log = run_exe(exe, ["/s", src], outdir)
        # 产物写在源目录里
        result = snapshot_and_cleanup([
            os.path.join(src, "rgb8.Tex2D"),
            os.path.join(src, "rgba8.Tex2D"),
            os.path.join(src, "sub", "g8.Tex2D"),
        ])
        # preset.Tex2D 应原样保留(枚举跳过 .Tex2D)
        preset = os.path.join(src, "preset.Tex2D")
        result["preset_untouched"] = (
            {"size": 6} if os.path.isfile(preset) and os.path.getsize(preset) == 6 else {"size": -1})
        return {"exit_code": code, "outputs": result, "log_tail": log[-400:]}

    code, log = run_exe(exe, list(args[1:]) + [args[0]], outdir)
    return {"exit_code": code,
            "outputs": snapshot_and_cleanup(expected_out_paths(args, outdir)),
            "log_tail": log[-400:]}

# ---------------------------------------------------------------- 命令入口

def collect_all_cases():
    cases = list(CASES)
    cases.append(("dir_mode_s", [], ["rgb8.Tex2D", "rgba8.Tex2D", "sub_g8.Tex2D"]))
    return cases

def cmd_run(exe, outdir):
    os.makedirs(outdir, exist_ok=True)
    manifest = {"exe": exe, "cases": {}}
    workroot = os.path.abspath(os.path.join(outdir, "work"))
    os.makedirs(workroot, exist_ok=True)
    for name, args, expect in collect_all_cases():
        print(f"[run] {name} ...", flush=True)
        manifest["cases"][name] = run_case(exe, name, args, expect, workroot)
    mf = os.path.join(outdir, "manifest.json")
    with open(mf, "w", encoding="utf8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=1)
    print("manifest written:", mf)
    return 0

def cmd_compare(exe, against):
    with open(against, "r", encoding="utf8") as f:
        base = json.load(f)
    workroot = os.path.join(HERE, "_compare_work")
    os.makedirs(workroot, exist_ok=True)
    failed = 0
    for name, args, expect in collect_all_cases():
        want = base["cases"].get(name)
        if want is None:
            print(f"[MISSING-BASELINE] {name}")
            failed += 1
            continue
        got = run_case(exe, name, args, expect, workroot)

        problems = []
        w_out, g_out = want.get("outputs", {}), got["outputs"]
        for k in sorted(set(w_out) | set(g_out)):
            w, g = w_out.get(k), g_out.get(k)
            if w is None:
                problems.append(f"多余产物 {k} ({g['size']}B)")
            elif g is None:
                problems.append(f"缺失产物 {k}")
            elif w["size"] != g["size"]:
                problems.append(f"{k} 尺寸 {w['size']} != {g['size']}")
            elif "sha256" in w and w["sha256"] != g.get("sha256"):
                problems.append(f"{k} 内容 sha256 不一致")

        if problems:
            failed += 1
            print(f"[FAIL] {name}")
            for p in problems:
                print("        ", p)
            print("         old exit={0} new exit={1}".format(
                want.get("exit_code"), got["exit_code"]))
        else:
            note = "" if want.get("exit_code") == got["exit_code"] \
                else f" (exit_code {want.get('exit_code')}->{got['exit_code']})"
            print(f"[PASS] {name}{note}")
    total = len(collect_all_cases())
    print(f"\n{total - failed}/{total} passed")
    return 1 if failed else 0

def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("gen-inputs")
    p = sub.add_parser("run");     p.add_argument("--exe", required=True); p.add_argument("--out", required=True)
    p = sub.add_parser("compare"); p.add_argument("--exe", required=True); p.add_argument("--against", required=True)
    a = ap.parse_args()

    if a.cmd == "gen-inputs":
        return gen_inputs()
    if a.cmd == "run":
        return cmd_run(a.exe, a.out)
    if a.cmd == "compare":
        return cmd_compare(a.exe, a.against)
    return 2

if __name__ == "__main__":
    sys.exit(main())
