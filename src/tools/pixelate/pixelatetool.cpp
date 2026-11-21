// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "pixelatetool.h"
#include "utils/confighandler.h"

#include <QApplication>
#include <QColor>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <algorithm>
#include <array>
#include <random>

PixelateTool::PixelateTool(QObject* parent)
  : AbstractTwoPointTool(parent)
{
    m_insecurePixelate = ConfigHandler().insecurePixelate();
}

QIcon PixelateTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "pixelate.svg");
}

QString PixelateTool::name() const
{
    return tr("Pixelate");
}

CaptureTool::Type PixelateTool::type() const
{
    return CaptureTool::TYPE_PIXELATE;
}

QString PixelateTool::description() const
{
    return tr("Set Pixelate as the paint tool.");
}

QRect PixelateTool::boundingRect() const
{
    return QRect(points().first, points().second).normalized();
}

CaptureTool* PixelateTool::copy(QObject* parent)
{
    auto* tool = new PixelateTool(parent);
    copyParams(this, tool);
    return tool;
}

void PixelateTool::invalidateCache()
{
    m_effectValid = false;
    m_cachedEffect = QImage();
}

void PixelateTool::drawMove(const QPoint& p)
{
    AbstractTwoPointTool::drawMove(p);
    invalidateCache();
}

void PixelateTool::drawMoveWithAdjustment(const QPoint& p)
{
    AbstractTwoPointTool::drawMoveWithAdjustment(p);
    invalidateCache();
}

void PixelateTool::move(const QPoint& pos)
{
    AbstractTwoPointTool::move(pos);
    invalidateCache();
}

void PixelateTool::onSizeChanged(int size)
{
    AbstractTwoPointTool::onSizeChanged(size);
    invalidateCache();
}

/**
 * Since pixelation does not protect the contents of the pixelated area
 * (see e.g. https://github.com/bishopfox/unredacter),
 * _pseudo-pixelation_ is used:
 *
 * Only colors from the fringe of the selected area are used to generate
 * a pixelation-like effect. The interior of the selected area is not used
 * as an input at all and hence can not be recovered.
 *
 */
void PixelateTool::process(QPainter& painter, const QPixmap& pixmap)
{
    const bool useInsecurePixelate = m_insecurePixelate;

    const qreal pixelRatio = pixmap.devicePixelRatio();
    if (pixelRatio <= 0.0 || pixmap.isNull()) {
        return;
    }
    const QRect logicalBounds(
      QPoint(0, 0), (QSizeF(pixmap.size()) / pixelRatio).toSize());
    QRect selection = boundingRect().intersected(logicalBounds);
    if (selection.isEmpty()) {
        return;
    }
    const QRect selectionScaled =
      QRect(selection.topLeft() * pixelRatio,
            selection.bottomRight() * pixelRatio)
        .intersected(pixmap.rect());
    if (selectionScaled.isEmpty()) {
        return;
    }

    // The effect is deterministic for these inputs. Reuse the cached image
    // across paints (and mouse-move driven repaints) until one of them changes.
    const EffectKey key{
        selection, size(), useInsecurePixelate, pixmap.cacheKey()
    };
    if (!m_effectValid || !(m_effectKey == key)) {
        m_cachedEffect = computeEffect(
          pixmap, selection, selectionScaled, pixelRatio, useInsecurePixelate);
        m_effectKey = key;
        m_effectValid = true;
    }
    const bool prevSmooth =
      painter.testRenderHint(QPainter::SmoothPixmapTransform);
    if (!useInsecurePixelate || size() > 1) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    }
    painter.drawImage(selection, m_cachedEffect);
    if (prevSmooth !=
        painter.testRenderHint(QPainter::SmoothPixmapTransform)) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, prevSmooth);
    }
}

QImage PixelateTool::computeEffect(const QPixmap& pixmap,
                                   const QRect& selection,
                                   const QRect& selectionScaled,
                                   qreal pixelRatio,
                                   bool useInsecurePixelate) const
{
    const int width = qMax(
      1, static_cast<int>(selection.width() * (0.5 / qMax(1, size() + 1))));
    const int height = qMax(
      1, static_cast<int>(selection.height() * (0.5 / qMax(1, size() + 1))));
    const QSize effectSize(width, height);

    if (useInsecurePixelate) {
        if (size() <= 1) {
            const QSize physicalSize =
              (QSizeF(selection.size()) * pixelRatio).toSize();
            QImage effect(physicalSize, QImage::Format_ARGB32_Premultiplied);
            effect.setDevicePixelRatio(pixelRatio);
            effect.fill(Qt::transparent);

            QPainter effectPainter(&effect);
            auto* blur = new QGraphicsBlurEffect;
            blur->setBlurRadius(10);
            auto* item = new QGraphicsPixmapItem(pixmap.copy(selectionScaled));
            item->setGraphicsEffect(blur);

            QGraphicsScene scene;
            scene.addItem(item);

            const QRectF target(QPointF(0, 0), selection.size());
            scene.render(&effectPainter, target, QRectF());
            blur->setBlurRadius(12);
            // multiple repeat for make blur effect stronger
            scene.render(&effectPainter, target, QRectF());
            return effect;
        } else {
            auto pixmapPixelated = pixmap.copy(selectionScaled);
            pixmapPixelated = pixmapPixelated.scaled(
              effectSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            pixmapPixelated =
              pixmapPixelated.scaled(selection.width(), selection.height());
            return pixmapPixelated.toImage();
        }
    }

    // the PRNG is only used for visual effects and NOT part of the security
    // boundary. Fast xorshift32 + 3-uniform Irwin-Hall approximates Gaussian
    // noise without per-pixel log/sqrt on the UI thread.
    quint32 prngState = 42u;
    auto nextFloat01 = [&prngState]() -> float {
        prngState ^= prngState << 13;
        prngState ^= prngState >> 17;
        prngState ^= prngState << 5;
        return static_cast<float>(prngState >> 8) * (1.0f / 16777216.0f);
    };
    auto nextApproxNormal = [&nextFloat01](float stddev) -> float {
        // Sum of 3 U(0,1) minus 1.5 has mean 0 and variance 0.25 (stddev 0.5).
        return (nextFloat01() + nextFloat01() + nextFloat01() - 1.5f) *
               (2.0f * stddev);
    };

    const float samplingStdDev = static_cast<float>(5 * size() + 1);
    constexpr float colorNoiseStdDev = 0.1f;

    QPoint const offset_top(0, selectionScaled.topLeft().y() == 0 ? 0 : -1);
    QPoint const offset_bottom(
      0,
      selectionScaled.bottomLeft().y() == pixmap.rect().bottomLeft().y() ? 0
                                                                         : 1);
    QPoint const offset_left(selectionScaled.topLeft().x() == 0 ? 0 : -1, 0);
    QPoint const offset_right(
      selectionScaled.topRight().x() == pixmap.rect().topRight().x() ? 0 : 1,
      0);

    // only values from the fringe will be used to compute the
    // pseudo-pixelation
    std::array<QImage, 4> fringe = {
        // top fringe
        pixmap
          .copy(QRect(selectionScaled.topLeft() + offset_top,
                      selectionScaled.topRight() + offset_top))
          .toImage(),
        // bottom fringe
        pixmap
          .copy(QRect(selectionScaled.bottomLeft() + offset_bottom,
                      selectionScaled.bottomRight() + offset_bottom))
          .toImage(),
        // left fringe
        pixmap
          .copy(QRect(selectionScaled.topLeft() + offset_left,
                      selectionScaled.bottomLeft() + offset_left))
          .toImage(),
        // right fringe
        pixmap
          .copy(QRect(selectionScaled.topRight() + offset_right,
                      selectionScaled.bottomRight() + offset_right))
          .toImage()
    };

    // Scanline access needs a known 32-bit layout. The screenshot is opaque,
    // so this reads the same color components as QImage::pixel()+QColor did.
    for (auto& img : fringe) {
        if (img.format() != QImage::Format_ARGB32) {
            img = img.convertToFormat(QImage::Format_ARGB32);
        }
    }

    // Image where the pseudo-pixelation is calculated.
    // This will later be scaled to cover the selected area.
    QImage pixelated = QImage(effectSize, QImage::Format_RGB32);
    pixelated.fill(Qt::black);
    QRgb* const outBits = reinterpret_cast<QRgb*>(pixelated.bits());
    const qsizetype outStride =
      pixelated.bytesPerLine() / static_cast<qsizetype>(sizeof(QRgb));

    std::array<const uchar*, 4> fringeBits{};
    std::array<qsizetype, 4> fringeStride{};
    std::array<int, 4> fringeW{};
    std::array<int, 4> fringeH{};
    for (int i = 0; i < 4; ++i) {
        fringeBits[i] = fringe[i].constBits();
        fringeStride[i] = fringe[i].bytesPerLine();
        fringeW[i] = fringe[i].width();
        fringeH[i] = fringe[i].height();
        if (fringeBits[i] == nullptr || fringeW[i] <= 0 || fringeH[i] <= 0) {
            return pixelated;
        }
    }

    constexpr float inv255 = 1.0f / 255.0f;
    const float invW = 1.0f / static_cast<float>(width);
    const float invH = 1.0f / static_cast<float>(height);

    // For every pixel of the effect, we consider four projections
    // to the fringe and sample a pixel from there.
    // Then a horizontal and vertical interpolation are calculated.
    std::array<std::array<float, 3>, 4> samples;

    for (int y = 0; y < height; ++y) {
        QRgb* const outRow = outBits + y * outStride;
        float const vertical = y * invH;
        float const minY = static_cast<float>(qMin(y, height - y)) * invH;
        for (int x = 0; x < width; ++x) {
            float const n = nextApproxNormal(colorNoiseStdDev);
            float const horizontal = x * invW;
            float const minX = static_cast<float>(qMin(x, width - x)) * invW;

            for (int i = 0; i < 4; ++i) {
                int const sx =
                  std::clamp(static_cast<int>(horizontal * fringeW[i] +
                                              nextApproxNormal(samplingStdDev)),
                             0,
                             fringeW[i] - 1);
                int const sy =
                  std::clamp(static_cast<int>(vertical * fringeH[i] +
                                              nextApproxNormal(samplingStdDev)),
                             0,
                             fringeH[i] - 1);
                QRgb const p = reinterpret_cast<const QRgb*>(
                  fringeBits[i] + sy * fringeStride[i])[sx];
                samples[i][0] = static_cast<float>(qRed(p)) * inv255;
                samples[i][1] = static_cast<float>(qGreen(p)) * inv255;
                samples[i][2] = static_cast<float>(qBlue(p)) * inv255;
            }

            // weights of the horizontal resp. vertical interpolation
            float const weight_h = minX - minY + 0.5f;
            float const weight_v = 1.0f - weight_h;

            // compute the weighted sum of the vertical and horizontal
            // interpolations
            std::array<int, 3> rgb = { 0, 0, 0 };
            for (int i = 0; i < 3; ++i) {
                float c =
                  // horizontal interpolation
                  weight_h * ((1 - horizontal) * samples[2][i] +
                              horizontal * samples[3][i])

                  // vertical interpolation
                  + weight_v * ((1 - vertical) * samples[0][i] +
                                vertical * samples[1][i])

                  // additional noise
                  + n;

                rgb[i] = static_cast<int>(0xff * c);
                rgb[i] = std::clamp(rgb[i], 0, 0xff);
            }
            outRow[x] = qRgb(rgb[0], rgb[1], rgb[2]);
        }
    }

    return pixelated;
}

void PixelateTool::drawSearchArea(QPainter& painter, const QPixmap& pixmap)
{
    Q_UNUSED(pixmap)
    painter.fillRect(boundingRect(), QBrush(Qt::black));
}

void PixelateTool::paintMousePreview(QPainter& painter,
                                     const CaptureContext& context)
{
    Q_UNUSED(context)
    Q_UNUSED(painter)
}

void PixelateTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}
