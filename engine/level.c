#include "level.h"
#include "dstuff/ds_dbvt.h"
#include "dstuff/ds_alloc.h"
#include "../editor/ed_level_defs.h"
#include "r_draw.h"
#include "ent.h"
#include "phys.h"

struct r_model_t *l_world_model;
struct ds_dbvn_t l_world_dbvt;
struct p_shape_def_t *l_world_shape;
struct p_collider_t *l_world_collider;
struct p_col_def_t l_world_col_def;

struct l_player_record_t l_player_record;

extern struct r_texture_t *r_default_albedo_texture;
extern struct r_texture_t *r_default_normal_texture;
extern struct r_texture_t *r_default_roughness_texture;

void l_Init()
{
    l_world_shape = p_AllocShapeDef();
    l_world_shape->type = P_COL_SHAPE_TYPE_ITRI_MESH;

    l_world_col_def.shape_count = 1;
    l_world_col_def.shape = l_world_shape;
    l_world_col_def.type = P_COLLIDER_TYPE_STATIC;
    l_world_col_def.mass = 0.0;
}

void l_Shutdown()
{

}

void l_DestroyWorld()
{
    if(l_world_collider)
    {
        mem_Free(l_world_shape->itri_mesh.indices);
        l_world_shape->itri_mesh.indices = NULL;
        l_world_shape->itri_mesh.index_count = 0;
        mem_Free(l_world_shape->itri_mesh.verts);
        l_world_shape->itri_mesh.verts = NULL;
        l_world_shape->itri_mesh.vert_count = 0;

        p_DestroyCollider(l_world_collider);
        l_world_collider = NULL;
    }

    if(l_world_model)
    {
        r_DestroyModel(l_world_model);
        l_world_model = NULL;
    }
}

void l_ClearLevel()
{
    r_DestroyAllLighs();
    e_DestroyAllEntities();
    l_DestroyWorld();
}

void l_DeserializeLevel(void *level_buffer, size_t buffer_size, uint32_t data_flags)
{
    char *in_buffer = level_buffer;
    struct ed_level_section_t *level_section = (struct ed_level_section_t *)in_buffer;

    if(data_flags & L_LEVEL_DATA_LIGHTS)
    {
        struct l_light_section_t *light_section = (struct l_light_section_t *)(in_buffer + level_section->light_section_start);
        struct l_light_record_t *light_records = (struct l_light_record_t *)(in_buffer + light_section->record_start);

        for(uint32_t record_index = 0; record_index < light_section->record_count; record_index++)
        {
            struct l_light_record_t *record = light_records + record_index;
            struct r_light_t *light = r_CreateLight(record->type, &record->position, &record->color, record->radius, record->energy);
            record->d_index = light->index;
        }
    }

    struct l_material_section_t *material_section = (struct l_material_section_t *)(in_buffer + level_section->material_section_start);
    struct l_material_record_t *material_records = (struct l_material_record_t *)(in_buffer + material_section->record_start);

    for(uint32_t record_index = 0; record_index < material_section->record_count; record_index++)
    {
        struct l_material_record_t *record = material_records + record_index;
        struct r_material_t *material = r_FindMaterial(record->name);

        if(!material)
        {
            struct r_texture_t *diffuse_texture;
            struct r_texture_t *normal_texture;
            struct r_texture_t *roughness_texture;

            diffuse_texture = r_FindTexture(record->diffuse_texture);
            if(!diffuse_texture)
            {
                diffuse_texture = r_default_albedo_texture;
            }

            normal_texture = r_FindTexture(record->normal_texture);
            if(!normal_texture)
            {
                normal_texture = r_default_normal_texture;
            }

            roughness_texture = r_FindTexture(record->roughness_texture);
            if(!roughness_texture)
            {
                roughness_texture = r_default_roughness_texture;
            }

            material = r_CreateMaterial(record->name, diffuse_texture, normal_texture, roughness_texture);
        }

        record->material = material;
    }

    if(data_flags & L_LEVEL_DATA_ENTITIES)
    {
        struct l_ent_def_section_t *ent_def_section = (struct l_ent_def_section_t *)(in_buffer + level_section->ent_def_section_start);
        struct l_ent_def_record_t *ent_def_records = (struct l_ent_def_record_t *)(in_buffer + ent_def_section->record_start);

        for(uint32_t record_index = 0; record_index < ent_def_section->record_count; record_index++)
        {
            struct l_ent_def_record_t *record = ent_def_records + record_index;

            record->def = e_FindEntDef(E_ENT_DEF_TYPE_ROOT, record->name);

            if(!record->def)
            {
                record->def = e_LoadEntDef(record->name);
            }
        }

        struct l_entity_section_t *entity_section = (struct l_entity_section_t *)(in_buffer + level_section->entity_section_start);
        struct l_entity_record_t *entity_records = (struct l_entity_record_t *)(in_buffer + entity_section->record_start);

        for(uint32_t record_index = 0; record_index < entity_section->record_count; record_index++)
        {
            struct l_entity_record_t *record = entity_records + record_index;
            struct e_ent_def_t *ent_def = ent_def_records[record->ent_def].def;
            struct e_entity_t *entity = e_SpawnEntity(ent_def, &record->position, &record->scale, &record->orientation);
            record->d_index = entity->index;
        }
    }

    if(data_flags & L_LEVEL_DATA_WORLD)
    {
        struct l_world_section_t *world_section = (struct l_world_section_t *)(in_buffer + level_section->world_section_start);
        struct r_vert_t *verts = (struct r_vert_t *)(in_buffer + world_section->vert_start);
        uint32_t *indices = (uint32_t *)(in_buffer + world_section->index_start);
        struct l_batch_record_t *batch_records = (struct l_batch_record_t *)(in_buffer + world_section->batch_start);

        struct ds_buffer_t index_buffer = ds_buffer_create(sizeof(uint32_t), world_section->index_count);
        struct ds_buffer_t batch_buffer = ds_buffer_create(sizeof(struct r_batch_t), world_section->batch_count);
        struct ds_buffer_t col_verts = ds_buffer_create(sizeof(vec3_t), world_section->vert_count);

        ds_buffer_fill(&index_buffer, 0, indices, world_section->index_count);

        for(uint32_t batch_index = 0; batch_index < world_section->batch_count; batch_index++)
        {
            struct l_batch_record_t *record = batch_records + batch_index;
            struct r_batch_t *batch = ((struct r_batch_t *)batch_buffer.buffer) + batch_index;

            batch->start = record->start;
            batch->count = record->count;
            batch->material = material_records[record->material].material;
        }

        for(uint32_t vert_index = 0; vert_index < world_section->vert_count; vert_index++)
        {
            vec3_t *col_vert = ((vec3_t *)col_verts.buffer) + vert_index;
            struct r_vert_t *draw_vert = verts + vert_index;
            *col_vert = draw_vert->pos;
        }

        l_world_shape->itri_mesh.indices = index_buffer.buffer;
        l_world_shape->itri_mesh.index_count = world_section->index_count;
        l_world_shape->itri_mesh.verts = col_verts.buffer;
        l_world_shape->itri_mesh.vert_count = world_section->vert_count;
        l_world_collider = p_CreateCollider(&l_world_col_def, &vec3_t_c(0.0, 0.0, 0.0), &mat3_t_c_id());

        struct r_model_geometry_t geometry = {};
        geometry.batches = batch_buffer.buffer;
        geometry.batch_count = world_section->batch_count;
        geometry.indices = indices;
        geometry.index_count = world_section->index_count;
        geometry.verts = verts;
        geometry.vert_count = world_section->vert_count;
        l_world_model = r_CreateModel(&geometry, NULL, "world_model");

        ds_buffer_destroy(&batch_buffer);
    }
}









