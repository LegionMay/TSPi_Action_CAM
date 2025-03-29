# 使用 Ubuntu 20.04 的 arm64 架构镜像
FROM --platform=linux/arm64 ubuntu:20.04

# 第一步：安装基础工具和证书（必须在其他操作之前）
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# 第二步：配置 DNS（覆盖默认 DNS）
RUN echo "nameserver 8.8.8.8" > /etc/resolv.conf && \
    echo "nameserver 223.5.5.5" >> /etc/resolv.conf

# 第三步：配置国内镜像源
RUN sed -i 's/archive.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list

# 第四步：禁止自动更新和清理缓存
RUN echo 'APT::Get::Never-Include-Phased-Updates "true";' > /etc/apt/apt.conf.d/99-phased-updates && \
    echo 'APT::Update::Post-Invoke-Success {"rm -f /var/cache/apt/archives/*.deb /var/cache/apt/archives/partial/*.deb";};' > /etc/apt/apt.conf.d/99-clean

# 第五步：安装固定版本工具链和开发依赖
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    # 开发基础工具
    build-essential \
    pkg-config \
    openssh-client \
    # GLib 开发
    libglib2.0-dev \
    # GStreamer 核心开发包
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    # GStreamer 扩展插件
    libgstreamer-plugins-good1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    gstreamer1.0-libav \
    # 开发工具
    valgrind \
    cmake \
    # 锁定版本防止升级
    && apt-mark hold gcc g++ libstdc++6 \
    && rm -rf /var/lib/apt/lists/*