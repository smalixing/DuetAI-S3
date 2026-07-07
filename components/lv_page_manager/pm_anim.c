/*
 * MIT License
 * C-language port of PageManager - animation attributes.
 */
#include "page_manager.h"
#include "pm_log.h"

/* Animation setter / getter implementations (replace C++ lambdas).
 *
 * Each pair drives one animatable property of a page root object so the
 * generic animation engine can interpolate it: the setter writes a value,
 * the getter reads the current value (used as the animation start point).
 *   - x   : horizontal position  (horizontal slide animations)
 *   - y   : vertical position    (vertical slide animations)
 *   - opa : background opacity    (fade animations)
 */

/**
 * @brief  Animation setter: set a page root's X position.
 * @param  obj  The lv_obj_t* page root (passed as void*).
 * @param  v    New X coordinate.
 * @retval None
 */
static void pm_anim_setter_x(void* obj, int32_t v)
{
    lv_obj_set_x((lv_obj_t*)obj, (lv_coord_t)v);
}
/**
 * @brief  Animation getter: read a page root's current X position.
 * @param  obj  The lv_obj_t* page root (passed as void*).
 * @retval current X coordinate.
 */
static int32_t pm_anim_getter_x(void* obj)
{
    return (int32_t)lv_obj_get_x((lv_obj_t*)obj);
}
/**
 * @brief  Animation setter: set a page root's Y position.
 * @param  obj  The lv_obj_t* page root (passed as void*).
 * @param  v    New Y coordinate.
 * @retval None
 */
static void pm_anim_setter_y(void* obj, int32_t v)
{
    lv_obj_set_y((lv_obj_t*)obj, (lv_coord_t)v);
}
/**
 * @brief  Animation getter: read a page root's current Y position.
 * @param  obj  The lv_obj_t* page root (passed as void*).
 * @retval current Y coordinate.
 */
static int32_t pm_anim_getter_y(void* obj)
{
    return (int32_t)lv_obj_get_y((lv_obj_t*)obj);
}
/**
 * @brief  Animation setter: set a page root's background opacity.
 * @param  obj  The lv_obj_t* page root (passed as void*).
 * @param  v    New opacity (LV_OPA_TRANSP..LV_OPA_COVER).
 * @retval None
 */
static void pm_anim_setter_opa(void* obj, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t*)obj, (lv_opa_t)v, LV_PART_MAIN);
}
/**
 * @brief  Animation getter: read a page root's background opacity.
 * @param  obj  The lv_obj_t* page root (passed as void*).
 * @retval current opacity.
 */
static int32_t pm_anim_getter_opa(void* obj)
{
    return (int32_t)lv_obj_get_style_bg_opa((lv_obj_t*)obj, LV_PART_MAIN);
}

/**
 * @brief  Test whether an animation type is one of the "over" family.
 * @note   "Over" animations slide the new page in on top of the old one,
 *         which stays put (OVER_LEFT/RIGHT/TOP/BOTTOM). This drives whether
 *         the underlying page is kept visible and made draggable.
 * @param  anim  Animation type (pm_load_anim_t value).
 * @retval true if @p anim is an over animation.
 */
bool page_manager_get_is_over_anim(uint8_t anim)
{
    return (anim >= PM_LOAD_ANIM_OVER_LEFT && anim <= PM_LOAD_ANIM_OVER_BOTTOM);
}

/**
 * @brief  Test whether an animation type is one of the "move" family.
 * @note   "Move" animations push the old page out as the new page enters
 *         (MOVE_LEFT/RIGHT/TOP/BOTTOM).
 * @param  anim  Animation type (pm_load_anim_t value).
 * @retval true if @p anim is a move animation.
 */
bool page_manager_get_is_move_anim(uint8_t anim)
{
    return (anim >= PM_LOAD_ANIM_MOVE_LEFT && anim <= PM_LOAD_ANIM_MOVE_BOTTOM);
}

/**
 * @brief  Get the animation type currently in effect.
 * @param  self  Pointer to the page manager.
 * @retval The active pm_load_anim_t (resolved global or page-custom type).
 */
pm_load_anim_t page_manager_get_current_load_anim_type(pm_manager_t* self)
{
    return (pm_load_anim_t)self->anim_state.current.type;
}

/**
 * @brief  Resolve the full attribute set for the current animation type.
 * @note   Convenience wrapper: looks up GetCurrentLoadAnimType() via
 *         page_manager_get_load_anim_attr().
 * @param  self  Pointer to the page manager.
 * @param  attr  Out: filled with the geometry and setter/getter for the anim.
 * @retval true on success, false if the current type is invalid.
 */
bool page_manager_get_current_load_anim_attr(pm_manager_t* self, pm_load_anim_attr_t* attr)
{
    return page_manager_get_load_anim_attr(page_manager_get_current_load_anim_type(self), attr);
}

/**
 * @brief  Fill in the drag direction, start/end values and setter/getter for
 *         a given page-load animation type.
 * @note   For each animation type this sets four value pairs describing how
 *         the entering and exiting pages move/fade during a push and during a
 *         pop, expressed relative to the screen size (LV_HOR_RES/LV_VER_RES).
 *         The drag direction selects which property is animated, and the
 *         matching setter/getter pair is assigned: X for horizontal, Y for
 *         vertical, opacity for fade. PM_LOAD_ANIM_NONE zeroes the struct.
 * @param  anim  Animation type (pm_load_anim_t value).
 * @param  attr  Out: attribute structure to populate (must be non-NULL).
 * @retval true  Attributes filled for a known animation type.
 * @retval false @p attr is NULL or @p anim is not a recognised type.
 */
bool page_manager_get_load_anim_attr(uint8_t anim, pm_load_anim_attr_t* attr)
{
    lv_coord_t hor = LV_HOR_RES;
    lv_coord_t ver = LV_VER_RES;

    if (attr == NULL) return false;

    switch (anim) {
    case PM_LOAD_ANIM_OVER_LEFT:
        attr->drag_dir = PM_ROOT_DRAG_DIR_HOR;
        attr->push.enter.start = hor; attr->push.enter.end = 0;
        attr->push.exit.start  = 0;   attr->push.exit.end  = 0;
        attr->pop.enter.start  = 0;   attr->pop.enter.end  = 0;
        attr->pop.exit.start   = 0;   attr->pop.exit.end   = hor;
        break;

    case PM_LOAD_ANIM_OVER_RIGHT:
        attr->drag_dir = PM_ROOT_DRAG_DIR_HOR;
        attr->push.enter.start = -hor; attr->push.enter.end = 0;
        attr->push.exit.start  = 0;    attr->push.exit.end  = 0;
        attr->pop.enter.start  = 0;    attr->pop.enter.end  = 0;
        attr->pop.exit.start   = 0;    attr->pop.exit.end   = -hor;
        break;

    case PM_LOAD_ANIM_OVER_TOP:
        attr->drag_dir = PM_ROOT_DRAG_DIR_VER;
        attr->push.enter.start = ver; attr->push.enter.end = 0;
        attr->push.exit.start  = 0;   attr->push.exit.end  = 0;
        attr->pop.enter.start  = 0;   attr->pop.enter.end  = 0;
        attr->pop.exit.start   = 0;   attr->pop.exit.end   = ver;
        break;

    case PM_LOAD_ANIM_OVER_BOTTOM:
        attr->drag_dir = PM_ROOT_DRAG_DIR_VER;
        attr->push.enter.start = -ver; attr->push.enter.end = 0;
        attr->push.exit.start  = 0;    attr->push.exit.end  = 0;
        attr->pop.enter.start  = 0;    attr->pop.enter.end  = 0;
        attr->pop.exit.start   = 0;    attr->pop.exit.end   = -ver;
        break;

    case PM_LOAD_ANIM_MOVE_LEFT:
        attr->drag_dir = PM_ROOT_DRAG_DIR_HOR;
        attr->push.enter.start = hor; attr->push.enter.end = 0;
        attr->push.exit.start  = 0;   attr->push.exit.end  = -hor;
        attr->pop.enter.start  = -hor; attr->pop.enter.end = 0;
        attr->pop.exit.start   = 0;   attr->pop.exit.end   = hor;
        break;

    case PM_LOAD_ANIM_MOVE_RIGHT:
        attr->drag_dir = PM_ROOT_DRAG_DIR_HOR;
        attr->push.enter.start = -hor; attr->push.enter.end = 0;
        attr->push.exit.start  = 0;    attr->push.exit.end  = hor;
        attr->pop.enter.start  = hor;  attr->pop.enter.end  = 0;
        attr->pop.exit.start   = 0;    attr->pop.exit.end   = -hor;
        break;

    case PM_LOAD_ANIM_MOVE_TOP:
        attr->drag_dir = PM_ROOT_DRAG_DIR_VER;
        attr->push.enter.start = ver;  attr->push.enter.end = 0;
        attr->push.exit.start  = 0;    attr->push.exit.end  = -ver;
        attr->pop.enter.start  = -ver; attr->pop.enter.end  = 0;
        attr->pop.exit.start   = 0;    attr->pop.exit.end   = ver;
        break;

    case PM_LOAD_ANIM_MOVE_BOTTOM:
        attr->drag_dir = PM_ROOT_DRAG_DIR_VER;
        attr->push.enter.start = -ver; attr->push.enter.end = 0;
        attr->push.exit.start  = 0;    attr->push.exit.end  = ver;
        attr->pop.enter.start  = ver;  attr->pop.enter.end  = 0;
        attr->pop.exit.start   = 0;    attr->pop.exit.end   = -ver;
        break;

    case PM_LOAD_ANIM_FADE_ON:
        attr->drag_dir = PM_ROOT_DRAG_DIR_NONE;
        attr->push.enter.start = LV_OPA_TRANSP; attr->push.enter.end = LV_OPA_COVER;
        attr->push.exit.start  = LV_OPA_COVER;  attr->push.exit.end  = LV_OPA_COVER;
        attr->pop.enter.start  = LV_OPA_COVER;  attr->pop.enter.end  = LV_OPA_COVER;
        attr->pop.exit.start   = LV_OPA_COVER;  attr->pop.exit.end   = LV_OPA_TRANSP;
        break;

    case PM_LOAD_ANIM_NONE:
        memset(attr, 0, sizeof(pm_load_anim_attr_t));
        return true;

    default:
        PM_LOG_ERROR("Load anim type error: %d", anim);
        return false;
    }

    /* Setter / getter selection */
    if (attr->drag_dir == PM_ROOT_DRAG_DIR_HOR) {
        attr->setter = pm_anim_setter_x;
        attr->getter = pm_anim_getter_x;
    } else if (attr->drag_dir == PM_ROOT_DRAG_DIR_VER) {
        attr->setter = pm_anim_setter_y;
        attr->getter = pm_anim_getter_y;
    } else {
        attr->setter = pm_anim_setter_opa;
        attr->getter = pm_anim_getter_opa;
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* global animation                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief  Set the global (default) page-load animation.
 * @note   Pages that do not specify their own animation type use these
 *         settings. An out-of-range @p anim is clamped to PM_LOAD_ANIM_NONE.
 * @param  self  Pointer to the page manager.
 * @param  anim  Animation type to use globally.
 * @param  time  Animation duration in milliseconds.
 * @param  path  LVGL easing/path callback for the animation curve.
 * @retval None
 */
void page_manager_set_global_load_anim_type(
    pm_manager_t*     self,
    pm_load_anim_t    anim,
    uint16_t          time,
    lv_anim_path_cb_t path)
{
    if (self == NULL) return;
    if (anim > _PM_LOAD_ANIM_LAST) {
        anim = PM_LOAD_ANIM_NONE;
    }
    self->anim_state.global.type = (uint8_t)anim;
    self->anim_state.global.time = time;
    self->anim_state.global.path = path;

    PM_LOG_INFO("Set global load anim type = %d", anim);
}

/**
 * @brief  Choose the animation attributes to apply for the page being switched.
 * @note   Resolves the "current" animation from either the global settings or
 *         the page's own custom settings:
 *           - If the page's type is PM_LOAD_ANIM_GLOBAL it inherits the global
 *             animation.
 *           - If the page sets a custom type that is out of range, it is reset
 *             to the global animation (error case).
 *           - Otherwise the page's valid custom animation is used.
 *         The result is stored in self->anim_state.current.
 * @param  self  Pointer to the page manager.
 * @param  base  Pointer to the page whose animation is being resolved.
 * @retval None
 */
void page_manager_switch_anim_type_update(pm_manager_t* self, pm_page_base_t* base)
{
    if (self == NULL || base == NULL) return;

    if (base->priv.anim.attr.type == PM_LOAD_ANIM_GLOBAL) {
        PM_LOG_INFO(
            "Page(%s) anim.type was not set, use global = %d",
            base->name, self->anim_state.global.type);
        self->anim_state.current = self->anim_state.global;
    } else {
        if (base->priv.anim.attr.type > _PM_LOAD_ANIM_LAST) {
            PM_LOG_ERROR(
                "Page(%s) ERROR custom anim.type = %d, use global = %d",
                base->name,
                base->priv.anim.attr.type,
                self->anim_state.global.type);
            base->priv.anim.attr = self->anim_state.global;
        } else {
            PM_LOG_INFO(
                "Page(%s) custom anim.type set = %d",
                base->name,
                base->priv.anim.attr.type);
        }
        self->anim_state.current = base->priv.anim.attr;
    }
}

/**
 * @brief  Initialise an lv_anim_t with the current animation's timing.
 * @note   Resets the animation descriptor, then applies the current
 *         duration and easing path. When the current type is PM_LOAD_ANIM_NONE
 *         the duration is forced to 0 so the change is applied instantly.
 *         The caller still sets the var, exec/ready callbacks and values.
 * @param  self  Pointer to the page manager.
 * @param  a     Out: animation descriptor to initialise.
 * @retval None
 */
void page_manager_anim_default_init(pm_manager_t* self, lv_anim_t* a)
{
    lv_anim_init(a);
    uint32_t time = (page_manager_get_current_load_anim_type(self) == PM_LOAD_ANIM_NONE)
                  ? 0
                  : self->anim_state.current.time;
    lv_anim_set_time(a, time);
    lv_anim_set_path_cb(a, self->anim_state.current.path);
}