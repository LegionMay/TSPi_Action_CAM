// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.5.0
// LVGL version: 9.1.0
// Project name: TSPi_ActionCamera

#include "../ui.h"

void ui_Screen1_screen_init(void)
{
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Image2 = lv_image_create(ui_Screen1);
    lv_obj_set_width(ui_Image2, LV_SIZE_CONTENT);   /// 800
    lv_obj_set_height(ui_Image2, LV_SIZE_CONTENT);    /// 480
    lv_obj_set_align(ui_Image2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image2, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_remove_flag(ui_Image2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Roller2 = lv_roller_create(ui_Screen1);
    lv_roller_set_options(ui_Roller2, "Option 1\nOption 2\nOption 3", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_height(ui_Roller2, 147);
    lv_obj_set_width(ui_Roller2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_x(ui_Roller2, 354);
    lv_obj_set_y(ui_Roller2, -6);
    lv_obj_set_align(ui_Roller2, LV_ALIGN_CENTER);



}
