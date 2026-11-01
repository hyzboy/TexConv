# TexConv 回归基线工具
#
# 用法:
#   生成测试输入图:  python regression.py gen-inputs
#   跑基线:          python regression.py run  --exe <TexConv.exe> --out <目录>
#   与基线对比:      python regression.py compare --exe <TexConv.exe> --against <manifest.json>
#
# 判定标准:产物 .Tex2D 的字节数 + sha256 逐字节一致。
# 退出码与 stdout 不参与判定(旧版退出码恒 0、日志格式随 hgl 变化),仅记录供人工检查。
#
# cube/IBL 用例(cube_*)不依赖基线:它们校验的是与基线无关的物理不变量
# (均匀白环境 → 辐照度恒等;单面亮 → 相邻四面按对称性相等)。若 baseline 里
# 已记录同名用例,还会额外比对产物哈希。
#
# 外部工具路径可用 --magick/--res 或环境变量覆盖:
#   TEXCONV_MAGICK     ImageMagick 可执行文件
#   TEXCONV_RES_IMAGE  ULRE 的 res/image 目录(提供 Grid2x2.png / Lena32.tga)

import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
INPUTS_DIR = os.path.join(HERE, "baseline", "inputs")
MAGICK = os.environ.get("TEXCONV_MAGICK") or r"E:\ImageMagick-7.1.2-Q16-HDRI\magick.exe"
RES_IMAGE = os.environ.get("TEXCONV_RES_IMAGE") or r"E:\ULRE\res\image"

# ---------------------------------------------------------------- 测试输入生成

def gen_inputs():
    os.makedirs(INPUTS_DIR, exist_ok=True)

    m = magick

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

    # Intel ISPC 编码器(构建期由 ispc.exe 编译 kernel,需 TexEncIntel.dll 与 exe 同目录):
    # 应产出与 AMD 不同的 BC7 数据
    ("intel_ispc",        [I("rgb8.png"), "/Intel"],             ["rgb8.Tex2D"]),

    # 负例:损坏文件 → 加载失败;无有效产物
    ("neg_corrupt",       [I("corrupt.png")],                    []),

    # 负例:3通道源配 RGBA8 目标 → RGB 创建器拒绝;旧实现会留下 32 字节头文件
    ("neg_rgb8_rgba8tgt", [I("rgb8.png"), "/RGB:RGBA8"],         ["rgb8.Tex2D"]),
]

# 目录模式用例在 run_case 中单独处理

# ---------------------------------------------------------------- cube/IBL 用例
#
# 这些用例不依赖 baseline:校验与实现无关的几何/物理不变量。
#   * 几何:TexCube 必为 view=3 且 6 面 FACE_SIZE²;Tex2D 投影必为 view=1 且 (2*FACE_SIZE)²
#   * 均匀白环境 → 漫反射辐照度与 GGX 预滤波都必须恒为 255（能量守恒）
#   * 单面点亮 → 辐照度/预滤波的相邻四面必须旋转对称（切线基建错轴时这里会明显偏斜）
# 若 baseline 已记录同名用例,compare 还会额外比对产物哈希。

FACE_SIZE = 32
CUBE_FACE_NAMES = ["PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ"]

CUBE_CASES = {
    # 常规 cubemap:6 面渐变 → 6 面 TexCube
    "cube_default": {
        "gradient": True,
        "args": ["/cube", "/RGBA:RGBA8"],
        "out": "cubedef",
        "expect": ["cubedef.TexCube"],
    },
    # IBL:白炉测试
    "cube_ibl_flat": {
        "solid": (255, 255, 255, 255),
        "args": ["/cube", "/RGBA:RGBA8", "/ibl:RGBA8"],
        "out": "cubeflat",
        "expect": ["cubeflat.TexCube",
                   "cubeflat_irradiance.TexCube",
                   "cubeflat_prefilter.TexCube"],
        "invariant": "uniform_white",
    },
    # IBL:单面点亮(+X 面),校验旋转对称性
    "cube_ibl_dir": {
        "faces": [(255, 255, 255, 255)] + [(0, 0, 0, 255)] * 5,
        "args": ["/cube", "/RGBA:RGBA8", "/ibl:RGBA8"],
        "out": "cubedir",
        "expect": ["cubedir.TexCube",
                   "cubedir_irradiance.TexCube",
                   "cubedir_prefilter.TexCube"],
        "invariant": "face_symmetry",
    },
    # 2D 投影:/octa 与 /laea 都必须产出 2*FACE_SIZE 见方的单张 Tex2D
    "cube_octa": {
        "gradient": True,
        "args": ["/cube", "/RGBA:RGBA8", "/octa"],
        "out": "cubeocta",
        "expect": ["cubeocta.Tex2D"],
    },
    "cube_laea": {
        "gradient": True,
        "args": ["/cube", "/RGBA:RGBA8", "/laea"],
        "out": "cubelaea",
        "expect": ["cubelaea.Tex2D"],
    },
    # /octa + /ibl:三个产物都是 Tex2D(投影在释放 6 面之前不得丢掉 IBL 源数据)
    "cube_octa_ibl": {
        "gradient": True,
        "args": ["/cube", "/RGBA:RGBA8", "/octa", "/ibl:RGBA8"],
        "out": "cubeoctaibl",
        "expect": ["cubeoctaibl.Tex2D",
                   "cubeoctaibl_irradiance.Tex2D",
                   "cubeoctaibl_prefilter.Tex2D"],
    },
    # 负例:互斥参数必须直接拒绝,不产出任何文件
    "neg_laea_ibl": {
        "gradient": True,
        "args": ["/cube", "/RGBA:RGBA8", "/laea", "/ibl:RGBA8"],
        "out": "negali",
        "expect": [],
        "exit": 1,
    },
    "neg_octa_laea": {
        "gradient": True,
        "args": ["/cube", "/RGBA:RGBA8", "/octa", "/laea"],
        "out": "negaol",
        "expect": [],
        "exit": 1,
    },
}

def magick(*args):
    """调用 ImageMagick;缺失时给出可操作的报错"""
    exe = MAGICK
    if not exe or not os.path.isfile(exe):
        exe = shutil.which(exe or "magick")
        if not exe:
            print("找不到 ImageMagick。用 --magick 或环境变量 TEXCONV_MAGICK 指定 magick 可执行文件。",
                  file=sys.stderr)
            raise SystemExit(2)
    subprocess.run([exe, *args], check=True, capture_output=True)

def read_tex(path):
    """解析 Tex2D/TexCube 容器头(32 字节),返回 view/尺寸/像素负载"""
    with open(path, "rb") as f:
        head = f.read(32)
        payload = f.read()
    if len(head) < 32 or head[:7] != b"Texture":
        raise ValueError("不是有效的纹理容器: " + path)
    w, h, _ = struct.unpack("<III", head[9:21])
    return head[8], w, h, payload

def tex_face_means(path):
    """按 6 个面切分负载,返回每面的均值(0-255 或 half 解码前的原始字节均值)"""
    view, w, h, payload = read_tex(path)
    n = w * h * 4
    return view, w, h, [sum(payload[i*n:(i+1)*n]) / n for i in range(6)]

def check_cube_geometry(name, got_paths):
    problems = []
    for fn, fp in got_paths.items():
        if not os.path.isfile(fp):
            problems.append("缺失产物 " + fn)
            continue
        view, w, h, _ = read_tex(fp)
        if fn.endswith(".TexCube"):
            want = (3, FACE_SIZE, FACE_SIZE)
        else:
            want = (1, FACE_SIZE * 2, FACE_SIZE * 2)
        if (view, w, h) != want:
            problems.append("%s 容器为 view=%d %dx%d,预期 view=%d %dx%d"
                            % (fn, view, w, h, want[0], want[1], want[2]))
    return problems

def check_uniform_white(got_paths):
    problems = []
    for fn, fp in got_paths.items():
        if not os.path.isfile(fp):
            continue
        _, w, h, payload = read_tex(fp)
        lo, hi = min(payload), max(payload)
        if lo != 255 or hi != 255:
            problems.append("%s 白炉应恒为 255,实际 [%d,%d]" % (fn, lo, hi))
    return problems

def check_face_symmetry(got_paths):
    """单面点亮时,辐照度/预滤波的 4 个相邻面必须等亮,对侧面必须最暗"""
    problems = []
    means = {}
    for fn, fp in got_paths.items():
        if not os.path.isfile(fp) or not fn.endswith(".TexCube"):
            continue
        _, _, _, m = tex_face_means(fp)
        means[fn] = m
    for fn, m in means.items():
        if "_irradiance" not in fn and "_prefilter" not in fn:
            continue
        lit = max(range(6), key=lambda i: m[i])
        sides = [m[i] for i in range(6) if i // 2 != lit // 2]
        opposite = [m[i] for i in range(6) if i // 2 == lit // 2 and i != lit][0]
        side_mean = sum(sides) / len(sides)
        if side_mean <= 0.0:
            problems.append("%s 相邻四面均值为 0" % fn)
            continue
        spread = max(sides) - min(sides)
        if spread > 0.02 * side_mean:
            problems.append("%s 相邻四面不对称:%s (偏差 %.2f > 2%%)"
                            % (fn, [round(x, 2) for x in sides], 100.0 * spread / side_mean))
        if opposite >= min(sides):
            problems.append("%s 对侧面 %.2f 不应亮于相邻面 %s"
                            % (fn, opposite, [round(x, 2) for x in sides]))
    irr = next((m for fn, m in means.items() if "_irradiance" in fn), None)
    pre = next((m for fn, m in means.items() if "_prefilter" in fn), None)
    if irr and pre:
        li = max(range(6), key=lambda i: irr[i])
        lp = max(range(6), key=lambda i: pre[i])
        if li != lp:
            problems.append("辐照度与预滤波的受光面不一致 (%d vs %d)" % (li, lp))
        elif pre[lp] <= irr[li]:
            problems.append("预滤波峰值 %.2f 应高于辐照度峰值 %.2f(波瓣更收束)"
                            % (pre[lp], irr[li]))
    return problems

def prepare_cube_faces(workroot, spec):
    """生成 6 张 FACE_SIZE 见方的 RGBA PNG 面图(渐变/统色/逐面指定)"""
    out = spec["out"]
    faces_dir = os.path.join(workroot, "cube_" + out)
    if os.path.isdir(faces_dir):
        shutil.rmtree(faces_dir)
    os.makedirs(faces_dir)

    paths = []
    for i, face in enumerate(CUBE_FACE_NAMES):
        p = os.path.join(faces_dir, "%s_%s.png" % (out, face))
        if "faces" in spec:
            r, g, b, a = spec["faces"][i]
            src = ["xc:rgba(%d,%d,%d,%.3f)" % (r, g, b, a / 255.0)]
        elif "solid" in spec:
            r, g, b, a = spec["solid"]
            src = ["xc:rgba(%d,%d,%d,%.3f)" % (r, g, b, a / 255.0)]
        else:
            src = ["gradient:red-blue", "-alpha", "set"]
        magick("-size", "%dx%d" % (FACE_SIZE, FACE_SIZE), *src,
               "-define", "png:color-type=6", "-depth", "8", p)
        paths.append(p)
    return faces_dir, paths

def run_cube_case(exe, name, workroot):
    spec = CUBE_CASES[name]
    faces_dir, face_paths = prepare_cube_faces(workroot, spec)

    # 产物写进程 CWD,故在面图目录里跑,/out: 给出确定的基名
    code, log = run_exe(exe, list(spec["args"]) + ["/out:" + spec["out"]] + face_paths, faces_dir)

    got_paths = {fn: os.path.join(faces_dir, fn) for fn in spec["expect"]}
    problems = check_cube_geometry(name, got_paths)
    want_exit = spec.get("exit", 0)
    if code != want_exit:
        problems.append("退出码 %d,预期 %d" % (code, want_exit))
    invariant = spec.get("invariant")
    if invariant == "uniform_white":
        problems += check_uniform_white(got_paths)
    elif invariant == "face_symmetry":
        problems += check_face_symmetry(got_paths)

    outputs = snapshot_and_cleanup(list(got_paths.values()))
    shutil.rmtree(faces_dir, ignore_errors=True)
    return {"exit_code": code, "outputs": outputs, "problems": problems, "log_tail": log[-400:]}


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

def collect_all_names():
    return [c[0] for c in collect_all_cases()] + list(CUBE_CASES)

def cmd_run(exe, outdir):
    os.makedirs(outdir, exist_ok=True)
    manifest = {"exe": exe, "cases": {}}
    workroot = os.path.abspath(os.path.join(outdir, "work"))
    os.makedirs(workroot, exist_ok=True)
    for name, args, expect in collect_all_cases():
        print(f"[run] {name} ...", flush=True)
        manifest["cases"][name] = run_case(exe, name, args, expect, workroot)
    for name in CUBE_CASES:
        print(f"[run] {name} ...", flush=True)
        manifest["cases"][name] = run_cube_case(exe, name, workroot)
        for p in manifest["cases"][name].get("problems", []):
            print("        不变量失败:", p)
    mf = os.path.join(outdir, "manifest.json")
    with open(mf, "w", encoding="utf8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)
    print("manifest written:", mf)
    return 0

def report_case(name, want, got, extra_problems=()):
    """比对 baseline(可选)并附加自校验问题;返回问题条数"""
    problems = list(extra_problems)
    w_out, g_out = (want or {}).get("outputs", {}), got["outputs"]
    if want is None:
        # 无 baseline 时只做自校验(不变量),不与历史产物比对
        w_out = dict(g_out)
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
        print(f"[FAIL] {name}")
        for p in problems:
            print("        ", p)
        print("         old exit={0} new exit={1}".format(
            (want or {}).get("exit_code"), got["exit_code"]))
        return len(problems)
    note = "" if not want or want.get("exit_code") == got["exit_code"] \
        else f" (exit_code {want.get('exit_code')}->{got['exit_code']})"
    print(f"[PASS] {name}{note}")
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
        if report_case(name, want, got):
            failed += 1

    # cube/IBL 用例:cold baseline 里没有,故缺失时退化为自校验
    for name in CUBE_CASES:
        got = run_cube_case(exe, name, workroot)
        if report_case(name, base["cases"].get(name), got, got.get("problems", ())):
            failed += 1

    total = len(collect_all_names())
    print(f"\n{total - failed}/{total} passed")
    return 1 if failed else 0

def main():
    global MAGICK, RES_IMAGE
    ap = argparse.ArgumentParser()
    ap.add_argument("--magick", help="ImageMagick 可执行文件(默认读 TEXCONV_MAGICK)")
    ap.add_argument("--res", help="输入图分辨率 WxH(默认读 TEXCONV_RES_IMAGE)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("gen-inputs")
    p = sub.add_parser("run");     p.add_argument("--exe", required=True); p.add_argument("--out", required=True)
    p = sub.add_parser("compare"); p.add_argument("--exe", required=True); p.add_argument("--against", required=True)
    a = ap.parse_args()

    if a.magick:
        MAGICK = a.magick
    if a.res:
        RES_IMAGE = a.res

    if a.cmd == "gen-inputs":
        return gen_inputs()
    if a.cmd == "run":
        return cmd_run(a.exe, a.out)
    if a.cmd == "compare":
        return cmd_compare(a.exe, a.against)
    return 2

if __name__ == "__main__":
    sys.exit(main())

