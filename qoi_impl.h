#ifndef QOI_FORMAT_CODEC_QOI_IMPL_H_
#define QOI_FORMAT_CODEC_QOI_IMPL_H_

// Implementation of QOI encoder and decoder
// This file contains the actual implementation that will be included by qoi.h

bool QoiEncodeImpl(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {
    // qoi-header part
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    // qoi-data part
    int run = 0;
    int px_num = width * height;
    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a = 255u;
    uint8_t pre_r = 0, pre_g = 0, pre_b = 0, pre_a = 255;

    for (int i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();

        // Handle first pixel
        if (i == 0) {
            QoiWriteU8(QOI_OP_RGB_TAG);
            QoiWriteU8(r);
            QoiWriteU8(g);
            QoiWriteU8(b);

            int index = QoiColorHash(r, g, b, a);
            history[index][0] = r;
            history[index][1] = g;
            history[index][2] = b;
            history[index][3] = a;

            pre_r = r;
            pre_g = g;
            pre_b = b;
            pre_a = a;
            continue;
        }

        // Check for RUN operation
        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            run++;
            if (run == 62 || i == px_num - 1) {
                QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
                run = 0;
            }
            continue;
        } else if (run > 0) {
            QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
            run = 0;
        }

        // Check for INDEX operation
        int index = QoiColorHash(r, g, b, a);
        if (history[index][0] == r && history[index][1] == g &&
            history[index][2] == b && history[index][3] == a) {
            QoiWriteU8(index);
        } else {
            // Store in history
            history[index][0] = r;
            history[index][1] = g;
            history[index][2] = b;
            history[index][3] = a;

            // Check color differences
            int dr = (int)r - (int)pre_r;
            int dg = (int)g - (int)pre_g;
            int db = (int)b - (int)pre_b;

            if (a == pre_a && dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
                // QOI_OP_DIFF
                QoiWriteU8(QOI_OP_DIFF_TAG | (((dr + 2) & 0x03) << 4) | (((dg + 2) & 0x03) << 2) | ((db + 2) & 0x03));
            } else if (a == pre_a) {
                // Check for LUMA operation
                int drdg = dr - dg;
                int dbdg = db - dg;
                if (dg >= -32 && dg <= 31 && drdg >= -8 && drdg <= 7 && dbdg >= -8 && dbdg <= 7) {
                    // QOI_OP_LUMA
                    QoiWriteU8(QOI_OP_LUMA_TAG | ((dg + 32) & 0x3f));
                    QoiWriteU8((((drdg + 8) & 0x0f) << 4) | ((dbdg + 8) & 0x0f));
                } else {
                    // QOI_OP_RGB or QOI_OP_RGBA
                    if (a == pre_a) {
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                    } else {
                        QoiWriteU8(QOI_OP_RGBA_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                        QoiWriteU8(a);
                    }
                }
            } else {
                // QOI_OP_RGBA (alpha changed)
                QoiWriteU8(QOI_OP_RGBA_TAG);
                QoiWriteU8(r);
                QoiWriteU8(g);
                QoiWriteU8(b);
                QoiWriteU8(a);
            }
        }
        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    // Flush any remaining run
    if (run > 0) {
        QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
    }

    // qoi-padding part
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecodeImpl(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {
    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read header
    width = QoiReadU32();
    height = QoiReadU32();
    channels = QoiReadU8();
    colorspace = QoiReadU8();

    int run = 0;
    int px_num = width * height;
    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a = 255u;
    uint8_t pre_r = 0, pre_g = 0, pre_b = 0, pre_a = 255;

    for (int i = 0; i < px_num; ++i) {
        if (i == 0) {
            // First pixel - read it directly
            r = QoiReadU8();
            g = QoiReadU8();
            b = QoiReadU8();
            if (channels == 4) a = QoiReadU8();
            // Update history
            int index = QoiColorHash(r, g, b, a);
            history[index][0] = r;
            history[index][1] = g;
            history[index][2] = b;
            history[index][3] = a;
        } else if (run > 0) {
            run--;
        } else {
            uint8_t tag = QoiReadU8();

            if (tag == QOI_OP_RGB_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
            } else if (tag == QOI_OP_RGBA_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                a = QoiReadU8();
            } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
                // INDEX operation
                int index = tag;
                r = history[index][0];
                g = history[index][1];
                b = history[index][2];
                a = history[index][3];
            } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
                // DIFF operation
                int8_t dr = ((tag >> 4) & 0x03) - 2;
                int8_t dg = ((tag >> 2) & 0x03) - 2;
                int8_t db = (tag & 0x03) - 2;
                r = pre_r + dr;
                g = pre_g + dg;
                b = pre_b + db;
            } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
                // LUMA operation
                int8_t dg = (tag & 0x3f) - 32;
                uint8_t second_byte = QoiReadU8();
                int8_t drdg = ((second_byte >> 4) & 0x0f) - 8;
                int8_t dbdg = (second_byte & 0x0f) - 8;
                int8_t dr = drdg + dg;
                int8_t db = dbdg + dg;
                r = pre_r + dr;
                g = pre_g + dg;
                b = pre_b + db;
            } else if ((tag & QOI_MASK_2) == QOI_OP_RUN_TAG) {
                // RUN operation
                run = (tag & 0x3f);
            }
        }

        // Update history
        int index = QoiColorHash(r, g, b, a);
        history[index][0] = r;
        history[index][1] = g;
        history[index][2] = b;
        history[index][3] = a;

        // Update previous values
        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
    }

    bool valid = true;
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_IMPL_H_