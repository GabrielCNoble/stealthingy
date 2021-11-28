#ifndef E_DEFS_H
#define E_DEFS_H

#include <stdint.h>
#include "../lib/dstuff/ds_list.h"
#include "r_defs.h"
#include "p_defs.h"

struct e_ent_def_section_t
{
    size_t record_start;
    size_t record_count;
    size_t reserved[32];
};

struct e_ent_def_record_t
{
    size_t child_start;
    size_t child_count;

    size_t collider_start;

    char model[128];

    mat3_t orientation;
    vec3_t position;
    vec3_t scale;
};

enum E_ENT_DEF_TYPES
{
    E_ENT_DEF_TYPE_CHILD = 0,
    E_ENT_DEF_TYPE_ROOT,
    E_ENT_DEF_TYPE_LAST,
};

struct e_prop_t
{
    char *name;
    void *data;
    uint32_t size;
};

struct e_ent_def_t
{
    uint32_t index;
    uint32_t type;
    /* index used during level serialization to map an
    entity def to its record in the level data */
    uint32_t s_index;

    char name[64];

    struct e_ent_def_t *next;
    struct e_ent_def_t *prev;
    struct e_ent_def_t *children;
    /* all child nodes, including children of children */
    uint32_t children_count;

    struct e_constraint_t *constraints;

    struct r_model_t *model;
    struct p_col_def_t collider;

    mat3_t orientation;
    vec3_t position;
    vec3_t scale;

    struct ds_list_t props;
    /* spawned entity, used only when sorting constraints while spawning an entity */
    struct e_entity_t *entity;
};

struct e_constraint_t
{
    uint32_t index;
    struct e_constraint_t *next;
    struct e_ent_def_t *child_entity;
    struct p_constraint_def_t constraint;
};

enum E_COMPONENT_TYPES
{
    E_COMPONENT_TYPE_NODE = 0,
    E_COMPONENT_TYPE_TRANSFORM,
    E_COMPONENT_TYPE_MODEL,
    E_COMPONENT_TYPE_COLLIDER,
    E_COMPONENT_TYPE_LAST,
};

#define E_ENT_BASE_COMPONENT_FIELDS   \
    struct e_entity_t *entity;        \
    uint32_t index;                   \
    uint32_t type;


struct e_component_t
{
    E_ENT_BASE_COMPONENT_FIELDS;
};

struct e_node_t
{
    E_ENT_BASE_COMPONENT_FIELDS;
    struct e_node_t *parent;
    struct e_node_t *children;
    struct e_node_t *next;
    struct e_node_t *prev;

    mat3_t orientation;
    vec3_t position;
    vec3_t scale;
    vec3_t local_scale;
    uint32_t root_index;
};

struct e_collider_t
{
    E_ENT_BASE_COMPONENT_FIELDS;
    mat3_t offset_rotation;
    vec3_t offset_position;
    struct p_collider_t *collider;
};

struct e_transform_t
{
    E_ENT_BASE_COMPONENT_FIELDS;
    mat4_t transform;
    vec3_t extents;
};

struct e_model_t
{
    E_ENT_BASE_COMPONENT_FIELDS;
    struct r_model_t *model;
};

struct e_entity_t
{
    uint32_t index;
    struct e_ent_def_t *def;
    union
    {
        struct
        {
            struct e_node_t *node;
            struct e_transform_t *transform;
            struct e_model_t *model;
            struct e_collider_t *collider;
        };

        struct e_component_t *components[E_COMPONENT_TYPE_LAST];
    };
};


#endif // E_DEFS_H
