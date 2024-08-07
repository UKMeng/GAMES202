#include "denoiser.h"

Denoiser::Denoiser() : m_useTemportal(false) {}

void Denoiser::Reprojection(const FrameInfo &frameInfo) {
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    Matrix4x4 preWorldToScreen =
        m_preFrameInfo.m_matrix[m_preFrameInfo.m_matrix.size() - 1];
    Matrix4x4 preWorldToCamera =
        m_preFrameInfo.m_matrix[m_preFrameInfo.m_matrix.size() - 2];
#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // TODO: Reproject
            float id = frameInfo.m_id(x, y);
            if (id < 0) {
                m_valid(x, y) = false;
                m_misc(x, y) = m_accColor(x, y);
                continue;
            }
            Matrix4x4 model = frameInfo.m_matrix[id];
            Matrix4x4 preModel = m_preFrameInfo.m_matrix[id];
            Matrix4x4 transform = preWorldToScreen * preModel * Inverse(model);
            Float3 preScreenPos = transform(frameInfo.m_position(x, y), Float3::Point);
            if (preScreenPos.z < 0 || preScreenPos.x < 0 || preScreenPos.x >= width || preScreenPos.y < 0 || preScreenPos.y >= height || m_preFrameInfo.m_id(preScreenPos.x, preScreenPos.y) != id) {
                m_valid(x, y) = false;
                m_misc(x, y) = m_accColor(x, y);
            } else {
                m_valid(x, y) = true;
                m_misc(x, y) = m_accColor(preScreenPos.x, preScreenPos.y);
            }
        }
    }
    std::swap(m_misc, m_accColor);
}

void Denoiser::TemporalAccumulation(const Buffer2D<Float3> &curFilteredColor) {
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    int kernelRadius = 3;
#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (!m_valid(x, y)) {
                m_misc(x, y) = curFilteredColor(x, y);
                continue;
            }
            // TODO: Temporal clamp
            Float3 color = m_accColor(x, y);
            int xStart = std::max(0, x - kernelRadius);
            int xEnd = std::min(width-1, x + kernelRadius);
            int yStart = std::max(0, y - kernelRadius);
            int yEnd = std::min(height-1, y + kernelRadius);
            int count = 0;
            Float3 colorSum = Float3(0.f);
            for (int nx = xStart; nx <= xEnd; nx++) {
                for (int ny = yStart; ny <= yEnd; ny++) {
                    colorSum += curFilteredColor(nx, ny);
                    count++;
                }
            }
            Float3 colorMean = colorSum / float(count);
            Float3 colorVariance = Float3(0.f);
            for (int nx = xStart; nx <= xEnd; nx++) {
                for (int ny = yStart; ny <= yEnd; ny++) {
                    colorVariance += Sqr(curFilteredColor(nx, ny) - colorMean);
                }
            }
            Float3 sigma = SafeSqrt(colorVariance / float(count));
            color = Clamp(color, colorMean - sigma * m_colorBoxK, colorMean + sigma * m_colorBoxK);

            // TODO: Exponential moving average
            float alpha = m_alpha;
            m_misc(x, y) = Lerp(color, curFilteredColor(x, y), alpha);
        }
    }
    std::swap(m_misc, m_accColor);
}

Buffer2D<Float3> Denoiser::Filter(const FrameInfo &frameInfo) {
    int height = frameInfo.m_beauty.m_height;
    int width = frameInfo.m_beauty.m_width;
    Buffer2D<Float3> filteredImage = CreateBuffer2D<Float3>(width, height);
    int kernelRadius = 16;

#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // TODO: Joint bilateral filter
            if (frameInfo.m_normal(x, y) == Float3(0.f)) {
                filteredImage(x, y) = frameInfo.m_beauty(x, y);
                continue;
            }
            Float3 sumOfWightsValues = Float3(0.f);
            float sumOfWeights = 0.0f;

//            int xStart = std::max(0, x - kernelRadius);
//            int xEnd = std::min(width-1, x + kernelRadius);
//            int yStart = std::max(0, y - kernelRadius);
//            int yEnd = std::min(height-1, y + kernelRadius);

//            for (int nx = xStart; nx <= xEnd; nx++) {
//                for (int ny = yStart; ny <= yEnd; ny++) {
//                    if (frameInfo.m_normal(nx, ny) != Float3(0.f)) {
//                        float coord = ((x - nx) * (x - nx) + (y - ny) * (y - ny)) / (2.0f * m_sigmaCoord * m_sigmaCoord);
//                        float color = SqrDistance(frameInfo.m_beauty(x, y), frameInfo.m_beauty(nx, ny)) / (2.0f * m_sigmaColor * m_sigmaColor);
//                        float normal = Sqr(SafeAcos(Dot(frameInfo.m_normal(x, y), frameInfo.m_normal(nx, ny)))) / (2.0f * m_sigmaNormal * m_sigmaNormal);
//                        float distance = Distance(frameInfo.m_position(x, y), frameInfo.m_position(nx, ny));
//                        float plane = distance == 0.0 ? 0.0:
//                                        Sqr(Dot(frameInfo.m_normal(x, y),(frameInfo.m_position(x, y) - frameInfo.m_position(nx, ny)) / distance))
//                                        / (2.0f * m_sigmaPlane * m_sigmaPlane);
//                        float weight = std::exp( -coord-color-normal-plane);
//                        sumOfWightsValues += frameInfo.m_beauty(nx, ny) * weight;
//                        sumOfWeights += weight;
//                    }
//                }
//            }

            // A-trous Wavelet
            int passes = 4;
            for (int pass = 0; pass < passes; pass++) {
                int step = pow(2, pass);
                int xStart = std::max(0, x - step * 2);
                int xEnd = std::min(width-1, x + step * 2);
                int yStart = std::max(0, y - step * 2);
                int yEnd = std::min(height-1, y + step * 2);
                for (int nx = xStart; nx <= xEnd; nx += step) {
                    for (int ny = yStart; ny <= yEnd; ny += step) {
                        if (frameInfo.m_normal(nx, ny) != Float3(0.f)) {
                            float coord = ((x - nx) * (x - nx) + (y - ny) * (y - ny)) / (2.0f * m_sigmaCoord * m_sigmaCoord);
                            float color = SqrDistance(frameInfo.m_beauty(x, y), frameInfo.m_beauty(nx, ny)) / (2.0f * m_sigmaColor * m_sigmaColor);
                            float normal = Sqr(SafeAcos(Dot(frameInfo.m_normal(x, y), frameInfo.m_normal(nx, ny)))) / (2.0f * m_sigmaNormal * m_sigmaNormal);
                            float distance = Distance(frameInfo.m_position(x, y), frameInfo.m_position(nx, ny));
                            float plane = distance == 0.0 ? 0.0:
                                          Sqr(Dot(frameInfo.m_normal(x, y),(frameInfo.m_position(x, y) - frameInfo.m_position(nx, ny)) / distance))
                                          / (2.0f * m_sigmaPlane * m_sigmaPlane);
                            float weight = std::exp( -coord-color-normal-plane);
                            sumOfWightsValues += frameInfo.m_beauty(nx, ny) * weight;
                            sumOfWeights += weight;
                        }
                    }
                }
            }

            filteredImage(x, y) = sumOfWightsValues / sumOfWeights;
        }
    }
    return filteredImage;
}

void Denoiser::Init(const FrameInfo &frameInfo, const Buffer2D<Float3> &filteredColor) {
    m_accColor.Copy(filteredColor);
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    m_misc = CreateBuffer2D<Float3>(width, height);
    m_valid = CreateBuffer2D<bool>(width, height);
}

void Denoiser::Maintain(const FrameInfo &frameInfo) { m_preFrameInfo = frameInfo; }

Buffer2D<Float3> Denoiser::ProcessFrame(const FrameInfo &frameInfo) {
    // Filter current frame
    Buffer2D<Float3> filteredColor;
    filteredColor = Filter(frameInfo);
    // Reproject previous frame color to current
    if (m_useTemportal) {
        Reprojection(frameInfo);
        TemporalAccumulation(filteredColor);
    } else {
        Init(frameInfo, filteredColor);
    }

    // Maintain
    Maintain(frameInfo);
    if (!m_useTemportal) {
        m_useTemportal = true;
    }
    return m_accColor;
}
