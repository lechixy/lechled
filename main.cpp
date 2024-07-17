#include <iostream>
#include <windows.h>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <algorithm> // For algorithms like max_element

#include "SimpleSerial.h"

// Adjust these to your serial port and baud rate
const char *port = "\\\\.\\COM9";
const int baud_rate = 115200;

using namespace std;

// Function to detect black bars and return the cropping dimensions
RECT detectBlackBars(const std::vector<BYTE>& data, int width, int height) {
    RECT crop = {0, 0, width, height};

    // Detect top black bar
    for (int y = 0; y < height; ++y) {
        bool isBlackRow = true;
        for (int x = 0; x < width; ++x) {
            int index = (y * width + x) * 4;
            if (data[index] != 0 || data[index + 1] != 0 || data[index + 2] != 0) {
                isBlackRow = false;
                break;
            }
        }
        if (!isBlackRow) {
            crop.top = y;
            break;
        }
    }

    // Detect bottom black bar
    for (int y = height - 1; y >= 0; --y) {
        bool isBlackRow = true;
        for (int x = 0; x < width; ++x) {
            int index = (y * width + x) * 4;
            if (data[index] != 0 || data[index + 1] != 0 || data[index + 2] != 0) {
                isBlackRow = false;
                break;
            }
        }
        if (!isBlackRow) {
            crop.bottom = y + 1;
            break;
        }
    }

    // Detect left black bar
    for (int x = 0; x < width; ++x) {
        bool isBlackCol = true;
        for (int y = 0; y < height; ++y) {
            int index = (y * width + x) * 4;
            if (data[index] != 0 || data[index + 1] != 0 || data[index + 2] != 0) {
                isBlackCol = false;
                break;
            }
        }
        if (!isBlackCol) {
            crop.left = x;
            break;
        }
    }

    // Detect right black bar
    for (int x = width - 1; x >= 0; --x) {
        bool isBlackCol = true;
        for (int y = 0; y < height; ++y) {
            int index = (y * width + x) * 4;
            if (data[index] != 0 || data[index + 1] != 0 || data[index + 2] != 0) {
                isBlackCol = false;
                break;
            }
        }
        if (!isBlackCol) {
            crop.right = x + 1;
            break;
        }
    }

    return crop;
}

// Function to capture the most dominant color from the screen
COLORREF getMostDominantColor() {
    // Get the screen dimensions
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);

    // Define a smaller resolution for the bitmap capture
    const int scaled_width = screen_width / 4;
    const int scaled_height = screen_height / 4;

    // Create a device context and a compatible bitmap
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, scaled_width, scaled_height);
    SelectObject(hdcMem, hBitmap);

    // Copy the screen into the bitmap
    StretchBlt(hdcMem, 0, 0, scaled_width, scaled_height, hdcScreen, 0, 0, screen_width, screen_height, SRCCOPY);

    // Retrieve the bitmap data
    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = scaled_width;
    bi.biHeight = -scaled_height;  // top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;  // RGBA
    bi.biCompression = BI_RGB;

    std::vector<BYTE> data(scaled_width * scaled_height * 4); // RGBA data
    GetDIBits(hdcMem, hBitmap, 0, scaled_height, data.data(), (BITMAPINFO *)&bi, DIB_RGB_COLORS);

    // Detect black bars
    RECT crop = detectBlackBars(data, scaled_width, scaled_height);

    // Count occurrences of each color
    std::unordered_map<COLORREF, int> colorCount;
    int blackCount = 0;
    int whiteCount = 0;
    for (int y = crop.top; y < crop.bottom; ++y) {
        for (int x = crop.left; x < crop.right; ++x) {
            int index = (y * scaled_width + x) * 4;
            BYTE r = data[index + 2];
            BYTE g = data[index + 1];
            BYTE b = data[index];
            // If all RGB values are below or equal to 25, count as black
            if (r <= 25 && g <= 25 && b <= 25) {
                blackCount++;
            } else if (r >= 230 && g >= 230 && b >= 230) { // If all RGB values are above or equal to 230, count as white
                whiteCount++;
            } else {
                // Reduce color precision to improve grouping
                r = (r >> 3) << 3;
                g = (g >> 3) << 3;
                b = (b >> 3) << 3;
                COLORREF color = RGB(r, g, b);
                colorCount[color]++;
            }
        }
    }

    // Calculate the total processed pixels
    int totalPixels = (crop.right - crop.left) * (crop.bottom - crop.top);

    // If more than 82% of the processed pixels are black, return black
    if (blackCount > 0.82 * totalPixels) {
        return RGB(0, 0, 0);
    }

    // If more than 90% of the processed pixels are white, return white
    if (whiteCount > 0.90 * totalPixels) {
        return RGB(255, 255, 255);
    }

    // Find the most dominant color
    auto maxColor = std::max_element(colorCount.begin(), colorCount.end(),
        [](const std::pair<COLORREF, int>& a, const std::pair<COLORREF, int>& b) {
            return a.second < b.second;
        });

    // Clean up
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return maxColor->first;
}

void sendColorToArduino(SimpleSerial& arduino, COLORREF color) {
    // Extract RGB components
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    std::cout << "R: " << r << " G: " << g << " B: " << b << std::endl;

    // Format as a string and send over serial
    char buffer[12];
    sprintf(buffer, "%03d%03d%03d", r, g, b);
    arduino.WriteSerialPort(buffer);
    std::cout << buffer << std::endl;
}

int main() {
    SimpleSerial arduino(port, baud_rate);
    if (!arduino.IsConnected()) {
        std::cerr << "Failed to connect to Arduino" << std::endl;
        return 1;
    }

    int colorCount = 0;
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        COLORREF dominantColor = getMostDominantColor();
        sendColorToArduino(arduino, dominantColor);
        colorCount++;

        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = currentTime - startTime;

        if (elapsed.count() >= 1.0) {
            std::cout << "Colors sent in the last second: " << colorCount << std::endl;
            colorCount = 0;
            startTime = currentTime;
        }

        // Sleep for a short duration to prevent excessive CPU usage
        //Sleep(100);
    }

    return 0;
}
