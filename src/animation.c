#include "animation.h"
#include "event.h"
 
double function_linear(double x) {
  return x;
}

double function_square(double x) {
  return x*x;
}

double function_tanh(double x) {
  double a = 0.52;
  return a * tanh(2. * atanh(1. / (2. * a)) * (x  - 0.5)) + 0.5;
}

double function_sin(double x) {
  return sin(M_PI / 2. * x);
}

double function_exp(double x) {
  return x*exp(x - 1.);
}

double function_bounce(double x) {
  double alpha = 2.;
  double beta = 0.8;
  if (x < 1. / alpha) {
    return alpha*alpha * x * x;
  }
  else {
    return beta * beta * (x - 1./2. + 1./alpha/2.)
           + 1. - beta*beta* (1./2. + 1./alpha/2.);
  }
}

double function_overshoot(double x) {
  return x * (1. + 0.5*(sin(3. * M_PI * x)));
}

static ANIMATOR_CALLBACK(animator_handler) {
  struct event *event = event_create(&g_event_loop, ANIMATOR_REFRESH, NULL);
  event_loop_post(&g_event_loop, event);
}

struct animation* animation_create() {
  struct animation* animation = malloc(sizeof(struct animation));
  memset(animation, 0, sizeof(struct animation));

  return animation;
}

void animation_destroy(struct animation* animation) {
  if (animation) free(animation);
}

void animation_setup(struct animation* animation, void* target, animator_function* update_function, int initial_value, int final_value, uint32_t duration, char interp_function) {
  animation->counter = 1;
  animation->duration = duration;
  animation->initial_value = initial_value;
  animation->final_value = final_value;
  animation->update_function = update_function;
  animation->target = target;
  animation->seperate_bytes = false;

  if (interp_function == INTERP_FUNCTION_TANH) {
    animation->interp_function = &function_tanh;
  } else if (interp_function == INTERP_FUNCTION_SIN) {
    animation->interp_function = &function_sin;
  } else if (interp_function == INTERP_FUNCTION_QUADRATIC) {
    animation->interp_function = &function_square;
  } else if (interp_function == INTERP_FUNCTION_BOUNCE) {
    animation->interp_function = &function_bounce;
  } else if (interp_function == INTERP_FUNCTION_EXP) {
    animation->interp_function = &function_exp;
  } else if (interp_function == INTERP_FUNCTION_OVERSHOOT) {
    animation->interp_function = &function_overshoot;
  } else {
    animation->interp_function = &function_linear;
  }
}

bool animation_update(struct animation* animation) {
  if (!animation->target
      || !animation->update_function
      || animation->counter > animation->duration) {
    return false;
  }

  if (animation->offset > 0) {
    animation->offset--;
    return false;
  } 

  double slider = animation->interp_function((double)animation->counter
                             / (double)animation->duration);

  int value;
  if (animation->seperate_bytes) {
    for (int i = 0; i < 4; i++) {
      unsigned char byte_i = *((unsigned char*)&animation->initial_value + i);
      unsigned char byte_f = *((unsigned char*)&animation->final_value + i);

      unsigned char byte_val = (1. - slider) * byte_i + slider * byte_f;
      *((unsigned char*)&value + i) = byte_val;
    }
  }
  else {
    value = (1. - slider) * animation->initial_value
            + slider * animation->final_value;
  }

  animation->counter++;
  bool needs_update = animation->update_function(animation->target, value);

  bool found_item = false;
  for (int i = 0; i < g_bar_manager.bar_item_count; i++) {
    if (needs_update
        && (animation->target >= (void*)g_bar_manager.bar_items[i])
        && (animation->target < ((void*)g_bar_manager.bar_items[i]
                                 + sizeof(struct bar_item)         ))) {

      bar_item_needs_update(g_bar_manager.bar_items[i]);
      found_item = true;
    }
  }

  if (!found_item && needs_update) g_bar_manager.bar_needs_update = true;

  return needs_update;
}

void animator_init(struct animator* animator) {
  animator->animations = NULL;
  animator->animation_count = 0;
  animator->interp_function = 0;
  animator->duration = 0;
}

void animator_calculate_offset_for_animation(struct animator* animator, struct animation* animation) {
  if (animator->animation_count < 1) return;

  uint32_t offset = 0;
  struct animation* previous = NULL;
  for (uint32_t i = 0; i < animator->animation_count; i++) {
    struct animation* current = animator->animations[i];
    if (current->target == animation->target
        && current->update_function == animation->update_function) {
      offset += current->duration - current->counter;
      previous = current;
    }
  }
  animation->offset = offset;
  if (previous) animation->initial_value = previous->final_value;
}

void animator_add(struct animator* animator, struct animation* animation) {
  animator_calculate_offset_for_animation(animator, animation);
  animator->animations = realloc(animator->animations,
                                 sizeof(struct animaton*)
                                        * ++animator->animation_count);
  animator->animations[animator->animation_count - 1] = animation;

  if (animator->animation_count == 1) {
    animator->clock = CFRunLoopTimerCreate(NULL,
                                           CFAbsoluteTimeGetCurrent()+1./60.,
                                           1./60.,
                                           0,
                                           0,
                                           animator_handler,
                                           NULL                              );

    CFRunLoopAddTimer(CFRunLoopGetMain(),
                      animator->clock,
                      kCFRunLoopCommonModes);
  }
}

void animator_remove(struct animator* animator, struct animation* animation) {
  if (animator->animation_count == 1) {
    free(animator->animations);
    animator->animations = NULL;
    animator->animation_count = 0;
  } else {
    struct animation* tmp[animator->animation_count - 1];
    int count = 0;
    for (int i = 0; i < animator->animation_count; i++) {
      if (animator->animations[i] == animation) continue;
      tmp[count++] = animator->animations[i];
    }
    animator->animation_count--;
    animator->animations = realloc(animator->animations,
                                   sizeof(struct animation*)
                                          *animator->animation_count);

    memcpy(animator->animations,
           tmp,
           sizeof(struct animation*)*animator->animation_count);
  }

  animation_destroy(animation);

  if (animator->animation_count == 0) {
    CFRunLoopRemoveTimer(CFRunLoopGetMain(),
                         animator->clock,
                         kCFRunLoopCommonModes);
    CFRelease(animator->clock);
  }
}

bool animator_update(struct animator* animator) {
  bool removed = false;
  bool needs_refresh = false;
  for (uint32_t i = 0; i < animator->animation_count; i++) {
    if (removed) i--;
    removed = false;
    needs_refresh |= animation_update(animator->animations[i]);
    if (animator->animations[i]->counter > animator->animations[i]->duration) {
      animator_remove(animator, animator->animations[i]);
      removed = true;
    }
  }
  return needs_refresh;
}
