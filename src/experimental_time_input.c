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

typedef enum {
    Dcf77ExperimentalTimeInputStateNone,
    Dcf77ExperimentalTimeInputStateActive,
    Dcf77ExperimentalTimeInputStateActiveEditing,
} Dcf77ExperimentalTimeInputState;

#define DCF77_EXPERIMENTAL_TIME_INPUT_ROW_TIME 0U
#define DCF77_EXPERIMENTAL_TIME_INPUT_ROW_DATE 1U
#define DCF77_EXPERIMENTAL_TIME_INPUT_ROW_COUNT 2U
#define DCF77_EXPERIMENTAL_TIME_INPUT_COLUMN_COUNT 3U

#define DCF77_EXPERIMENTAL_TIME_INPUT_STATE(model, target_row, target_column)                  \
    ((model)->row == (target_row) && (model)->column == (target_column) ?                      \
         ((model)->editing ? Dcf77ExperimentalTimeInputStateActiveEditing :                     \
                             Dcf77ExperimentalTimeInputStateActive) :                           \
         Dcf77ExperimentalTimeInputStateNone)

static void dcf77_experimental_time_input_cleanup_date(DateTime* datetime) {
    const uint8_t max_day =
        datetime_get_days_per_month(datetime_is_leap_year(datetime->year), datetime->month);

    if(datetime->day > max_day) {
        datetime->day = max_day;
    }
}

static void dcf77_experimental_time_input_normalize(DateTime* datetime) {
    const uint32_t timestamp = datetime_datetime_to_timestamp(datetime);
    datetime_timestamp_to_datetime(timestamp, datetime);
}

static void dcf77_experimental_time_input_draw_block(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    Font font,
    Dcf77ExperimentalTimeInputState state,
    const char* text) {
    canvas_set_color(canvas, ColorBlack);

    if(state != Dcf77ExperimentalTimeInputStateNone) {
        if(state == Dcf77ExperimentalTimeInputStateActiveEditing) {
            const int32_t arrow_x = x + (int32_t)(width / 2U);
            canvas_draw_dot(canvas, arrow_x, y - 3);
            canvas_draw_line(canvas, arrow_x - 1, y - 2, arrow_x + 1, y - 2);
            canvas_draw_line(canvas, arrow_x - 2, y - 1, arrow_x + 2, y - 1);
            canvas_draw_line(
                canvas,
                arrow_x - 2,
                y + (int32_t)height + 1,
                arrow_x + 2,
                y + (int32_t)height + 1);
            canvas_draw_line(
                canvas,
                arrow_x - 1,
                y + (int32_t)height + 2,
                arrow_x + 1,
                y + (int32_t)height + 2);
            canvas_draw_dot(canvas, arrow_x, y + (int32_t)height + 3);
        }
        canvas_draw_rbox(canvas, x, y, width, height, 1);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, width, height, 1);
    }

    canvas_set_font(canvas, font);
    canvas_draw_str_aligned(
        canvas, x + (int32_t)(width / 2U), y + (int32_t)(height / 2U), AlignCenter, AlignCenter, text);

    if(state != Dcf77ExperimentalTimeInputStateNone) {
        canvas_set_color(canvas, ColorBlack);
    }
}

static void dcf77_experimental_time_input_draw_callback(Canvas* canvas, void* context) {
    Dcf77ExperimentalTimeInputModel* model = context;
    char buffer[16];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 16, "Time");

    snprintf(buffer, sizeof(buffer), "%02u", model->datetime.hour);
    dcf77_experimental_time_input_draw_block(
        canvas,
        32,
        2,
        28,
        20,
        FontBigNumbers,
        DCF77_EXPERIMENTAL_TIME_INPUT_STATE(model, DCF77_EXPERIMENTAL_TIME_INPUT_ROW_TIME, 0U),
        buffer);
    canvas_draw_box(canvas, 62, 15, 2, 2);
    canvas_draw_box(canvas, 62, 9, 2, 2);

    snprintf(buffer, sizeof(buffer), "%02u", model->datetime.minute);
    dcf77_experimental_time_input_draw_block(
        canvas,
        66,
        2,
        28,
        20,
        FontBigNumbers,
        DCF77_EXPERIMENTAL_TIME_INPUT_STATE(model, DCF77_EXPERIMENTAL_TIME_INPUT_ROW_TIME, 1U),
        buffer);
    canvas_draw_box(canvas, 96, 15, 2, 2);
    canvas_draw_box(canvas, 96, 9, 2, 2);

    snprintf(buffer, sizeof(buffer), "%02u", model->datetime.second);
    dcf77_experimental_time_input_draw_block(
        canvas,
        100,
        2,
        28,
        20,
        FontBigNumbers,
        DCF77_EXPERIMENTAL_TIME_INPUT_STATE(model, DCF77_EXPERIMENTAL_TIME_INPUT_ROW_TIME, 2U),
        buffer);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 41, "Date");

    snprintf(buffer, sizeof(buffer), "%02u", model->datetime.day);
    dcf77_experimental_time_input_draw_block(
        canvas,
        44,
        28,
        17,
        12,
        FontPrimary,
        DCF77_EXPERIMENTAL_TIME_INPUT_STATE(model, DCF77_EXPERIMENTAL_TIME_INPUT_ROW_DATE, 0U),
        buffer);
    canvas_draw_box(canvas, 65, 36, 2, 2);

    snprintf(buffer, sizeof(buffer), "%02u", model->datetime.month);
    dcf77_experimental_time_input_draw_block(
        canvas,
        71,
        28,
        17,
        12,
        FontPrimary,
        DCF77_EXPERIMENTAL_TIME_INPUT_STATE(model, DCF77_EXPERIMENTAL_TIME_INPUT_ROW_DATE, 1U),
        buffer);
    canvas_draw_box(canvas, 92, 36, 2, 2);

    snprintf(buffer, sizeof(buffer), "%04u", model->datetime.year);
    dcf77_experimental_time_input_draw_block(
        canvas,
        98,
        28,
        30,
        12,
        FontPrimary,
        DCF77_EXPERIMENTAL_TIME_INPUT_STATE(model, DCF77_EXPERIMENTAL_TIME_INPUT_ROW_DATE, 2U),
        buffer);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, "Back to save");
}

static bool dcf77_experimental_time_input_navigation(
    InputEvent* event,
    Dcf77ExperimentalTimeInputModel* model) {
    if(event->key == InputKeyUp) {
        if(model->row > 0U) {
            model->row--;
        }
    } else if(event->key == InputKeyDown) {
        if(model->row + 1U < DCF77_EXPERIMENTAL_TIME_INPUT_ROW_COUNT) {
            model->row++;
        }
    } else if(event->key == InputKeyRight) {
        if(model->column + 1U < DCF77_EXPERIMENTAL_TIME_INPUT_COLUMN_COUNT) {
            model->column++;
        }
    } else if(event->key == InputKeyLeft) {
        if(model->column > 0U) {
            model->column--;
        }
    } else if(event->key == InputKeyOk) {
        model->editing = !model->editing;
    } else if(event->key == InputKeyBack && model->editing) {
        model->editing = false;
    } else {
        return false;
    }

    return true;
}

static bool dcf77_experimental_time_input_edit_time(
    InputEvent* event,
    Dcf77ExperimentalTimeInputModel* model) {
    if(event->key == InputKeyUp) {
        if(model->column == 0U) {
            model->datetime.hour = (model->datetime.hour + 1U) % 24U;
        } else if(model->column == 1U) {
            model->datetime.minute = (model->datetime.minute + 1U) % 60U;
        } else if(model->column == 2U) {
            model->datetime.second = (model->datetime.second + 1U) % 60U;
        } else {
            return false;
        }
        return true;
    }

    if(event->key == InputKeyDown) {
        if(model->column == 0U) {
            model->datetime.hour = (model->datetime.hour == 0U) ? 23U : model->datetime.hour - 1U;
        } else if(model->column == 1U) {
            model->datetime.minute =
                (model->datetime.minute == 0U) ? 59U : model->datetime.minute - 1U;
        } else if(model->column == 2U) {
            model->datetime.second =
                (model->datetime.second == 0U) ? 59U : model->datetime.second - 1U;
        } else {
            return false;
        }
        return true;
    }

    return dcf77_experimental_time_input_navigation(event, model);
}

static bool dcf77_experimental_time_input_edit_date(
    InputEvent* event,
    Dcf77ExperimentalTimeInputModel* model) {
    if(event->key == InputKeyUp) {
        if(model->column == 0U) {
            if(model->datetime.day < 31U) {
                model->datetime.day++;
            }
        } else if(model->column == 1U) {
            if(model->datetime.month < 12U) {
                model->datetime.month++;
            }
        } else if(model->column == 2U) {
            if(model->datetime.year < 2099U) {
                model->datetime.year++;
            }
        } else {
            return false;
        }
    } else if(event->key == InputKeyDown) {
        if(model->column == 0U) {
            if(model->datetime.day > 1U) {
                model->datetime.day--;
            }
        } else if(model->column == 1U) {
            if(model->datetime.month > 1U) {
                model->datetime.month--;
            }
        } else if(model->column == 2U) {
            if(model->datetime.year > 2000U) {
                model->datetime.year--;
            }
        } else {
            return false;
        }
    } else {
        return dcf77_experimental_time_input_navigation(event, model);
    }

    dcf77_experimental_time_input_cleanup_date(&model->datetime);
    dcf77_experimental_time_input_normalize(&model->datetime);
    return true;
}

static bool dcf77_experimental_time_input_input_callback(InputEvent* event, void* context) {
    Dcf77ExperimentalTimeInput* instance = context;
    bool consumed = false;

    with_view_model(
        instance->view,
        Dcf77ExperimentalTimeInputModel * model,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                if(model->editing) {
                    if(model->row == DCF77_EXPERIMENTAL_TIME_INPUT_ROW_TIME) {
                        consumed = dcf77_experimental_time_input_edit_time(event, model);
                    } else if(model->row == DCF77_EXPERIMENTAL_TIME_INPUT_ROW_DATE) {
                        consumed = dcf77_experimental_time_input_edit_date(event, model);
                    }
                } else {
                    consumed = dcf77_experimental_time_input_navigation(event, model);
                }
            }
        },
        true);

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
            model->row = 0U;
            model->column = 0U;
            model->editing = false;
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
            dcf77_experimental_time_input_cleanup_date(&model->datetime);
            dcf77_experimental_time_input_normalize(&model->datetime);
            model->row = 0U;
            model->column = 0U;
            model->editing = false;
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
