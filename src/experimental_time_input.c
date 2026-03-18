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

static uint8_t dcf77_experimental_time_input_days_per_month(uint8_t month, uint16_t year) {
    if(month < 1U || month > 12U) {
        return 31U;
    }

    return datetime_get_days_per_month(datetime_is_leap_year(year), month);
}

static void dcf77_experimental_time_input_normalize_date(DateTime* datetime) {
    if(datetime->year < 1970U) {
        datetime->year = 1970U;
    } else if(datetime->year > 2069U) {
        datetime->year = 2069U;
    }

    if(datetime->month < 1U) {
        datetime->month = 1U;
    } else if(datetime->month > 12U) {
        datetime->month = 12U;
    }

    const uint8_t max_day = dcf77_experimental_time_input_days_per_month(datetime->month, datetime->year);
    if(datetime->day < 1U) {
        datetime->day = 1U;
    } else if(datetime->day > max_day) {
        datetime->day = max_day;
    }
}

static uint8_t dcf77_experimental_time_input_weekday(const DateTime* datetime) {
    DateTime normalized = *datetime;
    const uint32_t timestamp = datetime_datetime_to_timestamp(&normalized);
    datetime_timestamp_to_datetime(timestamp, &normalized);
    return normalized.weekday;
}

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
    static const char* const weekday_labels[] = {"?", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    Dcf77ExperimentalTimeInputModel* model = context;
    char day_text[4];
    char month_text[4];
    char year_text[6];
    char hour_text[4];
    char minute_text[4];
    const uint8_t weekday = dcf77_experimental_time_input_weekday(&model->datetime);

    snprintf(day_text, sizeof(day_text), "%02u", model->datetime.day);
    snprintf(month_text, sizeof(month_text), "%02u", model->datetime.month);
    snprintf(year_text, sizeof(year_text), "%04u", model->datetime.year);
    snprintf(hour_text, sizeof(hour_text), "%02u", model->datetime.hour);
    snprintf(minute_text, sizeof(minute_text), "%02u", model->datetime.minute);

    dcf77_experimental_time_input_draw_value(canvas, 4, 2, 28U, day_text, model->field == 0U);
    canvas_draw_box(canvas, 35, 14, 2, 2);
    dcf77_experimental_time_input_draw_value(canvas, 40, 2, 28U, month_text, model->field == 1U);
    canvas_draw_box(canvas, 71, 14, 2, 2);
    dcf77_experimental_time_input_draw_value(canvas, 76, 2, 48U, year_text, model->field == 2U);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 18, 28, AlignCenter, AlignBottom, weekday_labels[(weekday <= 7U) ? weekday : 0U]);

    dcf77_experimental_time_input_draw_value(canvas, 24, 34, 28U, hour_text, model->field == 3U);
    canvas_draw_box(canvas, 62, 46, 2, 2);
    dcf77_experimental_time_input_draw_value(canvas, 74, 34, 28U, minute_text, model->field == 4U);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Back to save");
}

static bool dcf77_experimental_time_input_step_field(
    Dcf77ExperimentalTimeInputModel* model,
    bool increment) {
    if(model->field == 0U) {
        const uint8_t max_day =
            dcf77_experimental_time_input_days_per_month(model->datetime.month, model->datetime.year);
        if(increment) {
            model->datetime.day = (model->datetime.day >= max_day) ? 1U : model->datetime.day + 1U;
        } else {
            model->datetime.day = (model->datetime.day <= 1U) ? max_day : model->datetime.day - 1U;
        }
        return true;
    }

    if(model->field == 1U) {
        if(increment) {
            model->datetime.month = (model->datetime.month >= 12U) ? 1U : model->datetime.month + 1U;
        } else {
            model->datetime.month = (model->datetime.month <= 1U) ? 12U : model->datetime.month - 1U;
        }
        dcf77_experimental_time_input_normalize_date(&model->datetime);
        return true;
    }

    if(model->field == 2U) {
        if(increment) {
            model->datetime.year = (model->datetime.year >= 2069U) ? 1970U : model->datetime.year + 1U;
        } else {
            model->datetime.year = (model->datetime.year <= 1970U) ? 2069U : model->datetime.year - 1U;
        }
        dcf77_experimental_time_input_normalize_date(&model->datetime);
        return true;
    }

    if(model->field == 3U) {
        if(increment) {
            model->datetime.hour = (model->datetime.hour >= 23U) ? 0U : model->datetime.hour + 1U;
        } else {
            model->datetime.hour = (model->datetime.hour == 0U) ? 23U : model->datetime.hour - 1U;
        }
        return true;
    }

    if(model->field == 4U) {
        if(increment) {
            model->datetime.minute = (model->datetime.minute >= 59U) ? 0U : model->datetime.minute + 1U;
        } else {
            model->datetime.minute = (model->datetime.minute == 0U) ? 59U : model->datetime.minute - 1U;
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
                    if(model->field < 4U) {
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
            dcf77_experimental_time_input_normalize_date(&model->datetime);
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
