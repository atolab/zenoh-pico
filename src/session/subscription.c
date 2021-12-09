/*
 * Copyright (c) 2017, 2021 ADLINK Technology Inc.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
 * which is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Contributors:
 *   ADLINK zenoh team, <zenoh@adlink-labs.tech>
 */

#include "zenoh-pico/collections/element.h"
#include "zenoh-pico/collections/intmap.h"
#include "zenoh-pico/collections/list.h"
#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/protocol/utils.h"
#include "zenoh-pico/protocol/msg.h"
#include "zenoh-pico/protocol/msgcodec.h"
#include "zenoh-pico/session/session.h"
#include "zenoh-pico/session/session.h"
#include "zenoh-pico/session/resource.h"
#include "zenoh-pico/system/platform.h"
#include "zenoh-pico/utils/logging.h"
#include "zenoh-pico/system/platform.h"

/*------------------ Pull ------------------*/
z_zint_t _zn_get_pull_id(zn_session_t *zn)
{
    return zn->pull_id++;
}

/*------------------ Subscription ------------------*/
/**
 * This function is unsafe because it operates in potentially concurrent data.
 * Make sure that the following mutexes are locked before calling this function:
 *  - zn->mutex_inner
 */
_z_list_t *__unsafe_zn_get_subscriptions_from_remote_key(zn_session_t *zn, const zn_reskey_t *reskey) // @TODO: use type-safe list
{
    _z_list_t *xs = NULL; // @TODO: use type-safe list

    // Case 1) -> numerical only reskey
    if (reskey->rname == NULL)
    {
        _z_list_t *subs = (_z_list_t *)_z_int_void_map_get(&zn->rem_res_loc_sub_map, reskey->rid); // @TODO: use type-safe list and intmap
        while (subs)
        {
            _zn_subscriber_t *sub = (_zn_subscriber_t *)_z_list_head(subs); // @TODO: use type-safe list
            xs = _z_list_push(xs, sub);                                     // @TODO: use type-safe list
            subs = _z_list_tail(subs);                                      // @TODO: use type-safe list
        }
    }
    // Case 2) -> string only reskey
    else if (reskey->rid == ZN_RESOURCE_ID_NONE)
    {
        // The complete resource name of the remote key
        z_str_t rname = reskey->rname;

        _z_list_t *subs = zn->local_subscriptions; // @TODO: use type-safe list
        while (subs)
        {
            _zn_subscriber_t *sub = (_zn_subscriber_t *)_z_list_head(subs); // @TODO: use type-safe list

            // The complete resource name of the subscribed key
            z_str_t lname;
            if (sub->key.rid == ZN_RESOURCE_ID_NONE)
            {
                // Do not allocate
                lname = sub->key.rname;
            }
            else
            {
                // Allocate a computed string
                lname = __unsafe_zn_get_resource_name_from_key(zn, _ZN_IS_LOCAL, &sub->key);
                if (lname == NULL)
                {
                    _z_list_free(&xs, _zn_noop_elem_free); // @TODO: use type-safe list
                    return xs;
                }
            }

            if (zn_rname_intersect(lname, rname))
                xs = _z_list_push(xs, sub); // @TODO: use type-safe list

            if (sub->key.rid != ZN_RESOURCE_ID_NONE)
                free(lname);

            subs = _z_list_tail(subs); // @TODO: use type-safe list
        }
    }
    // Case 3) -> numerical reskey with suffix
    else
    {
        _zn_resource_t *remote = __unsafe_zn_get_resource_by_id(zn, _ZN_IS_REMOTE, reskey->rid);
        if (remote == NULL)
            return xs;

        // Compute the complete remote resource name starting from the key
        z_str_t rname = __unsafe_zn_get_resource_name_from_key(zn, _ZN_IS_REMOTE, reskey);

        _z_list_t *subs = zn->local_subscriptions; // @TODO: use type-safe list
        while (subs)
        {
            _zn_subscriber_t *sub = (_zn_subscriber_t *)_z_list_head(subs); // @TODO: use type-safe list

            // Get the complete resource name to be passed to the subscription callback
            z_str_t lname;
            if (sub->key.rid == ZN_RESOURCE_ID_NONE)
            {
                // Do not allocate
                lname = sub->key.rname;
            }
            else
            {
                // Allocate a computed string
                lname = __unsafe_zn_get_resource_name_from_key(zn, _ZN_IS_LOCAL, &sub->key);
                if (lname == NULL)
                    continue;
            }

            if (zn_rname_intersect(lname, rname))
                xs = _z_list_push(xs, sub); // @TODO: use type-safe list

            if (sub->key.rid != ZN_RESOURCE_ID_NONE)
                free(lname);

            subs = _z_list_tail(subs); // @TODO: use type-safe list
        }

        free(rname);
    }

    return xs;
}

/**
 * This function is unsafe because it operates in potentially concurrent data.
 * Make sure that the following mutexes are locked before calling this function:
 *  - zn->mutex_inner
 */
void __unsafe_zn_add_rem_res_to_loc_sub_map(zn_session_t *zn, z_zint_t id, zn_reskey_t *reskey)
{
    // Check if there is a matching local subscription
    _z_list_t *subs = __unsafe_zn_get_subscriptions_from_remote_key(zn, reskey); // @TODO: use type-safe list
    if (subs != NULL)
    {
        // Update the list
        _z_list_t *sl = (_z_list_t *)_z_int_void_map_get(&zn->rem_res_loc_sub_map, id); // @TODO: use type-safe list
        if (sl)
        {
            // Free any ancient list
            _z_list_free(&sl, _zn_noop_elem_free); // @TODO: use type-safe list
        }
        // Update the list of active subscriptions
        _z_int_void_map_insert(&zn->rem_res_loc_sub_map, id, subs, _zn_noop_elem_free);
    }
}

/**
 * This function is unsafe because it operates in potentially concurrent data.
 * Make sure that the following mutexes are locked before calling this function:
 *  - zn->mutex_inner
 */
_zn_subscriber_t *__unsafe_zn_get_subscription_by_id(zn_session_t *zn, int is_local, z_zint_t id)
{
    _z_list_t *subs = is_local ? zn->local_subscriptions : zn->remote_subscriptions; // @TODO: use type-safe list
    while (subs)
    {
        _zn_subscriber_t *sub = (_zn_subscriber_t *)_z_list_head(subs); // @TODO: use type-safe list

        if (sub->id == id)
            return sub;

        subs = _z_list_tail(subs); // @TODO: use type-safe list
    }

    return NULL;
}

/**
 * This function is unsafe because it operates in potentially concurrent data.
 * Make sure that the following mutexes are locked before calling this function:
 *  - zn->mutex_inner
 */
_zn_subscriber_t *__unsafe_zn_get_subscription_by_key(zn_session_t *zn, int is_local, const zn_reskey_t *reskey)
{
    _z_list_t *subs = is_local ? zn->local_subscriptions : zn->remote_subscriptions; // @TODO: use type-safe list
    while (subs)
    {
        _zn_subscriber_t *sub = (_zn_subscriber_t *)_z_list_head(subs); // @TODO: use type-safe list

        if (sub->key.rid == reskey->rid && _z_str_eq(sub->key.rname, reskey->rname))
            return sub;

        subs = _z_list_tail(subs); // @TODO: use type-safe list
    }

    return NULL;
}

_zn_subscriber_t *_zn_get_subscription_by_id(zn_session_t *zn, int is_local, z_zint_t id)
{
    // Acquire the lock on the subscriptions data struct
    z_mutex_lock(&zn->mutex_inner);
    _zn_subscriber_t *sub = __unsafe_zn_get_subscription_by_id(zn, is_local, id);
    // Release the lock
    z_mutex_unlock(&zn->mutex_inner);
    return sub;
}

_zn_subscriber_t *_zn_get_subscription_by_key(zn_session_t *zn, int is_local, const zn_reskey_t *reskey)
{
    // Acquire the lock on the subscriptions data struct
    z_mutex_lock(&zn->mutex_inner);
    _zn_subscriber_t *sub = __unsafe_zn_get_subscription_by_key(zn, is_local, reskey);
    // Release the lock
    z_mutex_unlock(&zn->mutex_inner);
    return sub;
}

/**
 * This function is unsafe because it operates in potentially concurrent data.
 * Make sure that the following mutexes are locked before calling this function:
 *  - zn->mutex_inner
 */
void __unsafe_zn_add_loc_sub_to_rem_res_map(zn_session_t *zn, _zn_subscriber_t *sub)
{
    // Need to check if there is a remote resource declaration matching the new subscription
    zn_reskey_t loc_key;
    loc_key.rid = ZN_RESOURCE_ID_NONE;
    if (sub->key.rid == ZN_RESOURCE_ID_NONE)
        loc_key.rname = sub->key.rname;
    else
        loc_key.rname = __unsafe_zn_get_resource_name_from_key(zn, _ZN_IS_LOCAL, &sub->key);

    _zn_resource_t *rem_res = __unsafe_zn_get_resource_matching_key(zn, _ZN_IS_REMOTE, &loc_key);
    if (rem_res)
    {
        // Update the list of active subscriptions
        _z_list_t *subs = _z_int_void_map_get(&zn->rem_res_loc_sub_map, rem_res->id);            // @TODO: use type-safe list and intmap
        subs = _z_list_push(subs, sub);                                                          // @TODO: use type-safe list
        _z_int_void_map_insert(&zn->rem_res_loc_sub_map, rem_res->id, subs, _zn_noop_elem_free); // @TODO: use type-safe intmap
    }

    if (sub->key.rid != ZN_RESOURCE_ID_NONE)
        free(loc_key.rname);
}

_z_list_t *_zn_get_subscriptions_from_remote_key(zn_session_t *zn, const zn_reskey_t *reskey) // @TODO: use type-safe list
{
    // Acquire the lock on the subscriptions data struct
    z_mutex_lock(&zn->mutex_inner);
    _z_list_t *xs = __unsafe_zn_get_subscriptions_from_remote_key(zn, reskey); // @TODO: use type-safe list
    // Release the lock
    z_mutex_unlock(&zn->mutex_inner);
    return xs;
}

int _zn_register_subscription(zn_session_t *zn, int is_local, _zn_subscriber_t *sub)
{
    _Z_DEBUG_VA(">>> Allocating sub decl for (%lu,%s)\n", sub->key.rid, sub->key.rname);

    // Acquire the lock on the subscriptions data struct
    z_mutex_lock(&zn->mutex_inner);

    int res;
    _zn_subscriber_t *s = __unsafe_zn_get_subscription_by_key(zn, is_local, &sub->key);
    if (s)
    {
        // A subscription for this key already exists, return error
        res = -1;
    }
    else
    {
        // Register the new subscription
        if (is_local)
        {
            __unsafe_zn_add_loc_sub_to_rem_res_map(zn, sub);
            zn->local_subscriptions = _z_list_push(zn->local_subscriptions, sub); // @TODO: use type-safe list
        }
        else
        {
            zn->remote_subscriptions = _z_list_push(zn->remote_subscriptions, sub); // @TODO: use type-safe list
        }
        res = 0;
    }

    // Release the lock
    z_mutex_unlock(&zn->mutex_inner);

    return res;
}

/**
 * This function is unsafe because it operates in potentially concurrent data.
 * Make sure that the following mutexes are locked before calling this function:
 *  - zn->mutex_inner
 */
void __unsafe_zn_free_subscription(_zn_subscriber_t *sub)
{
    _zn_reskey_clear(&sub->key);
    if (sub->info.period)
        free(sub->info.period);
}

/**
 * This function is unsafe because it operates in potentially concurrent data.
 * Make sure that the following mutexes are locked before calling this function:
 *  - zn->mutex_inner
 */
int __unsafe_zn_subscription_predicate(const void *other, const void *this)
{
    _zn_subscriber_t *o = (_zn_subscriber_t *)other;
    _zn_subscriber_t *t = (_zn_subscriber_t *)this;
    if (t->id == o->id)
    {
        __unsafe_zn_free_subscription(t);
        return 1;
    }
    else
    {
        return 0;
    }
}

void _zn_unregister_subscription(zn_session_t *zn, int is_local, _zn_subscriber_t *s)
{
    // Acquire the lock on the subscription list
    z_mutex_lock(&zn->mutex_inner);

    if (is_local)
        zn->local_subscriptions = _z_list_drop_filter(zn->local_subscriptions, _zn_noop_elem_free, __unsafe_zn_subscription_predicate, s); // @TODO: use type-safe list
    else
        zn->remote_subscriptions = _z_list_drop_filter(zn->remote_subscriptions, _zn_noop_elem_free, __unsafe_zn_subscription_predicate, s); // @TODO: use type-safe list
    free(s);

    // Release the lock
    z_mutex_unlock(&zn->mutex_inner);
}

void _zn_flush_subscriptions(zn_session_t *zn)
{
    // Lock the resources data struct
    z_mutex_lock(&zn->mutex_inner);

    while (zn->local_subscriptions)
    {
        _zn_subscriber_t *sub = (_zn_subscriber_t *)_z_list_head(zn->local_subscriptions);  // @TODO: use type-safe list
        __unsafe_zn_free_subscription(sub);                                                 // @TODO: use type-safe list
        free(sub);                                                                          // @TODO: use type-safe list
        zn->local_subscriptions = _z_list_pop(zn->local_subscriptions, _zn_noop_elem_free); // @TODO: use type-safe list
    }

    while (zn->remote_subscriptions)
    {
        _zn_subscriber_t *sub = (_zn_subscriber_t *)_z_list_head(zn->remote_subscriptions);   // @TODO: use type-safe list
        __unsafe_zn_free_subscription(sub);                                                   // @TODO: use type-safe list
        free(sub);                                                                            // @TODO: use type-safe list
        zn->remote_subscriptions = _z_list_pop(zn->remote_subscriptions, _zn_noop_elem_free); // @TODO: use type-safe list
    }
    _z_int_void_map_clear(&zn->rem_res_loc_sub_map, _zn_noop_elem_free); // @TODO: use type-safe intmap

    // Release the lock
    z_mutex_unlock(&zn->mutex_inner);
}

void _zn_trigger_subscriptions(zn_session_t *zn, const zn_reskey_t reskey, const z_bytes_t payload)
{
    // Acquire the lock on the subscription list
    z_mutex_lock(&zn->mutex_inner);

    // Case 1) -> numeric only reskey
    if (reskey.rname == NULL)
    {
        // Get the declared resource
        _zn_resource_t *res = __unsafe_zn_get_resource_by_id(zn, _ZN_IS_REMOTE, reskey.rid);
        if (res == NULL)
            goto EXIT_SUB_TRIG;

        // Get the complete resource name to be passed to the subscription callback
        z_str_t rname;
        if (res->key.rid == ZN_RESOURCE_ID_NONE)
        {
            // Do not allocate
            rname = res->key.rname;
        }
        else
        {
            // Allocate a computed string
            rname = __unsafe_zn_get_resource_name_from_key(zn, _ZN_IS_LOCAL, &res->key);
            if (rname == NULL)
                goto EXIT_SUB_TRIG;
        }

        // Build the sample
        zn_sample_t s;
        s.key.val = rname;
        s.key.len = strlen(s.key.val);
        s.value = payload;

        // Iterate over the matching subscriptions
        _z_list_t *subs = (_z_list_t *)_z_int_void_map_get(&zn->rem_res_loc_sub_map, reskey.rid); // @TODO: use type-safe list
        while (subs)
        {
            _zn_subscriber_t *sub = (_zn_subscriber_t *)_z_list_head(subs); // @TODO: use type-safe list
            sub->callback(&s, sub->arg);
            subs = _z_list_tail(subs); // @TODO: use type-safe list
        }

        if (res->key.rid != ZN_RESOURCE_ID_NONE)
            free(rname);
    }
    // Case 2) -> string only reskey
    else if (reskey.rid == ZN_RESOURCE_ID_NONE)
    {
        // Build the sample
        zn_sample_t s;
        s.key.val = reskey.rname;
        s.key.len = strlen(s.key.val);
        s.value = payload;

        _z_list_t *subs = zn->local_subscriptions; // @TODO: use type-safe list
        while (subs)
        {
            _zn_subscriber_t *sub = (_zn_subscriber_t *)_z_list_head(subs); // @TODO: use type-safe list

            // Get the complete resource name to be passed to the subscription callback
            z_str_t rname;
            if (sub->key.rid == ZN_RESOURCE_ID_NONE)
            {
                // Do not allocate
                rname = sub->key.rname;
            }
            else
            {
                // Allocate a computed string
                rname = __unsafe_zn_get_resource_name_from_key(zn, _ZN_IS_LOCAL, &sub->key);
                if (rname == NULL)
                    continue;
            }

            if (zn_rname_intersect(rname, reskey.rname))
                sub->callback(&s, sub->arg);

            if (sub->key.rid != ZN_RESOURCE_ID_NONE)
                free(rname);

            subs = _z_list_tail(subs); // @TODO: use type-safe list
        }
    }
    // Case 3) -> numerical reskey with suffix
    else
    {
        // Compute the complete remote resource name starting from the key
        z_str_t rname = __unsafe_zn_get_resource_name_from_key(zn, _ZN_IS_REMOTE, &reskey);
        if (rname == NULL)
            goto EXIT_SUB_TRIG;

        // Build the sample
        zn_sample_t s;
        s.key.val = rname;
        s.key.len = strlen(s.key.val);
        s.value = payload;

        _z_list_t *subs = zn->local_subscriptions; // @TODO: use type-safe list
        while (subs)
        {
            _zn_subscriber_t *sub = (_zn_subscriber_t *)_z_list_head(subs); // @TODO: use type-safe list

            // Get the complete resource name to be passed to the subscription callback
            z_str_t lname;
            if (sub->key.rid == ZN_RESOURCE_ID_NONE)
            {
                // Do not allocate
                lname = sub->key.rname;
            }
            else
            {
                // Allocate a computed string
                lname = __unsafe_zn_get_resource_name_from_key(zn, _ZN_IS_LOCAL, &sub->key);
                if (lname == NULL)
                    continue;
            }

            if (zn_rname_intersect(lname, rname))
                sub->callback(&s, sub->arg);

            if (sub->key.rid != ZN_RESOURCE_ID_NONE)
                free(lname);

            subs = _z_list_tail(subs); // @TODO: use type-safe list
        }

        free(rname);
    }

EXIT_SUB_TRIG:
    // Release the lock
    z_mutex_unlock(&zn->mutex_inner);
}