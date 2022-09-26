#include "window.h"
#include "bar_manager.h"

extern struct bar_manager g_bar_manager;

void window_init(struct window* window) {
  window->context = NULL;
  window->parent = NULL;
  window->frame = CGRectNull;
  window->id = 0;
  window->origin = CGPointZero;
  window->surface_id = 0;
  window->needs_move = false;
  window->needs_resize = false;
}

static CFTypeRef window_create_region(struct window *window, CGRect frame) {
  CFTypeRef frame_region;
  CGSNewRegionWithRect(&frame, &frame_region);
  return frame_region;
}

void window_create(struct window* window, CGRect frame) {
  uint64_t set_tags = 0;
  uint64_t clear_tags = 0;

  if (g_bar_manager.sticky) {
    set_tags = kCGSStickyTagBit | kCGSHighQualityResamplingTagBit;
    clear_tags = kCGSSuperStickyTagBit;
  }

  window->origin = frame.origin;
  window->frame.origin = CGPointZero;
  window->frame.size = frame.size;

  frame.origin = CGPointZero;
  CFTypeRef frame_region = window_create_region(window, frame);
  uint64_t id;
  SLSNewWindow(g_connection, 2, window->origin.x, window->origin.y,
                                                  frame_region,
                                                  &id              );

  window->id = (uint32_t)id;

  CFRelease(frame_region);

  SLSSetWindowResolution(g_connection, window->id, 2.0f);
  SLSSetWindowTags(g_connection, window->id, &set_tags, 64);
  SLSClearWindowTags(g_connection, window->id, &clear_tags, 64);
  SLSSetWindowOpacity(g_connection, window->id, 0);

  const void* keys[] = { CFSTR("CGWindowContextShouldUseCA") };
  const void* values[] = { kCFBooleanTrue };
  CFDictionaryRef dict = CFDictionaryCreate(NULL,
                                            keys,
                                            values,
                                            1,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);

  window->context = SLWindowContextCreate(g_connection, window->id, dict);
  CFRelease(dict);

  CGContextSetInterpolationQuality(window->context, kCGInterpolationNone);
  window->needs_move = false;
  window->needs_resize = false;
}

void windows_freeze() {
  SLSDisableUpdate(g_connection);
}

void windows_unfreeze() {
  SLSReenableUpdate(g_connection);
}

void window_set_frame(struct window* window, CGRect frame) {
  if (window->needs_move
      || !CGPointEqualToPoint(window->origin, frame.origin)) {

    window->needs_move = true;
    window->origin = frame.origin;
  }

  if (window->needs_resize
      || !CGSizeEqualToSize(window->frame.size, frame.size)) {

    window->needs_resize = true;
    window->frame.size = frame.size;
  }
}

void window_move(struct window* window, CGPoint point) {
  window->origin = point;
  SLSMoveWindow(g_connection, window->id, &point);

  if (__builtin_available(macOS 12.0, *)) {
  } else {
    CFNumberRef number = CFNumberCreate(NULL,
                                        kCFNumberSInt32Type,
                                        &window->id         );

    const void* values[1] = { number };
    CFArrayRef array = CFArrayCreate(NULL, values , 1, &kCFTypeArrayCallBacks);
    SLSReassociateWindowsSpacesByGeometry(g_connection, array);
    CFRelease(array);
    CFRelease(number);
  }
}

bool window_apply_frame(struct window* window) {
  if (window->needs_resize) {
    CFTypeRef frame_region = window_create_region(window, window->frame);
    if (window->parent) window_order(window, window->parent, 0);
    SLSSetWindowShape(g_connection,
                      window->id,
                      0,
                      0,
                      frame_region );

    CFRelease(frame_region);
    window_move(window, window->origin);
    if (window->parent) {
      CGContextClearRect(window->context, window->frame);
      CGContextFlush(window->context);
      window_order(window, window->parent, 1);
    }
    window->needs_move = false;
    window->needs_resize = false;
    return true;
  } else if (window->needs_move) {
    window_move(window, window->origin);
    window->needs_move = false;
    return false;
  }
  return false;
}

void window_send_to_space(struct window* window, uint64_t dsid) {
  CFArrayRef window_list = cfarray_of_cfnumbers(&window->id, sizeof(uint32_t), 1, kCFNumberSInt32Type);
  SLSMoveWindowsToManagedSpace(g_connection, window_list, dsid);
  if (CGPointEqualToPoint(window->origin, g_nirvana)) {
    SLSMoveWindow(g_connection, window->id, &g_nirvana);
  }
}

void window_close(struct window* window) {
  CGContextRelease(window->context);
  SLSReleaseWindow(g_connection, window->id);

  window->context = NULL;
  window->id = 0;
  window->origin = CGPointZero;
  window->frame = CGRectNull;
  window->needs_move = false;
  window->needs_resize = false;
}

void window_set_level(struct window* window, uint32_t level) {
  SLSSetWindowLevel(g_connection, window->id, level);
}

void window_order(struct window* window, struct window* parent, int mode) {
  SLSRemoveFromOrderingGroup(g_connection, window->id);
  if (parent) {
    window->parent = parent;
    SLSOrderWindow(g_connection, window->id, mode, parent->id);
    SLSAddWindowToWindowOrderingGroup(g_connection,
                                      parent->id,
                                      window->id,
                                      mode         );
  } else {
    window->parent = NULL;
    SLSOrderWindow(g_connection, window->id, mode, 0);
  }
}

void window_assign_mouse_tracking_area(struct window* window, CGRect rect) {
  SLSRemoveAllTrackingAreas(g_connection, window->id);
  SLSAddTrackingRect(g_connection, window->id, rect);
}

void window_set_blur_radius(struct window* window, uint32_t blur_radius) {
  SLSSetWindowBackgroundBlurRadius(g_connection, window->id, blur_radius);
}

void context_set_font_smoothing(CGContextRef context, bool smoothing) {
  CGContextSetAllowsFontSmoothing(context, smoothing);
}

void window_disable_shadow(struct window* window) {
  CFIndex shadow_density = 0;
  CFNumberRef shadow_density_cf = CFNumberCreate(kCFAllocatorDefault,
                                                 kCFNumberCFIndexType,
                                                 &shadow_density      );

  const void *keys[1] = { CFSTR("com.apple.WindowShadowDensity") };
  const void *values[1] = { shadow_density_cf };
  CFDictionaryRef shadow_props_cf = CFDictionaryCreate(NULL,
                                             keys,
                                             values,
                                             1,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);

  SLSWindowSetShadowProperties(window->id, shadow_props_cf);
  CFRelease(shadow_density_cf);
  CFRelease(shadow_props_cf);
}

CGImageRef window_capture(struct window* window) {
  CGImageRef image_ref = NULL;

  uint64_t wid = window->id;
  SLSCaptureWindowsContentsToRectWithOptions(g_connection,
                                             &wid,
                                             true,
                                             CGRectNull,
                                             1 << 8,
                                             &image_ref  );

  CGRect bounds;
  SLSGetScreenRectForWindow(g_connection, wid, &bounds);
  bounds.size.width = (uint32_t) (bounds.size.width + 0.5);
  window->frame.size = bounds.size;

  return image_ref;
}
