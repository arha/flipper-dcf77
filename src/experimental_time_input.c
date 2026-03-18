#include "experimental_time_input.h"

#include <gui/elements.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct Dcf77ExperimentalTimeInput {
    View* view;
};

typedef struct {
    DateTime datetime;
    uint8_t field;
} Dcf77ExperimentalTimeInputModel;

static void dcf77_experimental_time_input_draw_value(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    const char* text,
    bool active) {
    canvas_set_color(canvas, ColorBlack);

    if(active) {
        canvas_draw_box(canvas, x, y, width, 18);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_frame(canvas, x, y, width, 18);
    }

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas, x + (int32_t)(width / 2U), y + 14, AlignCenter, AlignBottom, text);

    if(active) {
        canvas_set_color(canvas, ColorBlack);
    }
}

static void dcf77_experimental_time_input_draw_callback(Canvas* canvas, void* context) {
    Dcf77ExperimentalTimeInputModel* model = context;
    char day_text[4];
    char month_text[4];
    char year_text[6];

    snprintf(day_text, sizeof(day_text), "%02u", model->datetime.day);
    snprintf(month_text, sizeof(month_text), "%02u", model->datetime.month);
    snprintf(year_text, sizeof(year_text), "%04u", model->datetime.year);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "Preset time");

    dcf77_experimental_time_input_draw_value(canvas, 4, 16, 28U, day_text, model->field == 0U);
    canvas_draw_box(canvas, 35, 28, 2, 2);
    dcf77_experimental_time_input_draw_value(canvas, 40, 16, 28U, month_text, model->field == 1U);
    canvas_draw_box(canvas, 71, 28, 2, 2);
    dcf77_experimental_time_input_draw_value(canvas, 76, 16, 48U, year_text, model->field == 2U);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Back to save");
}

static bool dcf77_experimental_time_input_step_field(
    Dcf77ExperimentalTimeInputModel* model,
    bool increment) {
    if(model->field == 0U) {
        model->datetime.day = increment ? model->datetime.day + 1U : model->datetime.day - 1U;
        return true;
    }

    if(model->field == 1U) {
        model->datetime.month =
            increment ? model->datetime.month + 1U : model->datetime.month - 1U;
        return true;
    }

    if(model->field == 2U) {
        if(increment) {
            model->datetime.year = (model->datetime.year + 1U) % 10000U;
        } else {
            model->datetime.year = (model->datetime.year == 0U) ? 9999U : model->datetime.year - 1U;
        }
        return true;
    }

    return false;
}

static bool dcf77_experimental_time_input_input_callback(InputEvent* event, void* context) {
    Dcf77ExperimentalTimeInput* instance = context;
    bool consumed = false;

    with_view_model(
        instance->view,
        Dcf77ExperimentalTimeInputModel * model,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                if(event->key == InputKeyLeft) {
                    if(model->field > 0U) {
                        model->field--;
                    }
                    consumed = true;
                } else if(event->key == InputKeyRight) {
                    if(model->field < 2U) {
                        model->field++;
                    }
                    consumed = true;
                } else if(event->key == InputKeyUp) {
                    consumed = dcf77_experimental_time_input_step_field(model, true);
                } else if(event->key == InputKeyDown) {
                    consumed = dcf77_experimental_time_input_step_field(model, false);
                }
            }
        },
        consumed);

    return consumed;
}

Dcf77ExperimentalTimeInput* dcf77_experimental_time_input_alloc(void) {
    Dcf77ExperimentalTimeInput* instance = malloc(sizeof(Dcf77ExperimentalTimeInput));
    const DateTime default_datetime = {
        .hour = 0,
        .minute = 0,
        .second = 0,
        .day = 1,
        .month = 1,
        .year = 2000,
        .weekday = 6,
    };

    instance->view = view_alloc();
    view_allocate_model(
        instance->view, ViewModelTypeLocking, sizeof(Dcf77ExperimentalTimeInputModel));
    with_view_model(
        instance->view,
        Dcf77ExperimentalTimeInputModel * model,
        {
            model->datetime = default_datetime;
            model->field = 0U;
        },
        false);
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, dcf77_experimental_time_input_draw_callback);
    view_set_input_callback(instance->view, dcf77_experimental_time_input_input_callback);

    return instance;
}

void dcf77_experimental_time_input_free(Dcf77ExperimentalTimeInput* instance) {
    if(instance == NULL) {
        return;
    }

    view_free(instance->view);
    free(instance);
}

View* dcf77_experimental_time_input_get_view(Dcf77ExperimentalTimeInput* instance) {
    return instance->view;
}

void dcf77_experimental_time_input_set(
    Dcf77ExperimentalTimeInput* instance,
    const DateTime* datetime) {
    if(instance == NULL || datetime == NULL) {
        return;
    }

    with_view_model(
        instance->view,
        Dcf77ExperimentalTimeInputModel * model,
        {
            model->datetime = *datetime;
            model->field = 0U;
        },
        true);
}

bool dcf77_experimental_time_input_get(
    Dcf77ExperimentalTimeInput* instance,
    DateTime* datetime) {
    if(instance == NULL || datetime == NULL) {
        return false;
    }

    bool copied = false;
    with_view_model(
        instance->view,
        Dcf77ExperimentalTimeInputModel * model,
        {
            *datetime = model->datetime;
            copied = true;
        },
        false);

    return copied;
}
