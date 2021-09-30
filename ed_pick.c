#include <stdio.h>

#include "ed_pick.h"
#include "ed_world.h"
#include "ed_brush.h"
#include "game.h"
#include "r_main.h"

extern struct ed_world_context_data_t ed_w_ctx_data;
extern struct r_shader_t *ed_picking_shader;
extern mat4_t r_view_projection_matrix;
extern mat4_t r_camera_matrix;
extern mat4_t r_projection_matrix;
extern int32_t r_width;
extern int32_t r_height;
extern uint32_t r_vertex_buffer;
extern uint32_t r_index_buffer;
extern uint32_t ed_picking_framebuffer;

void ed_BeginPicking(mat4_t *view_projection_matrix)
{
    glBindBuffer(GL_ARRAY_BUFFER, r_vertex_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_index_buffer);

    r_BindShader(ed_picking_shader);

    if(view_projection_matrix)
    {
        r_SetDefaultUniformMat4(R_UNIFORM_VIEW_PROJECTION_MATRIX, view_projection_matrix);
    }
    else
    {
        r_SetDefaultUniformMat4(R_UNIFORM_VIEW_PROJECTION_MATRIX, &r_view_projection_matrix);
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ed_picking_framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);

}

uint32_t ed_EndPicking(int32_t mouse_x, int32_t mouse_y, struct ed_pickable_t *result)
{
    mouse_y = r_height - (mouse_y + 1);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, ed_picking_framebuffer);
    int32_t pickable_index[2];
    glReadPixels(mouse_x, mouse_y, 1, 1, GL_RG_INTEGER, GL_UNSIGNED_INT, pickable_index);

    if(pickable_index[0])
    {
        result->index = pickable_index[0] - 1;
        result->type = pickable_index[1] - 1;
        return 1;
    }

    return 0;
}

void ed_DrawPickable(struct ed_pickable_t *pickable, mat4_t *parent_transform)
{
    mat4_t model_matrix;

    if(parent_transform)
    {
        mat4_t_mul(&model_matrix, &pickable->transform, parent_transform);
    }
    else
    {
        model_matrix = pickable->transform;
    }

    mat4_t_mul(&model_matrix, &pickable->draw_offset, &model_matrix);
    r_SetDefaultUniformMat4(R_UNIFORM_MODEL_MATRIX, &model_matrix);
    r_SetNamedUniformI(r_GetNamedUniform(ed_picking_shader, "ed_index"), pickable->index + 1);
    r_SetNamedUniformI(r_GetNamedUniform(ed_picking_shader, "ed_type"), pickable->type + 1);
    struct ed_pickable_range_t *range = pickable->ranges;

    while(range)
    {
        glDrawElements(pickable->mode, range->count, GL_UNSIGNED_INT, (void *)(sizeof(uint32_t) * range->start));
        range = range->next;
    }
}

struct ed_pickable_t *ed_SelectPickable(int32_t mouse_x, int32_t mouse_y, struct ds_slist_t *pickables, mat4_t *parent_transform, mat4_t *view_projection_matrix)
{
    struct ed_pickable_t *selection = NULL;

    ed_BeginPicking(view_projection_matrix);

    for(uint32_t pickable_index = 0; pickable_index < pickables->cursor; pickable_index++)
    {
        struct ed_pickable_t *pickable = ds_slist_get_element(pickables, pickable_index);

        if(pickable && pickable->index != 0xffffffff)
        {
            ed_DrawPickable(pickable, parent_transform);
        }
    }

    struct ed_pickable_t result;

    if(ed_EndPicking(mouse_x, mouse_y, &result))
    {
        selection = ds_slist_get_element(pickables, result.index);
    }

    return selection;
}

struct ed_pickable_t *ed_SelectWidget(int32_t mouse_x, int32_t mouse_y, struct ed_widget_t *widget, mat4_t *widget_transform)
{
    mat4_t view_projection_matrix;
    widget->compute_model_view_projection_matrix(&view_projection_matrix, widget_transform);
    return ed_SelectPickable(mouse_x, mouse_y, &widget->pickables, NULL, &view_projection_matrix);
}

struct ed_widget_t *ed_CreateWidget()
{
    uint32_t index;
    struct ed_widget_t *widget;

    index = ds_slist_add_element(&ed_w_ctx_data.widgets, NULL);
    widget = ds_slist_get_element(&ed_w_ctx_data.widgets, index);

    widget->index = index;
    widget->compute_model_view_projection_matrix = ed_WidgetDefaultComputeModelViewProjectionMatrix;
    widget->setup_pickable_draw_state = ed_WidgetDefaultSetupPickableDrawState;

    if(!widget->pickables.buffers)
    {
        widget->pickables = ds_slist_create(sizeof(struct ed_pickable_t), 4);
    }

    return widget;
}

void ed_DestroyWidget(struct ed_widget_t *widget)
{

}

void ed_DrawWidget(struct ed_widget_t *widget, mat4_t *widget_transform)
{
    mat4_t view_projection_matrix;
    widget->compute_model_view_projection_matrix(&view_projection_matrix, widget_transform);

    r_i_SetViewProjectionMatrix(&view_projection_matrix);
    r_i_SetDepth(GL_TRUE, GL_ALWAYS);
    r_i_SetStencil(GL_TRUE, GL_KEEP, GL_KEEP, GL_REPLACE, GL_ALWAYS, 0xff, 0x01);

    for(uint32_t pickable_index = 0; pickable_index < widget->pickables.cursor; pickable_index++)
    {
        struct ed_pickable_t *pickable = ed_GetPickableOnList(pickable_index, &widget->pickables);

        if(pickable)
        {
            r_i_SetModelMatrix(&pickable->transform);
            struct r_i_draw_list_t *draw_list = r_i_AllocDrawList(1);
            draw_list->commands[0].start = pickable->ranges->start;
            draw_list->commands[0].count = pickable->ranges->count;
            draw_list->indexed = 1;
            r_i_DrawImmediate(R_I_DRAW_CMD_TRIANGLE_LIST, draw_list);
        }
    }

    r_i_SetDepth(GL_TRUE, GL_LEQUAL);
    r_i_SetStencil(GL_TRUE, GL_KEEP, GL_KEEP, GL_DECR, GL_EQUAL, 0xff, 0x01);

    for(uint32_t pickable_index = 0; pickable_index < widget->pickables.cursor; pickable_index++)
    {
        struct ed_pickable_t *pickable = ed_GetPickableOnList(pickable_index, &widget->pickables);
        widget->setup_pickable_draw_state(pickable_index, pickable);

        if(pickable)
        {
            r_i_SetModelMatrix(&pickable->transform);
            struct r_i_draw_list_t *draw_list = r_i_AllocDrawList(1);
            draw_list->commands[0].start = pickable->ranges->start;
            draw_list->commands[0].count = pickable->ranges->count;
            draw_list->indexed = 1;
            r_i_DrawImmediate(R_I_DRAW_CMD_TRIANGLE_LIST, draw_list);
        }
    }

    r_i_SetDepth(GL_TRUE, GL_LESS);
    r_i_SetStencil(GL_FALSE, GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE);
}

void ed_WidgetDefaultComputeModelViewProjectionMatrix(mat4_t *view_projection_matrix, mat4_t *widget_transform)
{
    mat4_t_identity(view_projection_matrix);
    *view_projection_matrix = r_camera_matrix;

    vec3_t_sub(&view_projection_matrix->rows[3].xyz, &view_projection_matrix->rows[3].xyz, &widget_transform->rows[3].xyz);
    vec3_t_normalize(&view_projection_matrix->rows[3].xyz, &view_projection_matrix->rows[3].xyz);
    vec3_t_mul(&view_projection_matrix->rows[3].xyz, &view_projection_matrix->rows[3].xyz, 50.0);

    mat4_t_invvm(view_projection_matrix, view_projection_matrix);
    mat4_t_mul(view_projection_matrix, view_projection_matrix, &r_projection_matrix);
}

void ed_WidgetDefaultSetupPickableDrawState(uint32_t pickable_index, struct ed_pickable_t *pickable)
{

}

struct ed_pickable_range_t *ed_AllocPickableRange()
{
    struct ed_pickable_range_t *range = NULL;
    uint32_t index = ds_slist_add_element(&ed_w_ctx_data.pickable_ranges, NULL);
    range = ds_slist_get_element(&ed_w_ctx_data.pickable_ranges, index);
    range->index = index;
    return range;
}

void ed_FreePickableRange(struct ed_pickable_range_t *range)
{
    if(range && range->index != 0xffffffff)
    {
        ds_slist_remove_element(&ed_w_ctx_data.pickable_ranges, range->index);
        range->index = 0xffffffff;
    }
}

struct ds_slist_t *ed_PickableListFromType(uint32_t type)
{
    switch(type)
    {
        case ED_PICKABLE_TYPE_BRUSH:
        case ED_PICKABLE_TYPE_LIGHT:
        case ED_PICKABLE_TYPE_ENTITY:
            return &ed_w_ctx_data.pickables.lists[ED_W_CTX_EDIT_MODE_OBJECT].pickables;

        case ED_PICKABLE_TYPE_FACE:
        case ED_PICKABLE_TYPE_EDGE:
            return &ed_w_ctx_data.pickables.lists[ED_W_CTX_EDIT_MODE_BRUSH].pickables;

    }

    return NULL;
}

struct ed_pickable_t *ed_CreatePickableOnList(uint32_t type, struct ds_slist_t *pickables)
{
    struct ed_pickable_t *pickable;
    uint32_t index;

    index = ds_slist_add_element(pickables, NULL);
    pickable = ds_slist_get_element(pickables, index);
    pickable->index = index;
    pickable->list = pickables;
    pickable->type = type;
    pickable->transform_flags = 0;
    pickable->selection_index = 0xffffffff;

    mat4_t_identity(&pickable->draw_offset);

    return pickable;
}

struct ed_pickable_t *ed_CreatePickable(uint32_t type)
{
    struct ds_slist_t *list = ed_PickableListFromType(type);
    return ed_CreatePickableOnList(type, list);
}

void ed_DestroyPickable(struct ed_pickable_t *pickable)
{
    if(pickable && pickable->index != 0xffffffff)
    {
        switch(pickable->type)
        {
            case ED_PICKABLE_TYPE_BRUSH:
            {
                ed_DestroyBrush(ed_GetBrush(pickable->primary_index));

                struct ds_slist_t *pickables = &ed_w_ctx_data.pickables.lists[ED_W_CTX_EDIT_MODE_BRUSH].pickables;
                struct ds_list_t *selections = &ed_w_ctx_data.pickables.lists[ED_W_CTX_EDIT_MODE_BRUSH].selections;

                for(uint32_t pickable_index = 0; pickable_index < pickables->cursor; pickable_index++)
                {
                    struct ed_pickable_t *part_pickable = (struct ed_pickable_t *)ds_slist_get_element(pickables, pickable_index);

                    if(part_pickable && part_pickable->index != 0xffffffff &&
                       part_pickable->primary_index == pickable->primary_index)
                    {
                        if(part_pickable->selection_index != 0xffffffff)
                        {
                            ed_w_DropSelection(part_pickable, selections);
                        }

                        ed_DestroyPickable(part_pickable);
                    }
                }
            }
            break;

            case ED_PICKABLE_TYPE_LIGHT:
                r_DestroyLight(r_GetLight(pickable->primary_index));
            break;

            case ED_PICKABLE_TYPE_ENTITY:
                g_DestroyEntity(g_GetEntity(pickable->primary_index));
            break;
        }

        while(pickable->ranges)
        {
            struct ed_pickable_range_t *next = pickable->ranges->next;
            ed_FreePickableRange(pickable->ranges);
            pickable->ranges = next;
        }

        ds_slist_remove_element(pickable->list, pickable->index);
        pickable->index = 0xffffffff;
    }
}

struct ed_pickable_t *ed_GetPickableOnList(uint32_t index, struct ds_slist_t *pickables)
{
    struct ed_pickable_t *pickable = ds_slist_get_element(pickables, index);

    if(pickable && pickable->index == 0xffffffff)
    {
        pickable = NULL;
    }

    return pickable;
}

struct ed_pickable_t *ed_GetPickable(uint32_t index, uint32_t type)
{
    struct ds_slist_t *list = ed_PickableListFromType(type);
    return ed_GetPickableOnList(index, list);
}

struct ed_pickable_t *ed_CreateBrushPickable(vec3_t *position, mat3_t *orientation, vec3_t *size)
{
    struct ed_pickable_t *pickable = NULL;

    struct ed_brush_t *brush = ed_CreateBrush(position, orientation, size);
    struct r_batch_t *first_batch = (struct r_batch_t *)brush->model->batches.buffer;

    pickable = ed_CreatePickable(ED_PICKABLE_TYPE_BRUSH);
    pickable->mode = GL_TRIANGLES;
    pickable->primary_index = brush->index;
    pickable->range_count = 1;
    pickable->ranges = ed_AllocPickableRange();
    mat4_t_comp(&pickable->transform, &brush->orientation, &brush->position);

    struct ed_face_t *face = brush->faces;

    while(face)
    {
        struct ed_pickable_t *face_pickable = ed_CreatePickable(ED_PICKABLE_TYPE_FACE);
        face_pickable->primary_index = brush->index;
        face_pickable->secondary_index = face->index;
        face_pickable->mode = GL_TRIANGLES;
        face_pickable->range_count = 0;
        mat4_t_comp(&face_pickable->transform, &brush->orientation, &brush->position);

        struct ed_face_polygon_t *polygon = face->polygons;

//        while(polygon)
//        {
//            struct ed_edge_t *edge = polygon->edges;
//
//            while(edge)
//            {
//                struct ed_pickable_t *edge_pickable = ed_CreatePickable(ED_PICKABLE_TYPE_EDGE);
//                edge_pickable->primary_index = brush->index;
//                edge_pickable->secondary_index = edge->index;
//                edge_pickable->mode = GL_LINES;
//                edge_pickable->range_count = 0;
//                edge = edge->next;
//            }
//
//            polygon = polygon->next;
//        }

        face = face->next;
    }

    return pickable;
}

struct ed_pickable_t *ed_CreateLightPickable(vec3_t *pos, vec3_t *color, float radius, float energy)
{
    return NULL;
}

struct ed_pickable_t *ed_CreateEntityPickable(mat4_t *transform, struct r_model_t *model)
{
    return NULL;
}
