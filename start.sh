#!/bin/bash

# 彩色输出函数
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检测系统类型
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
        OS_NAME=$NAME
    elif type lsb_release >/dev/null 2>&1; then
        OS=$(lsb_release -si | tr '[:upper:]' '[:lower:]')
        OS_NAME=$(lsb_release -sd)
    else
        print_error "无法检测操作系统类型"
        exit 1
    fi

    print_info "检测到操作系统: $OS_NAME"
}

# 安装依赖
install_dependencies() {
    print_info "检查并安装必要的依赖..."

    case $OS in
        ubuntu|debian|linuxmint)
            # Debian系
            PKG_MANAGER="apt-get"
            PKG_UPDATE="sudo $PKG_MANAGER update"
            PKG_INSTALL="sudo $PKG_MANAGER install -y"
            PKGS="g++ pkg-config libopencv-dev mpv"
            ;;
        fedora|centos|rhel|rocky)
            # RedHat系
            if command -v dnf >/dev/null 2>&1; then
                PKG_MANAGER="dnf"
            else
                PKG_MANAGER="yum"
            fi
            PKG_UPDATE="sudo $PKG_MANAGER update -y"
            PKG_INSTALL="sudo $PKG_MANAGER install -y"
            PKGS="gcc-c++ pkg-config opencv-devel mpv"
            ;;
        arch|manjaro)
            # Arch系
            PKG_MANAGER="pacman"
            PKG_UPDATE="sudo $PKG_MANAGER -Sy"
            PKG_INSTALL="sudo $PKG_MANAGER -S --noconfirm"
            PKGS="gcc pkg-config opencv mpv"
            ;;
        *)
            print_error "不支持的Linux发行版: $OS"
            print_info "请手动安装以下依赖: g++, pkg-config, opencv, mpv"
            exit 1
            ;;
    esac

    # 更新包管理器
    print_info "更新包列表..."
    $PKG_UPDATE

    # 安装依赖包
    print_info "安装依赖: $PKGS"
    $PKG_INSTALL $PKGS

    if [ $? -eq 0 ]; then
        print_success "依赖安装完成"
    else
        print_error "依赖安装失败"
        exit 1
    fi
}

# 检查命令是否存在
check_command() {
    if ! command -v $1 &> /dev/null; then
        print_error "$1 未安装"
        return 1
    else
        print_info "$1 已安装: $(command -v $1)"
        return 0
    fi
}

# 检查OpenCV开发库
check_opencv() {
    if ! pkg-config --exists opencv4; then
        if ! pkg-config --exists opencv; then
            print_error "OpenCV开发库未找到"
            return 1
        else
            print_info "找到OpenCV库 (opencv)"
            return 0
        fi
    else
        print_info "找到OpenCV库 (opencv4)"
        return 0
    fi
}

# 主函数
main() {
    print_info "开始执行ASCII视频转换脚本"
    echo "========================================"

    # 检测操作系统
    detect_os

    # 检查必要命令
    print_info "检查系统依赖..."

    NEED_INSTALL=false
    if ! check_command g++; then
        NEED_INSTALL=true
    fi

    if ! check_command pkg-config; then
        NEED_INSTALL=true
    fi

    if ! check_opencv; then
        NEED_INSTALL=true
    fi

    if ! check_command mpv; then
        print_warning "mpv未安装，将自动安装"
        NEED_INSTALL=true
    fi

    # 如果需要安装依赖
    if [ "$NEED_INSTALL" = true ]; then
        install_dependencies
    else
        print_success "所有依赖已满足"
    fi

    # 再次验证OpenCV
    if ! check_opencv; then
        print_error "OpenCV开发库安装失败，请手动安装"
        exit 1
    fi

    echo "========================================"
    print_info "开始编译程序..."

    # 第一步：编译C++程序
    print_info "执行编译命令: g++ -std=c++11 -O2 -o miku miku.cpp \`pkg-config --cflags --libs opencv4\`"

    # 尝试使用opencv4，如果失败则尝试opencv
    if pkg-config --exists opencv4; then
        g++ -std=c++11 -O2 -o miku miku.cpp `pkg-config --cflags --libs opencv4`
    else
        print_warning "未找到opencv4，尝试使用opencv"
        g++ -std=c++11 -O2 -o miku miku.cpp `pkg-config --cflags --libs opencv`
    fi

    # 检查编译是否成功
    if [ $? -eq 0 ] && [ -f "miku" ]; then
        print_success "编译成功！生成可执行文件: miku"
    else
        print_error "编译失败！"
        print_info "请检查:"
        print_info "1. miku.cpp 文件是否存在"
        print_info "2. OpenCV开发库是否正确安装"
        print_info "3. 编译错误信息"
        exit 1
    fi

    echo "========================================"
    print_info "运行ASCII视频转换..."

    # 第二步：运行视频转换程序
    if [ -f "miku.mp4" ]; then
        print_info "执行转换命令: ./miku miku.mp4 ascii.mp4 150 1.5"
        ./miku miku.mp4 ascii.mp4 150 1.5

        if [ $? -eq 0 ] && [ -f "ascii.mp4" ]; then
            print_success "视频转换完成！"
        else
            print_error "视频转换失败！"
            exit 1
        fi
    else
        print_error "输入视频文件 miku.mp4 不存在"
        print_info "请确保 miku.mp4 文件在当前目录"
        exit 1
    fi

    echo "========================================"
    print_info "启动视频播放器..."

    # 第三步：播放视频
    print_info "执行播放命令: mpv miku.mp4 & mpv ascii.mp4"

    # 检查mpv是否运行成功
    mpv miku.mp4 & mpv ascii.mp4
    MPV1_PID=$!

    # 检查进程是否在运行
    sleep 1
    if kill -0 $MPV1_PID 2>/dev/null; then
        print_success "两个视频播放器已启动"
        print_info "原始视频 PID: $MPV1_PID"
        print_info "ASCII视频 PID: $MPV2_PID"
    else
        print_warning "部分播放器可能启动失败"
    fi

    echo "========================================"
    print_success "所有步骤执行完成！"
    print_info "脚本执行完毕"
}

# 执行主函数
main "$@"
