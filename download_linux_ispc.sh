#!/usr/bin/env bash
#
# 下载并解包 ISPC（Intel SPMD Program Compiler，Linux x86-64）到
#     ISPCTextureCompressor/ISPC/linux/
#
# 为什么要它：ISPCTextureCompressor 的 kernel*.ispc 必须由 ispc 编译成 kernel*.obj，
# 而上游仓库既不含预编译产物、也不含编译器（ISPC/win、ISPC/linux 里的内容都被 .gitignore
# 忽略），所以缺它时 TexConv 的 Intel 编码器只能关掉（见 TexConv/CMakeLists.txt 的自动探测）。
# Windows 侧对称：download_win_ispc.bat（vcxproj 调 ..\ISPC\win\ispc.exe）。
#
# 拿到之后按上游 Makefile.linux 编译 kernel（该文件里 ISPC ?= ISPC/linux/ispc）：
#     cd ISPCTextureCompressor && ISPC=ISPC/linux/ispc make -f Makefile.linux
#
# 用法：
#     ./download_linux_ispc.sh                     # 下载默认的 trunk 构建（x86-64）
#     ./download_linux_ispc.sh <url>               # 换其它地址（例如 aarch64 版）
#     ./download_linux_ispc.sh --keep              # 保留下载的压缩包
#     ISPC_DEST_DIR=/tmp/ispc ./download_linux_ispc.sh    # 装到别处
#
# 选项：
#     --keep        保留下载的压缩包（默认装完即删）
#     -h | --help   显示帮助
#
# 说明：
#   * 包内一般是 bin/ispc 加同目录的共享库，故整个 bin/ 目录一起安装，只拷可执行文件
#     可能起不来（Windows 版就是 ispc.exe + ispc.dll + ispcrt*.dll）。
#   * 压缩包（trunk 构建约 126 MB）下载到目标目录下的 <包名>.part 并支持断点续传，
#     网络中断时重跑本脚本即接着下；目标目录或脚本旁已有同名压缩包则直接复用。
#
set -euo pipefail

DEFAULT_URL="https://github.com/ispc/ispc/releases/download/trunk-artifacts/ispc-trunk-linux.tar.gz"
MAX_RETRY=5

URL="$DEFAULT_URL"
KEEP=0

# ---------------------------------------------------------------------------
# 参数解析
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --keep)
            KEEP=1
            ;;
        -h|--help)
            # 打印文件头部的注释块（从第 2 行到第一条非注释行）
            awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "$0"
            exit 0
            ;;
        -*)
            echo "未知选项：$1（用 -h 查看用法）" >&2
            exit 2
            ;;
        *)
            URL="$1"
            ;;
    esac
    shift
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---------------------------------------------------------------------------
# MSYS/Cygwin（Git for Windows 的 bash）下 curl/wget/tar 常是原生 Windows 程序，
# 看不懂 /tmp/... 这类 MSYS 路径：症状是 `curl: (23) client returned ERROR on write`
# 或 "The system cannot find the file specified"。凡是交给它们的路径都先转成
# `C:/...` 形式（两边都认）；纯 Linux 上没有 cygpath，函数原样返回。
# ---------------------------------------------------------------------------
native_path()
{
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s\n' "$1"
    fi
}

# native_path 的反向：把 C:\... 转成 /c/... 形式。
# GNU tar 只认后者（把 `C:/...` 当成"远程主机:路径"报 Cannot connect to C:），
# 而 Windows 自带的 bsdtar 只认前者 —— 所以下面两个函数都把两种形态依次试一遍。
msys_path()
{
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$1"
    else
        printf '%s\n' "$1"
    fi
}

# 校验压缩包：tar 能列出内容就算完整。截断/损坏的包必须在这里被拦下（丢弃重下），
# 否则错误会拖到解包那步才冒出来（实测踩过：tar 流被当成可执行文件装了进来）。
check_archive()
{
    tar -tzf "$1" >/dev/null 2>&1 && return 0
    tar -tzf "$(msys_path "$1")" >/dev/null 2>&1 && return 0
    tar -tzf "$(native_path "$1")" >/dev/null 2>&1
}

# 解包（同样两种路径形态依次试），失败时把 tar 的报错打出来便于定位
untar()
{
    local archive="$1" dest="$2" out=""

    out="$(tar -xzf "${archive}" -C "${dest}" 2>&1)" && return 0
    out="$(tar -xzf "$(msys_path "${archive}")" -C "$(msys_path "${dest}")" 2>&1)" && return 0
    out="$(tar -xzf "$(native_path "${archive}")" -C "$(native_path "${dest}")" 2>&1)" && return 0

    printf '%s\n' "${out}" >&2
    return 1
}

# 判断一个文件是不是 tar 流：POSIX tar 的魔数 "ustar" 在偏移 257 处
is_tar_stream()
{
    [ "$(dd if="$1" bs=1 skip=257 count=5 2>/dev/null)" = "ustar" ]
}

# 目标目录：默认 <脚本目录>/ISPCTextureCompressor/ISPC/linux（可用环境变量覆盖）
DEST_DIR="${ISPC_DEST_DIR:-${SCRIPT_DIR}/ISPCTextureCompressor/ISPC/linux}"
DEST_BIN="${DEST_DIR}/ispc"

ARCHIVE_NAME="$(basename "${URL%%\?*}")"
ARCHIVE="${DEST_DIR}/${ARCHIVE_NAME}"
PART="${ARCHIVE}.part"

mkdir -p "${DEST_DIR}"

echo "下载地址 : ${URL}"
echo "目标目录 : ${DEST_DIR}"

# ---------------------------------------------------------------------------
# 下载（curl 优先，退回 wget）；支持断点续传 + 自动重试
#   放在目标目录而不是临时目录：中断后重跑本脚本可直接续传
# ---------------------------------------------------------------------------
download()
{
    local attempt=1
    local resume="-C -"      # 断点续传；服务端不支持续传(33)时退化为整包重下
    local rc=0

    while [ "${attempt}" -le "${MAX_RETRY}" ]; do
        if command -v curl >/dev/null 2>&1; then
            # -C - 续传（文件不存在则从 0 开始）；-f 让 HTTP 错误码变成非 0 退出；
            # --speed-limit/--speed-time：30 秒内平均速度低于 1KB/s 就判超时退出。
            # 没有这一对参数时，链路会被"挂住"（socket 不关也不传数据），curl 会一直等，
            # 重试逻辑永远不触发——这个坑实测踩到过。
            rc=0
            curl -fL ${resume} --connect-timeout 20 --retry 3 --retry-delay 2 \
                    --speed-limit 1024 --speed-time 30 \
                    -o "$(native_path "${PART}")" "${URL}" || rc=$?
        elif command -v wget >/dev/null 2>&1; then
            rc=0
            wget -c --read-timeout=30 --tries=3 -O "$(native_path "${PART}")" "${URL}" || rc=$?
        else
            echo "错误：既没有 curl 也没有 wget，无法下载。" >&2
            return 1
        fi

        if [ "${rc}" -eq 0 ]; then
            # 下完还要验完整性：截断/损坏的包必须在这里被拦下（丢掉重下），
            # 而不是拖到解包那步才报"不是 tar 包"
            if check_archive "${PART}"; then
                return 0
            fi
            echo "下载到的文件不完整（tar 列表校验失败），重新下载..." >&2
            rm -f "${PART}"
            resume=""
        elif [ "${rc}" -eq 33 ]; then
            # CURLE_RANGE_ERROR：服务端不支持续传。绝不能拿半截文件当完整包，
            # 删掉重下（后续尝试不再带 -C）。
            echo "（服务端不支持断点续传，改为整包重下）"
            rm -f "${PART}"
            resume=""
        fi

        echo "下载中断（第 ${attempt}/${MAX_RETRY} 次，退出码 ${rc}），3 秒后重试..." >&2
        sleep 3
        attempt=$((attempt + 1))
    done

    echo "错误：下载失败（已重试 ${MAX_RETRY} 次）。已下的部分保留在：${PART}" >&2
    echo "      网络恢复后重跑本脚本即可续传。" >&2
    return 1
}

# 已有压缩包就直接复用（例如手工放好的），避免重复下载；但先验完整性
if [ -s "${ARCHIVE}" ] && check_archive "${ARCHIVE}"; then
    echo "复用已有压缩包：${ARCHIVE}"
elif [ -s "${SCRIPT_DIR}/${ARCHIVE_NAME}" ] && check_archive "${SCRIPT_DIR}/${ARCHIVE_NAME}"; then
    ARCHIVE="${SCRIPT_DIR}/${ARCHIVE_NAME}"
    PART="${ARCHIVE}.part"
    echo "复用脚本旁的压缩包：${ARCHIVE}"
else
    echo "开始下载（支持续传，中断后重跑即可）..."
    download
fi

if [ -s "${PART}" ]; then
    mv -f "${PART}" "${ARCHIVE}"
    echo "下载完成：$(du -h "${ARCHIVE}" | cut -f1)"
fi

if [ ! -s "${ARCHIVE}" ]; then
    echo "错误：压缩包不存在或为空：${ARCHIVE}" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# 解包并定位 ispc 可执行文件
#   上游包的内部结构变过（<目录>/bin/ispc 或直接就是 ispc），故解包后用 find 定位；
#   若根本不是 tar 包（裸 gzip 的单个可执行文件），退回 gunzip。
# ---------------------------------------------------------------------------
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

EXTRACT_DIR="${TMP_DIR}/x"
mkdir -p "${EXTRACT_DIR}"

if ! untar "${ARCHIVE}" "${EXTRACT_DIR}"; then
    echo "tar 解包失败，改按裸 gzip 处理..." >&2

    RAW="${TMP_DIR}/raw"
    if ! gzip -dc "$(native_path "${ARCHIVE}")" > "${RAW}" 2>/dev/null; then
        echo "错误：既不是 tar.gz 也不是 gzip 文件。" >&2
        exit 1
    fi

    mkdir -p "${EXTRACT_DIR}"
    if is_tar_stream "${RAW}"; then
        # 扩展名不是 .tar.gz、内容却是 tar：按 tar 解，别把整条 tar 流当二进制装进去
        echo "（gzip 流内容其实是 tar，按 tar 解包）"
        tar -xf "${RAW}" -C "${EXTRACT_DIR}" || {
            echo "错误：无法解开 tar 流。" >&2
            exit 1
        }
    else
        mv -f "${RAW}" "${EXTRACT_DIR}/ispc"
    fi
fi

FOUND_BIN="$(find "${EXTRACT_DIR}" -type f -name 'ispc' -print -quit)"

if [ -z "${FOUND_BIN}" ]; then
    echo "错误：解包后没找到名为 ispc 的文件，包内容如下：" >&2
    find "${EXTRACT_DIR}" -maxdepth 2 >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# 安装到目标路径
#   整个目录一起拷：包里的 ispc 通常与共享库同处 bin/，只拷可执行文件可能起不来
# ---------------------------------------------------------------------------
FOUND_DIR="$(dirname "${FOUND_BIN}")"
echo "从 ${FOUND_DIR} 安装..."
cp -f "${FOUND_DIR}"/* "${DEST_DIR}/"
chmod +x "${DEST_BIN}"

# 装上的是不是真的可执行文件？防止把 tar 流 / 其它内容当 ispc 装进去
MAGIC="$(head -c 4 "${DEST_BIN}" | od -An -tx1 | tr -d ' \n')"
case "${MAGIC}" in
    7f454c46) echo "已确认：ELF 可执行文件" ;;
    4d5a*)    echo "已确认：PE 可执行文件" ;;
    *)
        echo "错误：装出来的 ${DEST_BIN} 不是可执行文件（魔数 ${MAGIC}）。" >&2
        echo "      压缩包可能不是 ispc 发行包，请检查地址。" >&2
        exit 1
        ;;
esac

echo
echo "安装完成：${DEST_BIN}"
ls -l "${DEST_DIR}"

# 复用来的压缩包不动；自己下的默认删掉（--keep 保留）
if [ "${KEEP}" -eq 0 ] && [ -s "${DEST_DIR}/${ARCHIVE_NAME}" ]; then
    rm -f "${DEST_DIR}/${ARCHIVE_NAME}"
fi

# ---------------------------------------------------------------------------
# 校验：能在本机跑就跑 --version；跨平台（例如在 Windows 上预取 Linux 版）时只提示
# ---------------------------------------------------------------------------
if "${DEST_BIN}" --version >/dev/null 2>&1; then
    echo "版本： $("${DEST_BIN}" --version 2>&1 | head -n 1)"
    echo "下一步：cd ISPCTextureCompressor && ISPC=ISPC/linux/ispc make -f Makefile.linux"
else
    echo "提示：本机无法执行该二进制（跨平台预取属正常现象），已就位即可。"
fi
