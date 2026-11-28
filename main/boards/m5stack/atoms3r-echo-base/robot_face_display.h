#ifndef ATOMS3R_ROBOT_FACE_DISPLAY_H
#define ATOMS3R_ROBOT_FACE_DISPLAY_H

#include "display/lcd_display.h"

// SPI LCD display that replaces the emoji with a minimal robot face:
// two white dot eyes and a line mouth on a dark background.
class RobotFaceDisplay : public SpiLcdDisplay {
public:
    RobotFaceDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                     int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                     bool swap_xy);

    void SetupUI() override;
    void SetEmotion(const char* emotion) override;
    void SetTheme(Theme* theme) override;
    void SetMotion(float x, float y, bool dizzy);

private:
    enum class EyeShape { kDot, kBigDot, kSmallDot, kHappy, kClosed };
    enum class MouthShape { kLine, kShortLine, kSmile, kBigSmile, kFrown, kOpen };

    struct Expression {
        EyeShape left_eye;
        EyeShape right_eye;
        MouthShape mouth;
    };

    lv_obj_t* face_ = nullptr;
    lv_obj_t* eye_dot_[2] = {nullptr, nullptr};
    lv_obj_t* eye_arc_[2] = {nullptr, nullptr};
    lv_obj_t* mouth_bar_ = nullptr;
    lv_obj_t* mouth_arc_ = nullptr;
    lv_obj_t* mouth_curve_ = nullptr;
    lv_obj_t* dizzy_eye_[2][2] = {};
    lv_point_precise_t mouth_points_[12] = {};
    Expression expression_ = {EyeShape::kDot, EyeShape::kDot, MouthShape::kSmile};
    bool dizzy_ = false;
    float motion_x_ = 0;
    float motion_y_ = 0;
    lv_timer_t* drift_timer_ = nullptr;

    static bool LookupExpression(const char* emotion, Expression& expression);
    static void DriftTimerCallback(lv_timer_t* timer);

    lv_obj_t* CreateArc(lv_obj_t* parent, int size, int width);
    void ApplyDittoColors();
    void ApplyEye(int index, EyeShape shape);
    void ApplyMouth(MouthShape shape);
    void ApplyExpression(const Expression& expression);
};

#endif  // ATOMS3R_ROBOT_FACE_DISPLAY_H
