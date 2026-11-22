#include "robot_face_display.h"

#include "display/lvgl_display/lvgl_theme.h"

#include <esp_log.h>
#include <esp_random.h>
#include <material_symbols.h>
#include <algorithm>
#include <cstring>

#define TAG "RobotFaceDisplay"

namespace {

// Geometry tuned for the 128x128 AtomS3R screen.
constexpr int kFaceWidth = 100;
constexpr int kFaceHeight = 64;
constexpr int kEyeOffsetX = 30;
constexpr int kEyeOffsetY = -10;
constexpr int kMouthOffsetY = 12;
constexpr int kStroke = 4;

constexpr int kEyeDot = 10;
constexpr int kEyeBigDot = 14;
constexpr int kEyeSmallDot = 6;

constexpr uint32_t kFeatureColor = 0xFFFFFF;
constexpr const char* kFaceThemeName = "dark";

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
    // The face is white-on-dark; always use the dark theme.
    auto dark_theme = LvglThemeManager::GetInstance().GetTheme(kFaceThemeName);
    if (dark_theme != nullptr) {
        current_theme_ = dark_theme;
    }
}

void RobotFaceDisplay::SetTheme(Theme* theme) {
    auto dark_theme = LvglThemeManager::GetInstance().GetTheme(kFaceThemeName);
    LcdDisplay::SetTheme(dark_theme != nullptr ? dark_theme : theme);
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
    lv_obj_t* parent = emoji_box_ != nullptr ? emoji_box_ : lv_screen_active();

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
    }

    mouth_bar_ = lv_obj_create(face_);
    lv_obj_set_style_bg_color(mouth_bar_, lv_color_hex(kFeatureColor), 0);
    lv_obj_set_style_bg_opa(mouth_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mouth_bar_, 0, 0);
    lv_obj_set_style_pad_all(mouth_bar_, 0, 0);
    lv_obj_remove_flag(mouth_bar_, LV_OBJ_FLAG_SCROLLABLE);

    mouth_arc_ = CreateArc(face_, 40, kStroke);

    if (emoji_label_ != nullptr) {
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (emoji_image_ != nullptr) {
        lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    }

    ApplyExpression({EyeShape::kDot, EyeShape::kDot, MouthShape::kLine});

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
        {"neutral", {EyeShape::kDot, EyeShape::kDot, MouthShape::kLine}},
        {"robot_2", {EyeShape::kDot, EyeShape::kDot, MouthShape::kLine}},
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
    lv_obj_t* dot = eye_dot_[index];
    lv_obj_t* arc = eye_arc_[index];

    if (shape == EyeShape::kHappy) {
        // Upper half arc reads as a closed, smiling "^" eye.
        lv_arc_set_angles(arc, 200, 340);
        lv_obj_align(arc, LV_ALIGN_CENTER, x, kEyeOffsetY + 5);
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
    lv_obj_align(dot, LV_ALIGN_CENTER, x, kEyeOffsetY);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
}

void RobotFaceDisplay::ApplyMouth(MouthShape shape) {
    switch (shape) {
        case MouthShape::kSmile:
        case MouthShape::kBigSmile: {
            const int size = shape == MouthShape::kBigSmile ? 44 : 40;
            lv_obj_set_size(mouth_arc_, size, size);
            if (shape == MouthShape::kBigSmile) {
                lv_arc_set_angles(mouth_arc_, 20, 160);
            } else {
                lv_arc_set_angles(mouth_arc_, 40, 140);
            }
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
    StopBlink();
    ApplyEye(0, expression.left_eye);
    ApplyEye(1, expression.right_eye);
    ApplyMouth(expression.mouth);

    auto is_dot = [](EyeShape s) {
        return s == EyeShape::kDot || s == EyeShape::kBigDot || s == EyeShape::kSmallDot;
    };
    if (is_dot(expression.left_eye) && is_dot(expression.right_eye)) {
        StartBlink();
    }
}

void RobotFaceDisplay::BlinkAnimCallback(void* var, int32_t value) {
    auto self = static_cast<RobotFaceDisplay*>(var);
    for (int i = 0; i < 2; i++) {
        lv_obj_t* dot = self->eye_dot_[i];
        int32_t base = lv_obj_get_style_width(dot, LV_PART_MAIN);
        lv_obj_set_height(dot, std::max<int32_t>(2, base * value / 100));
    }
}

void RobotFaceDisplay::StartBlink() {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, this);
    lv_anim_set_exec_cb(&a, BlinkAnimCallback);
    lv_anim_set_values(&a, 100, 0);
    lv_anim_set_duration(&a, 80);
    lv_anim_set_reverse_duration(&a, 120);
    lv_anim_set_delay(&a, 2000);
    lv_anim_set_repeat_delay(&a, 3800);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

void RobotFaceDisplay::StopBlink() {
    lv_anim_delete(this, BlinkAnimCallback);
    // Restore round eyes in case the animation stopped mid-blink.
    for (int i = 0; i < 2; i++) {
        if (eye_dot_[i] != nullptr) {
            lv_obj_set_height(eye_dot_[i], lv_obj_get_style_width(eye_dot_[i], LV_PART_MAIN));
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
                StopBlink();
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
    ApplyExpression(expression);
}
