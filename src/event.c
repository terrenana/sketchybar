#include "event.h"
#include "event_loop.h"

extern struct event_loop g_event_loop;
extern struct bar_manager g_bar_manager;
extern int g_connection;

enum event_type event_type_from_string(const char *str) {
    for (int i = EVENT_TYPE_UNKNOWN + 1; i < EVENT_TYPE_COUNT; ++i) {
        if (string_equals(str, event_type_str[i])) return i;
    }
    return EVENT_TYPE_UNKNOWN;
}

struct event *event_create(struct event_loop *event_loop, enum event_type type, void *context) {
    struct event *event = memory_pool_push(&event_loop->pool, struct event);
    event->type = type;
    event->context = context;
    event->info = 0;
    return event;
}

EVENT_CALLBACK(EVENT_HANDLER_DISTRIBUTED_NOTIFICATION) {
    debug("%s\n", context);
    bar_manager_handle_notification(&g_bar_manager, context);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_APPLICATION_FRONT_SWITCHED) {
    debug("%s\n", __FUNCTION__);
    bar_manager_handle_front_app_switch(&g_bar_manager, context);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_SPACE_CHANGED) {
    debug("%s\n", __FUNCTION__);
    bar_manager_handle_space_change(&g_bar_manager, false);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_DISPLAY_CHANGED) {
    debug("%s\n", __FUNCTION__);
    bar_manager_handle_display_change(&g_bar_manager);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_DISPLAY_ADDED) {
    debug("%s\n", __FUNCTION__);
    bar_manager_handle_display_change(&g_bar_manager);
    bar_manager_display_changed(&g_bar_manager);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_DISPLAY_REMOVED) {
    debug("%s\n", __FUNCTION__);
    bar_manager_handle_display_change(&g_bar_manager);
    bar_manager_display_changed(&g_bar_manager);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_DISPLAY_MOVED) {
    debug("%s\n", __FUNCTION__);
    bar_manager_handle_display_change(&g_bar_manager);
    bar_manager_display_changed(&g_bar_manager);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_DISPLAY_RESIZED) {
    debug("%s\n", __FUNCTION__);
    bar_manager_handle_display_change(&g_bar_manager);
    bar_manager_display_changed(&g_bar_manager);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_MENU_BAR_HIDDEN_CHANGED) {
    debug("%s:\n", __FUNCTION__);
    bar_manager_resize(&g_bar_manager);
    g_bar_manager.bar_needs_update = true;
    bar_manager_refresh(&g_bar_manager, false);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_SYSTEM_WOKE) {
    debug("%s:\n", __FUNCTION__);
    bar_manager_handle_system_woke(&g_bar_manager);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_SYSTEM_WILL_SLEEP) {
    debug("%s:\n", __FUNCTION__);
    bar_manager_handle_system_will_sleep(&g_bar_manager);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_SHELL_REFRESH) {
    debug("%s\n", __FUNCTION__);
    bar_manager_update(&g_bar_manager, false);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_ANIMATOR_REFRESH) {
    debug("%s\n", __FUNCTION__);
    bar_manager_animator_refresh(&g_bar_manager);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_MACH_MESSAGE) {
    debug("%s\n", __FUNCTION__);

    if (context) handle_message_mach(context);
    mach_msg_destroy(&((struct mach_buffer*) context)->message.header);
    free(context);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_MOUSE_UP) {
    debug("%s\n", __FUNCTION__);
    debug("EVENT_HANDLER_MOUSE_UP\n");

    uint32_t wid = get_window_id_from_cg_event(context);
    CGEventType type = CGEventGetType(context);
    uint32_t modifier_keys = CGEventGetFlags(context);
    uint32_t adid = display_arrangement(display_active_display_id());

    struct bar_item* bar_item = bar_manager_get_item_by_wid(&g_bar_manager,
                                                            wid,
                                                            adid           );

    if (!bar_item || bar_item->type == BAR_COMPONENT_GROUP) {
      CGPoint point = CGEventGetLocation(context);
      bar_item = bar_manager_get_item_by_point(&g_bar_manager, point, adid);
    }


    debug("item: %s\n", bar_item ? bar_item->name : "NULL");
    bar_item_on_click(bar_item, type, modifier_keys);
    CFRelease(context);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_MOUSE_ENTERED) {
    debug("%s\n", __FUNCTION__);
    debug("EVENT_HANDLER_MOUSE_ENTERED\n");
    uint32_t wid = get_window_id_from_cg_event(context);
    
    uint32_t adid = display_arrangement(display_active_display_id());

    struct bar* bar = bar_manager_get_bar_by_wid(&g_bar_manager, wid);
    if (bar) {
      // Handle global mouse entered event
      if (!bar->mouse_over
          && !bar_manager_mouse_over_any_popup(&g_bar_manager)) {
        bar->mouse_over = true;
        bar_manager_handle_mouse_entered_global(&g_bar_manager);
      }
      
      CFRelease(context);
      return EVENT_SUCCESS;
    }

    struct popup* popup = bar_manager_get_popup_by_wid(&g_bar_manager, wid);
    if (popup) {
      // Handle global mouse entered event
      if (!popup->mouse_over
          && !bar_manager_mouse_over_any_bar(&g_bar_manager)) {
        popup->mouse_over = true;
        bar_manager_handle_mouse_entered_global(&g_bar_manager);
      }
      
      CFRelease(context);
      return EVENT_SUCCESS;
    }
    struct bar_item* bar_item = bar_manager_get_item_by_wid(&g_bar_manager,
                                                            wid,
                                                            adid           );

    if (!bar_item) {
      CGPoint point = CGEventGetLocation(context);
      bar_item = bar_manager_get_item_by_point(&g_bar_manager, point, adid);
    }

    debug("item: %s\n", bar_item ? bar_item->name : "NULL");
    bar_manager_handle_mouse_entered(&g_bar_manager, bar_item);
    CFRelease(context);
    return EVENT_SUCCESS;
}

EVENT_CALLBACK(EVENT_HANDLER_MOUSE_EXITED) {
    debug("%s\n", __FUNCTION__);
    debug("EVENT_HANDLER_MOUSE_EXITED\n");
    uint32_t adid = display_arrangement(display_active_display_id());
    uint32_t wid = get_window_id_from_cg_event(context);

    struct bar* bar,* bar_target;
    struct popup* popup,* popup_target;
    struct window* origin_window;
    bool over_target = false;

    CGPoint point = CGEventGetLocation(context);
    if ((bar = bar_manager_get_bar_by_wid(&g_bar_manager, wid))) {
      origin_window = &bar->window;
      popup_target = bar_manager_get_popup_by_point(&g_bar_manager,
                                                           point          );
      over_target = (popup_target != NULL);
    }
    else if ((popup = bar_manager_get_popup_by_wid(&g_bar_manager, wid))) {
      origin_window = &popup->window;
      bar_target = bar_manager_get_bar_by_point(&g_bar_manager, point);
      over_target = (bar_target != NULL);
    }

    if (bar || popup) {
      // Handle global mouse exited event
      CGRect frame = origin_window->frame;
      frame.origin = origin_window->origin;

      bool over_origin = CGRectContainsPoint(frame, point);

      if (!over_origin && !over_target) {
        if (bar) bar->mouse_over = false;
        else popup->mouse_over = false;
        bar_manager_handle_mouse_exited_global(&g_bar_manager);
      } else if (!over_origin && over_target) {
        if (bar) {
          bar->mouse_over = false;
          popup_target->mouse_over = true;
        }
        else {
          popup->mouse_over = false;
          bar_target->mouse_over = true;
        }
      }
      
      CFRelease(context);
      return EVENT_SUCCESS;
    }

    struct bar_item* bar_item = bar_manager_get_item_by_wid(&g_bar_manager,
                                                            wid,
                                                            adid           );

    if (!bar_item || !(bar_item->update_mask & UPDATE_EXITED_GLOBAL)
        || bar_manager_get_bar_by_point(&g_bar_manager, point)) {
      bar_manager_handle_mouse_exited(&g_bar_manager, bar_item);
    } else if (bar_item) {
      bar_item->mouse_over = false;
    }

    debug("item: %s\n", bar_item ? bar_item->name : "NULL");
    CFRelease(context);
    return EVENT_SUCCESS;
}
