#include "level.h"
#include "dstuff/ds_dbvt.h"
#include "dstuff/ds_alloc.h"
#include "../editor/ed_level_defs.h"
#include "r_draw.h"
#include "physics.h"

struct r_model_t *l_world_model;
struct ds_dbvn_t l_world_dbvt;
struct p_tmesh_shape_t *l_world_shape;
struct p_collider_t *l_world_collider;

struct l_player_record_t l_player_record;

extern struct ds_slist_t r_lights;

void l_Init()
{

}

void l_Shutdown()
{

}

void l_InitGeometry(struct r_model_geometry_t *geometry)
{
//    w_world_model = r_CreateModel(geometry, NULL);
}

void l_ClearGeometry()
{
//    r_DestroyModel(w_world_model);
//    w_world_model = NULL;
}

void l_ClearLevel()
{
    r_DestroyAllLighs();
}

void l_DeserializeLevel(void *level_buffer, size_t buffer_size)
{
    char *in_buffer = level_buffer;
    struct ed_level_section_t *level_section = (struct ed_level_section_t *)in_buffer;
    struct l_light_section_t *light_section = (struct l_light_section_t *)(in_buffer + level_section->light_section_start);
    struct l_light_record_t *light_records = (struct l_light_record_t *)(in_buffer + light_section->record_start);

    for(uint32_t record_index = 0; record_index < light_section->record_count; record_index++)
    {
        struct l_light_record_t *record = light_records + record_index;
        r_CreateLight(record->type, &record->position, &record->color, record->radius, record->energy);
    }
}
