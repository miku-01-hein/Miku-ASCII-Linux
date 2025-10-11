#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <iomanip>

class EnhancedASCIIConverter {
private:
    const std::string ASCII_CHARS = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

    std::string currentCharset;
    int frameCount; // 移动到类内部作为成员变量

public:
    EnhancedASCIIConverter() : frameCount(0) {
        // 使用基本字符集确保兼容性
        currentCharset = ASCII_CHARS;
    }

    bool convertToColorASCII(const std::string& inputPath, const std::string& outputPath,
                             int asciiWidth = 80, double quality = 1.0) {
        cv::VideoCapture cap(inputPath);
        if (!cap.isOpened()) {
            std::cerr << "无法打开视频文件: " << inputPath << std::endl;
            return false;
        }

        // 获取视频信息
        double fps = cap.get(cv::CAP_PROP_FPS);
        int totalFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);
        int originalWidth = cap.get(cv::CAP_PROP_FRAME_WIDTH);
        int originalHeight = cap.get(cv::CAP_PROP_FRAME_HEIGHT);

        std::cout << "视频信息: " << originalWidth << "x" << originalHeight
        << ", " << fps << "fps, " << totalFrames << "帧" << std::endl;

        // 计算ASCII输出高度，保持宽高比
        int asciiHeight = static_cast<int>((asciiWidth * originalHeight / originalWidth) * 0.5);

        // 字符大小设置
        int charWidth = 6;  // 固定字符宽度
        int charHeight = 12; // 固定字符高度
        double fontSize = 0.3; // 较小的字体大小

        // 创建视频写入器
        cv::VideoWriter writer;
        cv::Size frameSize(asciiWidth * charWidth, asciiHeight * charHeight);

        std::cout << "输出尺寸: " << frameSize.width << "x" << frameSize.height << std::endl;
        std::cout << "ASCII网格: " << asciiWidth << "x" << asciiHeight << " 字符" << std::endl;
        std::cout << "使用字符集: " << currentCharset.length() << " 个字符" << std::endl;

        writer.open(outputPath,
                    cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                    fps,
                    frameSize);

        if (!writer.isOpened()) {
            std::cerr << "无法创建输出视频文件: " << outputPath << std::endl;
            // 尝试其他编码器
            writer.open(outputPath,
                        cv::VideoWriter::fourcc('a', 'v', 'c', '1'),
                        fps,
                        frameSize);
            if (!writer.isOpened()) {
                cap.release();
                return false;
            }
        }

        cv::Mat frame, resized;
        frameCount = 0; // 重置帧计数器

        std::cout << "开始转换视频..." << std::endl;

        // 测试字符显示
        testCharacterDisplay();

        while (true) {
            cap >> frame;
            if (frame.empty()) break;

            // 调整尺寸为ASCII网格大小
            cv::resize(frame, resized, cv::Size(asciiWidth, asciiHeight), 0, 0, cv::INTER_AREA);

            // 生成彩色ASCII艺术帧
            cv::Mat asciiFrame = generateColorASCIIFrame(resized, charWidth, charHeight, fontSize);

            // 写入视频
            writer.write(asciiFrame);

            frameCount++;
            if (frameCount % 30 == 0) {
                double progress = (frameCount * 100.0) / totalFrames;
                std::cout << "进度: " << frameCount << "/" << totalFrames
                << " 帧 (" << std::fixed << std::setprecision(1) << progress << "%)" << std::endl;
            }
        }

        cap.release();
        writer.release();

        std::cout << "转换完成! 总帧数: " << frameCount << std::endl;
        std::cout << "输出文件: " << outputPath << std::endl;
        return true;
                             }

private:
    void testCharacterDisplay() {
        std::cout << "测试字符显示:" << std::endl;
        std::cout << "基本字符集: " << currentCharset << std::endl;
        std::cout << "字符数量: " << currentCharset.length() << std::endl;

        // 显示字符亮度映射
        std::cout << "字符亮度映射:" << std::endl;
        for (size_t i = 0; i < currentCharset.length(); ++i) {
            double brightness = static_cast<double>(i) / (currentCharset.length() - 1);
            std::cout << "'" << currentCharset[i] << "' -> " << std::fixed << std::setprecision(2) << brightness;
            if (i % 8 == 7) std::cout << std::endl;
            else std::cout << " | ";
        }
        std::cout << std::endl;
    }

    cv::Mat generateColorASCIIFrame(const cv::Mat& colorFrame, int charWidth, int charHeight, double fontSize) {
        int width = colorFrame.cols;
        int height = colorFrame.rows;

        // 创建黑色背景的ASCII帧
        cv::Mat asciiFrame(height * charHeight, width * charWidth, CV_8UC3, cv::Scalar(0, 0, 0));

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                cv::Vec3b pixel = colorFrame.at<cv::Vec3b>(y, x);

                // 计算亮度 (使用加权平均)
                double brightness = (0.299 * pixel[2] + 0.587 * pixel[1] + 0.114 * pixel[0]) / 255.0;

                // 选择ASCII字符
                char asciiChar = getASCIIChar(brightness);

                // 使用原始颜色（BGR格式）
                cv::Scalar textColor(pixel[0], pixel[1], pixel[2]);

                // 计算文本位置
                cv::Point textPos(x * charWidth, (y + 1) * charHeight - 2);

                // 绘制ASCII字符
                cv::putText(asciiFrame,
                            std::string(1, asciiChar),
                            textPos,
                            cv::FONT_HERSHEY_SIMPLEX,
                            fontSize,
                            textColor,
                            1,
                            cv::LINE_AA);

                // 调试输出前几个像素的字符选择（只在第一帧显示）
                if (x < 3 && y < 2 && frameCount == 0) {
                    std::cout << "像素(" << x << "," << y << "): 亮度=" << std::fixed << std::setprecision(3)
                    << brightness << ", 字符='" << asciiChar << "'" << std::endl;
                }
            }
        }

        return asciiFrame;
    }

    char getASCIIChar(double brightness) {
        // 确保亮度在[0,1]范围内
        brightness = std::max(0.0, std::min(1.0, brightness));

        // 简单的线性映射到字符索引
        int index = static_cast<int>(brightness * (currentCharset.length() - 1));
        index = std::max(0, std::min(static_cast<int>(currentCharset.length() - 1), index));

        return currentCharset[index];
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "用法: " << argv[0] << " <输入视频> <输出视频> [ASCII宽度]" << std::endl;
        std::cout << "示例: " << argv[0] << " *.mp4 *.mp4" << std::endl;
        std::cout << "示例: " << argv[0] << " miku.mp4 ascii.mp4 120" << std::endl;
        std::cout << "建议ASCII宽度: 60-150 (数值越大越清晰但文件越大)" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    int asciiWidth = 80;

    if (argc >= 4) {
        asciiWidth = std::atoi(argv[3]);
    }

    // 验证参数
    if (asciiWidth < 20 || asciiWidth > 300) {
        std::cerr << "错误: ASCII宽度应在20-300之间" << std::endl;
        return 1;
    }

    EnhancedASCIIConverter converter;

    std::cout << "========================================" << std::endl;
    std::cout << "彩色ASCII视频转换器" << std::endl;
    std::cout << "========================================" << std::endl;

    if (converter.convertToColorASCII(inputPath, outputPath, asciiWidth, 1.0)) {
        std::cout << "========================================" << std::endl;
        std::cout << "成功创建彩色ASCII艺术视频!" << std::endl;
        std::cout << "输出文件: " << outputPath << std::endl;
        std::cout << "========================================" << std::endl;
    } else {
        std::cerr << "========================================" << std::endl;
        std::cerr << "转换失败!" << std::endl;
        std::cerr << "========================================" << std::endl;
        return 1;
    }

    return 0;
}
