#include "robot_face_display.h"

#include "display/lvgl_display/lvgl_theme.h"

#include <esp_log.h>
#include <esp_random.h>
#include <material_symbols.h>
#include <algorithm>
#include <cmath>
#include <cstring>

#define TAG "RobotFaceDisplay"

namespace {

// Geometry tuned for the 128x128 AtomS3R screen.
constexpr int kFaceWidth = 100;
constexpr int kFaceHeight = 64;
constexpr int kEyeOffsetX = 26;
constexpr int kEyeOffsetY = -12;
constexpr int kMouthOffsetY = 12;
constexpr int kStroke = 3;

constexpr int kEyeDot = 8;
constexpr int kEyeBigDot = 12;
constexpr int kEyeSmallDot = 6;

constexpr uint32_t kFeatureColor = 0x514650;
constexpr uint32_t kBackgroundColor = 0xB88AD3;
constexpr const char* kFaceThemeName = "dark";
constexpr lv_point_precise_t kMouthPoints[] = {
    {2, 2},   {10, 5},  {18, 8},  {26, 10}, {34, 12}, {42, 13},
    {50, 14}, {58, 15}, {66, 15}, {74, 15}, {82, 15}, {90, 15},
};
constexpr lv_point_precise_t kCrossPoints[2][2] = {{{1, 1}, {11, 11}}, {{11, 1}, {1, 11}}};

// Anti image-retention: shift the whole face by a few pixels periodically so
// the same LCD pixels are not lit by a static image for hours.
constexpr uint32_t kDriftPeriodMs = 2 * 60 * 1000;
constexpr int kDriftRange = 4;  // pixels, in each direction

}  // namespace

RobotFaceDisplay::RobotFaceDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                   int width, int height, int offset_x, int offset_y, bool mirror_x,
                                   bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy) {
    auto dark_theme = LvglThemeManager::GetInstance().GetTheme(kFaceThemeName);
    if (dark_theme != nullptr) {
        current_theme_ = dark_theme;
    }
}

void RobotFaceDisplay::SetTheme(Theme* theme) {
    auto dark_theme = LvglThemeManager::GetInstance().GetTheme(kFaceThemeName);
    LcdDisplay::SetTheme(dark_theme != nullptr ? dark_theme : theme);
    DisplayLockGuard lock(this);
    if (lock) {
        ApplyDittoColors();
    }
}

void RobotFaceDisplay::ApplyDittoColors() {
    const auto background = lv_color_hex(kBackgroundColor);
    const auto foreground = lv_color_hex(kFeatureColor);
    lv_obj_set_style_bg_color(lv_screen_active(), background, 0);
    if (container_ != nullptr) {
        lv_obj_set_style_bg_image_src(container_, nullptr, 0);
        lv_obj_set_style_bg_color(container_, background, 0);
    }
    if (top_bar_ != nullptr) {
        lv_obj_set_style_bg_color(top_bar_, background, 0);
    }
    if (bottom_bar_ != nullptr) {
        lv_obj_set_style_bg_color(bottom_bar_, background, 0);
    }
    lv_obj_t* labels[] = {network_label_, status_label_, notification_label_, mute_label_,
                          battery_label_, emoji_label_,  chat_message_label_};
    for (auto label : labels) {
        if (label != nullptr) {
            lv_obj_set_style_text_color(label, foreground, 0);
        }
    }
}

lv_obj_t* RobotFaceDisplay::CreateArc(lv_obj_t* parent, int size, int width) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(kFeatureColor), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_arc_set_rotation(arc, 0);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    return arc;
}

void RobotFaceDisplay::SetupUI() {
    LcdDisplay::SetupUI();

    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }
    ApplyDittoColors();
    lv_obj_t* parent = emoji_box_ != nullptr ? emoji_box_ : lv_screen_active();
    lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    face_ = lv_obj_create(parent);
    lv_obj_set_size(face_, kFaceWidth, kFaceHeight);
    lv_obj_set_style_bg_opa(face_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(face_, 0, 0);
    lv_obj_set_style_pad_all(face_, 0, 0);
    lv_obj_set_style_radius(face_, 0, 0);
    lv_obj_remove_flag(face_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(face_, LV_ALIGN_CENTER, 0, 0);

    for (int i = 0; i < 2; i++) {
        eye_dot_[i] = lv_obj_create(face_);
        lv_obj_set_style_bg_color(eye_dot_[i], lv_color_hex(kFeatureColor), 0);
        lv_obj_set_style_bg_opa(eye_dot_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(eye_dot_[i], 0, 0);
        lv_obj_set_style_pad_all(eye_dot_[i], 0, 0);
        lv_obj_set_style_radius(eye_dot_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(eye_dot_[i], LV_OBJ_FLAG_SCROLLABLE);

        eye_arc_[i] = CreateArc(face_, 16, 3);
        for (int j = 0; j < 2; ++j) {
            dizzy_eye_[i][j] = lv_line_create(face_);
            lv_obj_set_size(dizzy_eye_[i][j], 13, 13);
            lv_line_set_points(dizzy_eye_[i][j], kCrossPoints[j], 2);
            lv_obj_set_style_line_color(dizzy_eye_[i][j], lv_color_hex(kFeatureColor), 0);
            lv_obj_set_style_line_width(dizzy_eye_[i][j], kStroke, 0);
            lv_obj_set_style_line_rounded(dizzy_eye_[i][j], true, 0);
            lv_obj_align(dizzy_eye_[i][j], LV_ALIGN_CENTER, i == 0 ? -kEyeOffsetX : kEyeOffsetX,
                         kEyeOffsetY + (i == 0 ? 0 : 5));
            lv_obj_add_flag(dizzy_eye_[i][j], LV_OBJ_FLAG_HIDDEN);
        }
    }

    mouth_bar_ = lv_obj_create(face_);
    lv_obj_set_style_bg_color(mouth_bar_, lv_color_hex(kFeatureColor), 0);
    lv_obj_set_style_bg_opa(mouth_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mouth_bar_, 0, 0);
    lv_obj_set_style_pad_all(mouth_bar_, 0, 0);
    lv_obj_remove_flag(mouth_bar_, LV_OBJ_FLAG_SCROLLABLE);

    mouth_arc_ = CreateArc(face_, 40, kStroke);
    mouth_curve_ = lv_line_create(face_);
    lv_obj_set_size(mouth_curve_, 93, 18);
    std::copy(std::begin(kMouthPoints), std::end(kMouthPoints), mouth_points_);
    lv_line_set_points(mouth_curve_, mouth_points_,
                       sizeof(mouth_points_) / sizeof(mouth_points_[0]));
    lv_obj_set_style_line_color(mouth_curve_, lv_color_hex(kFeatureColor), 0);
    lv_obj_set_style_line_width(mouth_curve_, kStroke, 0);
    lv_obj_set_style_line_rounded(mouth_curve_, true, 0);
    lv_obj_align(mouth_curve_, LV_ALIGN_CENTER, 0, kMouthOffsetY);

    if (emoji_label_ != nullptr) {
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (emoji_image_ != nullptr) {
        lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    }

    ApplyExpression({EyeShape::kDot, EyeShape::kDot, MouthShape::kSmile});

    drift_timer_ = lv_timer_create(DriftTimerCallback, kDriftPeriodMs, this);
}

void RobotFaceDisplay::DriftTimerCallback(lv_timer_t* timer) {
    // Runs in the LVGL task with the display lock held.
    auto self = static_cast<RobotFaceDisplay*>(lv_timer_get_user_data(timer));
    const int span = kDriftRange * 2 + 1;
    const int dx = static_cast<int>(esp_random() % span) - kDriftRange;
    const int dy = static_cast<int>(esp_random() % span) - kDriftRange;
    // Move the emoji box (content-sized around the face) when it is the parent,
    // so the box does not grow to fit an off-center child.
    lv_obj_t* target = self->emoji_box_ != nullptr ? self->emoji_box_ : self->face_;
    lv_obj_align(target, LV_ALIGN_CENTER, dx, dy);
}

bool RobotFaceDisplay::LookupExpression(const char* emotion, Expression& expression) {
    struct Entry {
        const char* name;
        Expression expression;
    };
    static const Entry kEntries[] = {
        {"neutral", {EyeShape::kDot, EyeShape::kDot, MouthShape::kSmile}},
        {"robot_2", {EyeShape::kDot, EyeShape::kDot, MouthShape::kSmile}},
        {"relaxed", {EyeShape::kDot, EyeShape::kDot, MouthShape::kLine}},
        {"confident", {EyeShape::kDot, EyeShape::kDot, MouthShape::kLine}},
        {"cool", {EyeShape::kDot, EyeShape::kDot, MouthShape::kLine}},
        {"happy", {EyeShape::kDot, EyeShape::kDot, MouthShape::kSmile}},
        {"loving", {EyeShape::kDot, EyeShape::kDot, MouthShape::kSmile}},
        {"delicious", {EyeShape::kDot, EyeShape::kDot, MouthShape::kSmile}},
        {"laughing", {EyeShape::kHappy, EyeShape::kHappy, MouthShape::kBigSmile}},
        {"funny", {EyeShape::kHappy, EyeShape::kHappy, MouthShape::kBigSmile}},
        {"silly", {EyeShape::kHappy, EyeShape::kHappy, MouthShape::kBigSmile}},
        {"winking", {EyeShape::kDot, EyeShape::kHappy, MouthShape::kSmile}},
        {"kissy", {EyeShape::kHappy, EyeShape::kHappy, MouthShape::kShortLine}},
        {"embarrassed", {EyeShape::kHappy, EyeShape::kHappy, MouthShape::kShortLine}},
        {"sad", {EyeShape::kDot, EyeShape::kDot, MouthShape::kFrown}},
        {"crying", {EyeShape::kDot, EyeShape::kDot, MouthShape::kFrown}},
        {"angry", {EyeShape::kSmallDot, EyeShape::kSmallDot, MouthShape::kFrown}},
        {"surprised", {EyeShape::kBigDot, EyeShape::kBigDot, MouthShape::kOpen}},
        {"shocked", {EyeShape::kBigDot, EyeShape::kBigDot, MouthShape::kOpen}},
        {"thinking", {EyeShape::kDot, EyeShape::kSmallDot, MouthShape::kShortLine}},
        {"confused", {EyeShape::kSmallDot, EyeShape::kDot, MouthShape::kShortLine}},
        {"sleepy", {EyeShape::kClosed, EyeShape::kClosed, MouthShape::kShortLine}},
    };
    for (const auto& entry : kEntries) {
        if (strcmp(entry.name, emotion) == 0) {
            expression = entry.expression;
            return true;
        }
    }
    return false;
}

void RobotFaceDisplay::ApplyEye(int index, EyeShape shape) {
    const int x = index == 0 ? -kEyeOffsetX : kEyeOffsetX;
    const int y = kEyeOffsetY + (index == 0 ? 0 : 5);
    lv_obj_t* dot = eye_dot_[index];
    lv_obj_t* arc = eye_arc_[index];

    if (shape == EyeShape::kHappy) {
        // Upper half arc reads as a closed, smiling "^" eye.
        lv_arc_set_angles(arc, 200, 340);
        lv_obj_align(arc, LV_ALIGN_CENTER, x, y + 5);
        lv_obj_remove_flag(arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    int w = kEyeDot;
    int h = kEyeDot;
    switch (shape) {
        case EyeShape::kBigDot:
            w = h = kEyeBigDot;
            break;
        case EyeShape::kSmallDot:
            w = h = kEyeSmallDot;
            break;
        case EyeShape::kClosed:
            w = 14;
            h = 3;
            break;
        default:
            break;
    }
    lv_obj_set_size(dot, w, h);
    lv_obj_align(dot, LV_ALIGN_CENTER, x, y);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
}

void RobotFaceDisplay::ApplyMouth(MouthShape shape) {
    if (shape == MouthShape::kSmile) {
        lv_obj_add_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mouth_bar_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(mouth_curve_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(mouth_curve_, LV_OBJ_FLAG_HIDDEN);
    switch (shape) {
        case MouthShape::kBigSmile: {
            const int size = 80;
            lv_obj_set_size(mouth_arc_, size, size);
            lv_arc_set_angles(mouth_arc_, 45, 135);
            // Place the bottom of the arc around the neutral mouth line.
            lv_obj_align(mouth_arc_, LV_ALIGN_CENTER, 0, kMouthOffsetY - size / 2 + 4);
            lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(mouth_bar_, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        case MouthShape::kFrown: {
            const int size = 40;
            lv_obj_set_size(mouth_arc_, size, size);
            lv_arc_set_angles(mouth_arc_, 220, 320);
            lv_obj_align(mouth_arc_, LV_ALIGN_CENTER, 0, kMouthOffsetY + size / 2 - 2);
            lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(mouth_bar_, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        default:
            break;
    }

    lv_obj_add_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
    int w = 36;
    int h = kStroke;
    if (shape == MouthShape::kShortLine) {
        w = 16;
    } else if (shape == MouthShape::kOpen) {
        w = 12;
        h = 14;
    }
    lv_obj_set_size(mouth_bar_, w, h);
    lv_obj_set_style_radius(mouth_bar_, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(mouth_bar_, LV_ALIGN_CENTER, 0, kMouthOffsetY);
    lv_obj_remove_flag(mouth_bar_, LV_OBJ_FLAG_HIDDEN);
}

void RobotFaceDisplay::ApplyExpression(const Expression& expression) {
    ApplyEye(0, expression.left_eye);
    ApplyEye(1, expression.right_eye);
    ApplyMouth(expression.mouth);
}

void RobotFaceDisplay::SetMotion(float x, float y, bool dizzy) {
    DisplayLockGuard lock(this);
    if (!lock || face_ == nullptr) {
        return;
    }
    motion_x_ += (std::clamp(x * 1.4f, -1.0f, 1.0f) - motion_x_) * 0.16f;
    motion_y_ += (std::clamp(y * 1.4f, -1.0f, 1.0f) - motion_y_) * 0.16f;
    const int dx = static_cast<int>(std::round(motion_x_ * 13));
    const int dy = static_cast<int>(std::round(motion_y_ * 12));
    lv_obj_set_style_translate_x(face_, dx, 0);
    lv_obj_set_style_translate_y(face_, dy, 0);
    for (int i = 0; i < 2; ++i) {
        const int stretch = static_cast<int>(std::round(motion_x_ * (i == 0 ? -8 : 8)));
        lv_obj_set_style_translate_y(eye_dot_[i], stretch, 0);
        lv_obj_set_style_translate_y(eye_arc_[i], stretch, 0);
        for (auto eye : dizzy_eye_[i]) {
            lv_obj_set_style_translate_y(eye, stretch, 0);
        }
    }
    for (size_t i = 0; i < std::size(mouth_points_); ++i) {
        mouth_points_[i] = kMouthPoints[i];
        mouth_points_[i].y +=
            static_cast<int>(std::round(motion_x_ * (static_cast<float>(i) - 5.5f) * 0.6f));
    }
    lv_obj_invalidate(mouth_curve_);
    if (dizzy_ != dizzy) {
        dizzy_ = dizzy;
        ApplyExpression(expression_);
        for (int i = 0; i < 2; ++i) {
            if (dizzy_) {
                lv_obj_add_flag(eye_dot_[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(eye_arc_[i], LV_OBJ_FLAG_HIDDEN);
            }
            for (auto eye : dizzy_eye_[i]) {
                if (dizzy_) {
                    lv_obj_remove_flag(eye, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(eye, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }
}

void RobotFaceDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) {
        emotion = "neutral";
    }

    Expression expression;
    bool is_face = LookupExpression(emotion, expression);
    if (!is_face && material_symbols_get_utf8(emotion) == nullptr) {
        // Unknown emotion: fall back to the neutral face instead of an emoji.
        is_face = LookupExpression("neutral", expression);
    }

    if (!is_face) {
        // Status icons (e.g. Wi-Fi, download) keep using the stock icon path.
        {
            DisplayLockGuard lock(this);
            if (face_ != nullptr) {
                lv_obj_add_flag(face_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        LcdDisplay::SetEmotion(emotion);
        return;
    }

    DisplayLockGuard lock(this);
    if (face_ == nullptr) {
        ESP_LOGW(TAG, "SetEmotion('%s') called before SetupUI()", emotion);
        return;
    }
    if (gif_controller_) {
        gif_controller_->Stop();
        gif_controller_.reset();
    }
    if (emoji_label_ != nullptr) {
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (emoji_image_ != nullptr) {
        lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(face_, LV_OBJ_FLAG_HIDDEN);
    expression_ = expression;
    if (!dizzy_) {
        ApplyExpression(expression_);
    }
}
