#include "ui.h"
#include "page.h"
#include "image.h"
#include "font.h"

void error_page_display(const char *msg)
{   
    ui_draw_image(0,0,&wifi_error_image);
}
