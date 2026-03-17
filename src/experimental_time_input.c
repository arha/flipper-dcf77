#include "experimental_time_input.h"

#include <gui/elements.h>
#include <stdlib.h>

struct Dcf77ExperimentalTimeInput {
    View* view;
};

typedef struct {
    DateTime datetime;
    bool editing;
    uint8_t row;
    uint8_t column;
} Dcf77ExperimentalTimeInputModel;

static void dcf77_experimental_time_input_draw_callback(Canvas* canvas, void* context) {
    Dcf77ExperimentalTimeInputModel* model = context;
    UNUSED(model);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "TODO LOL");
}

static bool dcf77_experimental_time_input_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
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
